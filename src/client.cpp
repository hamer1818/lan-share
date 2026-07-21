// client.cpp — WS tabanli indirme istemcisi cekirdegi.
// CLI `get` ve GUI "Indir" sekmesi ayni fonksiyonlari kullanir:
//   build_tasks()  → /api/list ile gorev listesi (veya tek dosya)
//   do_download()  → worker havuzu, her worker kendi WS baglantisiyla ceker
// Ek: discover() (UDP kesif) ve browse() (tek seviye uzak listeleme).

#include "ws_client.hpp"  // asio — once gelmeli
#include "client.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>
#include <fmt/color.h>
#include <fmt/core.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace client {

namespace {

struct Target { std::string host; uint16_t port; };

Target parse_target(std::string s) {
    auto scheme = s.find("://");
    if (scheme != std::string::npos) s = s.substr(scheme + 3);
    auto slash = s.find('/');
    if (slash != std::string::npos) s = s.substr(0, slash);
    Target t{"127.0.0.1", 8000};
    auto colon = s.rfind(':');
    if (colon != std::string::npos) {
        t.host = s.substr(0, colon);
        int p = std::atoi(s.c_str() + colon + 1);
        if (p > 0 && p < 65536) t.port = static_cast<uint16_t>(p);
    } else {
        t.host = s;
    }
    if (t.host.empty()) t.host = "127.0.0.1";
    return t;
}

std::string clean_remote(std::string r) {
    for (auto& c : r) if (c == '\\') c = '/';
    while (!r.empty() && r.front() == '/') r.erase(0, 1);
    while (!r.empty() && r.back() == '/')  r.pop_back();
    return r;
}

struct Task {
    std::string remote;   // sunucu yolu
    fs::path    local;    // yerel hedef
    uint64_t    size = 0;
};

// Sunucudan gorev listesi cikarir. Klasorse /api/list kullanir (is_folder=true,
// outbase = outdir/kok-adi); degilse tek dosyalik gorev uretir.
bool build_tasks(const Target& t, const std::string& remote_clean,
                 const fs::path& outdir,
                 std::vector<Task>& tasks, fs::path& outbase,
                 uint64_t& total_bytes, bool& is_folder, std::string& root_name,
                 std::string& err)
{
    tasks.clear(); total_bytes = 0; is_folder = false;

    int status = 0; std::string body;
    std::string query = "/api/list?dir=" + util::url_encode_path("/" + remote_clean);
    try {
        wsc::http_get(t.host, t.port, query, status, body);
    } catch (const std::exception& e) {
        err = fmt::format("sunucuya baglanilamadi: {}", e.what());
        return false;
    }

    auto j = (status == 200) ? json::parse(body, nullptr, false) : json{};
    is_folder = (status == 200) && j.is_object() && j.value("ok", false);

    if (is_folder) {
        root_name = j.value("name", std::string("indirme"));
        if (root_name.empty()) root_name = "indirme";
        outbase = outdir / util::utf8_to_path(root_name);
        for (auto& f : j.value("files", json::array())) {
            std::string rp = f.value("path", std::string{});
            if (rp.empty()) continue;
            Task task;
            task.remote = remote_clean.empty() ? rp : remote_clean + "/" + rp;
            task.local  = outbase / util::utf8_to_path(rp);
            if (!util::is_under(outbase, task.local)) continue;  // guvenlik
            task.size = f.value("size", uint64_t{0});
            total_bytes += task.size;
            tasks.push_back(std::move(task));
        }
    } else {
        std::string fname = remote_clean;
        auto slash = fname.find_last_of('/');
        if (slash != std::string::npos) fname = fname.substr(slash + 1);
        if (fname.empty()) fname = "indirme";
        root_name = fname;
        outbase = outdir;
        Task task;
        task.remote = remote_clean;
        task.local  = outbase / util::utf8_to_path(fname);
        tasks.push_back(std::move(task));
    }

    if (tasks.empty()) { err = "indirilecek dosya yok"; return false; }
    return true;
}

// Tek bir dosyayi acik bir WS baglantisi uzerinden indirir.
void download_on(wsc::WebSocketClient& ws, const Task& task,
                 std::atomic<uint64_t>& bytes, std::atomic<bool>& cancel) {
    ws.send_text(json{{"path", task.remote}}.dump());

    std::string msg;
    uint8_t op = ws.recv(msg);
    if (op != wsc::OP_TEXT)
        throw std::runtime_error("beklenmeyen yanit (meta degil)");
    auto meta = json::parse(msg, nullptr, false);
    if (meta.is_discarded() || !meta.value("ok", false))
        throw std::runtime_error(
            meta.is_object() ? meta.value("error", std::string("sunucu hatasi"))
                             : std::string("gecersiz meta"));
    uint64_t size = meta.value("size", uint64_t{0});

    std::error_code ec;
    fs::create_directories(task.local.parent_path(), ec);
    std::ofstream f(task.local, std::ios::binary);
    if (!f)
        throw std::runtime_error("yazilamiyor: " + util::path_to_utf8(task.local));

    uint64_t written = 0;
    for (;;) {
        if (cancel.load()) throw std::runtime_error("iptal edildi");
        op = ws.recv(msg);
        if (op == wsc::OP_BIN) {
            f.write(msg.data(), static_cast<std::streamsize>(msg.size()));
            written += msg.size();
            bytes += msg.size();
            ws.send_text(R"({"ack":true})");
        } else if (op == wsc::OP_TEXT) {
            auto m = json::parse(msg, nullptr, false);
            if (!m.is_discarded()) {
                if (m.contains("done")) break;
                if (m.value("ok", true) == false)
                    throw std::runtime_error(m.value("error", std::string("hata")));
            }
        } else {  // OP_CLOSE
            break;
        }
    }
    f.close();
    if (size > 0 && written != size)
        throw std::runtime_error(fmt::format("eksik indirme: {}/{}", written, size));
}

// Worker havuzu ile tum gorevleri indirir; hatalarin listesini dondurur.
// Her worker bir WS baglantisini gorevler arasinda yeniden kullanir.
std::vector<std::string> do_download(const Target& t, const std::vector<Task>& tasks,
                                     int parallel,
                                     std::atomic<uint64_t>& bytes,
                                     std::atomic<uint64_t>& files,
                                     std::atomic<bool>& cancel)
{
    std::mutex               err_mtx;
    std::vector<std::string> errors;
    std::atomic<size_t>      idx{0};

    auto work = [&] {
        std::unique_ptr<wsc::WebSocketClient> ws;
        for (;;) {
            if (cancel.load()) break;
            size_t i = idx.fetch_add(1);
            if (i >= tasks.size()) break;
            const Task& task = tasks[i];
            try {
                if (!ws) {
                    ws = std::make_unique<wsc::WebSocketClient>();
                    ws->connect(t.host, t.port, "/ws/download");
                }
                download_on(*ws, task, bytes, cancel);
                files.fetch_add(1);
            } catch (const std::exception& e) {
                std::error_code ec; fs::remove(task.local, ec);  // yarim dosyayi sil
                if (ws) { try { ws->close(); } catch (...) {} ws.reset(); }
                if (cancel.load()) break;
                std::lock_guard lk(err_mtx);
                errors.push_back(fmt::format("{} — {}", task.remote, e.what()));
            }
        }
        if (ws) { try { ws->close(); } catch (...) {} }
    };

    int n = std::max(1, std::min<int>(parallel, static_cast<int>(tasks.size())));
    std::vector<std::thread> workers;
    workers.reserve(n);
    for (int i = 0; i < n; ++i) workers.emplace_back(work);
    for (auto& w : workers) w.join();
    return errors;
}

} // namespace

// ─── LAN kesfi ─────────────────────────────────────

std::vector<Device> discover(int timeout_ms) {
    std::vector<Device> out;
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return out;

    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST,
               reinterpret_cast<const char*>(&yes), sizeof(yes));
    DWORD tmo = 200;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tmo), sizeof(tmo));

    auto probe = [&](uint32_t addr_be) {
        sockaddr_in a{};
        a.sin_family      = AF_INET;
        a.sin_port        = htons(util::DISCOVERY_PORT);
        a.sin_addr.s_addr = addr_be;
        sendto(s, util::DISCOVERY_MAGIC,
               static_cast<int>(sizeof(util::DISCOVERY_MAGIC) - 1), 0,
               reinterpret_cast<sockaddr*>(&a), sizeof(a));
    };
    probe(INADDR_BROADCAST);
    {   // ayni makinedeki sunucu icin (test/localhost)
        in_addr lo{};
        inet_pton(AF_INET, "127.0.0.1", &lo);
        probe(lo.s_addr);
    }

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        char buf[512];
        sockaddr_in from{};
        int fl = sizeof(from);
        int n = recvfrom(s, buf, sizeof(buf) - 1, 0,
                         reinterpret_cast<sockaddr*>(&from), &fl);
        if (n <= 0) continue;
        buf[n] = '\0';
        auto j = json::parse(buf, nullptr, false);
        if (j.is_discarded() || !j.value("lan_share", false)) continue;

        char ipstr[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &from.sin_addr, ipstr, sizeof(ipstr));
        Device d{
            j.value("name", std::string{}),
            ipstr,
            static_cast<uint16_t>(j.value("port", 8000)),
            j.value("share", std::string{}),
        };
        bool dup = false;
        for (const auto& e : out)
            if (e.host == d.host && e.port == d.port) { dup = true; break; }
        if (!dup) out.push_back(std::move(d));
    }
    closesocket(s);
    return out;
}

// ─── Uzak listeleme (GUI gezgini) ──────────────────

bool browse(const std::string& server, const std::string& dir,
            std::vector<RemoteEntry>& out, std::string& err) {
    Target t = parse_target(server);
    try {
        int status = 0; std::string body;
        std::string q = "/api/browse?dir="
                      + util::url_encode_path(dir.empty() ? "/" : "/" + dir);
        wsc::http_get(t.host, t.port, q, status, body);
        if (status != 200) { err = fmt::format("HTTP {}", status); return false; }
        auto j = json::parse(body, nullptr, false);
        if (j.is_discarded() || !j.value("ok", false)) {
            err = "gecersiz yanit"; return false;
        }
        out.clear();
        for (auto& e : j.value("entries", json::array()))
            out.push_back({ e.value("name", std::string{}),
                            e.value("dir", false),
                            e.value("size", uint64_t{0}) });
        std::sort(out.begin(), out.end(),
            [](const RemoteEntry& a, const RemoteEntry& b) {
                if (a.is_dir != b.is_dir) return a.is_dir;
                return a.name < b.name;
            });
        return true;
    } catch (const std::exception& ex) {
        err = ex.what();
        return false;
    }
}

// ─── GUI arkaplan indirme ──────────────────────────

Downloader::~Downloader() {
    cancel_.store(true);
    if (th_.joinable()) th_.join();
}

bool Downloader::start(const std::string& server, const std::string& remote,
                       const std::string& outdir, int parallel,
                       uint64_t total_hint) {
    std::lock_guard lk(mtx_);
    if (active_.load()) return false;
    if (th_.joinable()) th_.join();
    cancel_   = false;
    finished_ = false;
    failed_   = false;
    bytes_ = 0; files_ = 0; total_bytes_ = 0; total_files_ = 0;
    final_secs_ = 0.0;
    message_.clear();
    t0_ = std::chrono::steady_clock::now();
    active_ = true;
    th_ = std::thread(&Downloader::run, this, server, remote, outdir,
                      parallel, total_hint);
    return true;
}

Downloader::Snapshot Downloader::snapshot() {
    Snapshot s;
    s.active      = active_.load();
    s.finished    = finished_.load();
    s.failed      = failed_.load();
    s.bytes       = bytes_.load();
    s.total_bytes = total_bytes_.load();
    s.files       = files_.load();
    s.total_files = total_files_.load();
    s.seconds = s.active
        ? std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count()
        : final_secs_.load();
    { std::lock_guard lk(mtx_); s.message = message_; }
    return s;
}

void Downloader::run(std::string server, std::string remote, std::string outdir,
                     int parallel, uint64_t total_hint) {
    Target t = parse_target(server);
    std::string remote_clean = clean_remote(std::move(remote));

    std::vector<Task> tasks;
    fs::path outbase;
    uint64_t total = 0;
    bool is_folder = false;
    std::string root, err;

    if (!build_tasks(t, remote_clean, util::utf8_to_path(outdir),
                     tasks, outbase, total, is_folder, root, err)) {
        { std::lock_guard lk(mtx_); message_ = err; }
        failed_ = true; finished_ = true; active_ = false;
        return;
    }
    if (!is_folder && total == 0 && total_hint > 0) {
        total = total_hint;
        tasks[0].size = total_hint;
    }
    total_bytes_ = total;
    total_files_ = tasks.size();

    std::error_code ec;
    fs::create_directories(is_folder ? outbase : util::utf8_to_path(outdir), ec);

    auto errors = do_download(t, tasks, parallel, bytes_, files_, cancel_);

    double secs = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0_).count();
    final_secs_ = secs;
    {
        std::lock_guard lk(mtx_);
        if (cancel_.load()) {
            message_ = "Iptal edildi.";
            failed_ = true;
        } else if (errors.empty()) {
            message_ = fmt::format("Tamamlandi — {} dosya, {} ({}/s)",
                files_.load(), util::human_size(bytes_.load()),
                util::human_size(static_cast<uint64_t>(
                    bytes_.load() / std::max(secs, 0.001))));
        } else {
            message_ = fmt::format("{} hata — ilk: {}", errors.size(), errors.front());
            failed_ = true;
        }
    }
    finished_ = true;
    active_ = false;
}

// ─── CLI `get` ─────────────────────────────────────

namespace {

void monitor_loop(std::atomic<uint64_t>& bytes, std::atomic<uint64_t>& files,
                  uint64_t total_bytes, size_t total_files,
                  std::chrono::steady_clock::time_point t0,
                  std::atomic<bool>& running) {
    using namespace std::chrono;
    while (running.load()) {
        std::this_thread::sleep_for(milliseconds(300));
        uint64_t b = bytes.load();
        uint64_t fdone = files.load();
        double el = duration<double>(steady_clock::now() - t0).count();
        double sp = b / std::max(el, 0.001);
        std::string line;
        if (total_bytes > 0) {
            double pct = std::min(100.0, 100.0 * b / total_bytes);
            double rem = sp > 0 ? (total_bytes - b) / sp : 0;
            line = fmt::format("  {:>3.0f}%  {} / {}  |  {}/s  |  {}/{} dosya  |  kalan {:.0f}s   ",
                pct, util::human_size(b), util::human_size(total_bytes),
                util::human_size(static_cast<uint64_t>(sp)),
                fdone, total_files, rem);
        } else {
            line = fmt::format("  {}  |  {}/s  |  {}/{} dosya   ",
                util::human_size(b), util::human_size(static_cast<uint64_t>(sp)),
                fdone, total_files);
        }
        fmt::print("\r{}", line);
        std::fflush(stdout);
    }
}

} // namespace

int run_get(const std::string& server, const std::string& remote,
            const std::string& outdir, int parallel) {
    using namespace fmt;
    Target t = parse_target(server);
    std::string remote_clean = clean_remote(remote);

    print(fg(color::medium_purple) | emphasis::bold,
          "\n  lan_share indirme istemcisi\n");
    print(fg(color::gray), "  Sunucu : "); print("http://{}:{}\n", t.host, t.port);
    print(fg(color::gray), "  Yol    : "); print("/{}\n", remote_clean);
    print(fg(color::gray), "  Hedef  : "); print("{}\n\n", outdir);

    std::vector<Task> tasks;
    fs::path outbase;
    uint64_t total_bytes = 0;
    bool is_folder = false;
    std::string root, err;

    if (!build_tasks(t, remote_clean, util::utf8_to_path(outdir),
                     tasks, outbase, total_bytes, is_folder, root, err)) {
        print(fg(color::tomato) | emphasis::bold, "  HATA: {}\n", err);
        return 1;
    }

    if (is_folder) {
        print(fg(color::cyan), "  Klasor: "); print("{} — {} dosya, {}\n\n",
              root, tasks.size(), util::human_size(total_bytes));
    } else {
        print(fg(color::cyan), "  Dosya: "); print("{}\n\n", root);
    }

    std::error_code ec;
    fs::create_directories(is_folder ? outbase : util::utf8_to_path(outdir), ec);

    std::atomic<uint64_t> bytes{0}, files{0};
    std::atomic<bool>     cancel{false};
    std::atomic<bool>     running{true};
    auto t0 = std::chrono::steady_clock::now();

    std::thread mon(monitor_loop, std::ref(bytes), std::ref(files),
                    total_bytes, tasks.size(), t0, std::ref(running));
    auto errors = do_download(t, tasks, parallel, bytes, files, cancel);
    running = false;
    mon.join();

    double el = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    uint64_t b = bytes.load();
    double sp = b / std::max(el, 0.001);

    fmt::print("\r{:80}\r", "");  // ilerleme satirini temizle
    print(fg(color::lime_green) | emphasis::bold, "  Tamamlandi: ");
    print("{} dosya, {}, {:.1f}s, ort {}/s\n",
          files.load(), util::human_size(b), el,
          util::human_size(static_cast<uint64_t>(sp)));

    if (!errors.empty()) {
        print(fg(color::tomato) | emphasis::bold, "  {} hata:\n", errors.size());
        for (const auto& e : errors) print(fg(color::tomato), "    - {}\n", e);
        return static_cast<int>(errors.size());
    }
    return 0;
}

} // namespace client
