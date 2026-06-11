// server.cpp — HTTP + WebSocket sunucusu.

#define CROW_MAIN
#define CROW_DISABLE_STATIC_DIR
#include <crow.h>

#include <fmt/color.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>

#include "server.hpp"
#include "util.hpp"
#include "page_template.h"

namespace fs = std::filesystem;

namespace server {

// ──────────────────────────────────────────────
//  Log sink
// ──────────────────────────────────────────────

namespace {

std::mutex     g_log_mtx;
LogSink        g_log_sink;

void default_cli_sink(LogLevel lvl, std::string_view msg) {
    using namespace fmt;
    text_style style;
    const char* prefix = "  ";
    switch (lvl) {
        case LogLevel::Up:   style = fg(color::lime_green);  prefix = "  << "; break;
        case LogLevel::Down: style = fg(color::cyan);         prefix = "  >> "; break;
        case LogLevel::Ws:   style = fg(color::dodger_blue);  prefix = "  WS "; break;
        case LogLevel::Ok:   style = fg(color::lime_green);   break;
        case LogLevel::Warn: style = fg(color::gold);         break;
        case LogLevel::Err:  style = fg(color::tomato);       prefix = "  !! "; break;
        case LogLevel::Info: style = fg(color::gray);         break;
    }
    fmt::print(style, "{}{}\n", prefix, msg);
    std::fflush(stdout);
}

} // namespace

void set_log_sink(LogSink sink) {
    std::lock_guard lk(g_log_mtx);
    g_log_sink = std::move(sink);
}

void log(LogLevel lvl, std::string_view text) {
    LogSink sink;
    {
        std::lock_guard lk(g_log_mtx);
        sink = g_log_sink ? g_log_sink : LogSink(default_cli_sink);
    }
    sink(lvl, text);
}

// ──────────────────────────────────────────────
//  State
// ──────────────────────────────────────────────

namespace {

Config                          g_cfg;
std::unique_ptr<crow::SimpleApp> g_app;
std::thread                     g_thread;
std::atomic<bool>               g_running{false};

constexpr std::string_view FAVICON_SVG_SV = FAVICON_SVG;
constexpr std::string_view PAGE_TEMPLATE_SV = PAGE_TEMPLATE;

struct WSState {
    std::string  dir_param;
    std::ofstream f;
    fs::path     dest;
    uint64_t     expected = 0;
    uint64_t     written  = 0;
    std::chrono::steady_clock::time_point t0;
    int          files_uploaded = 0;

    void reset_active() {
        if (f.is_open()) f.close();
        dest.clear();
        expected = 0;
        written  = 0;
    }
};

// ──────────────────────────────────────────────
//  GET handlers
// ──────────────────────────────────────────────

crow::response serve_favicon() {
    crow::response res(200);
    res.add_header("Content-Type",  "image/svg+xml");
    res.add_header("Cache-Control", "public, max-age=86400");
    res.write(std::string(FAVICON_SVG_SV));
    return res;
}

crow::response serve_directory(const fs::path& path) {
    fs::path rel = fs::relative(path, g_cfg.share_dir);
    std::string rel_str = util::path_to_utf8(rel);
    for (auto& c : rel_str) if (c == '\\') c = '/';
    if (rel_str == "." || rel_str.empty()) rel_str = "/";
    else                                    rel_str = "/" + rel_str;

    std::string entries_html;

    if (path != g_cfg.share_dir) {
        std::string parent = util::path_to_utf8(rel.parent_path());
        for (auto& c : parent) if (c == '\\') c = '/';
        if (parent == "." || parent.empty()) parent = "";
        std::string parent_url = "/" + parent;
        entries_html += fmt::format(
            "    <li>"
            "<span class=\"ico\">&#11014;&#65039;</span>"
            "<span class=\"nm\"><a class=\"dl\" href=\"{}\">.. (ust klasor)</a></span>"
            "</li>\n",
            util::url_encode_path(parent_url));
    }

    std::vector<fs::directory_entry> items;
    std::error_code ec;
    for (auto it = fs::directory_iterator(path, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec))
    {
        items.push_back(*it);
    }
    std::sort(items.begin(), items.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            bool ad = a.is_directory(), bd = b.is_directory();
            if (ad != bd) return ad;
            auto an = util::path_to_utf8(a.path().filename());
            auto bn = util::path_to_utf8(b.path().filename());
            std::transform(an.begin(), an.end(), an.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(bn.begin(), bn.end(), bn.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return an < bn;
        });

    int idx = 0;
    for (const auto& item : items) {
        std::string name_utf8 = util::path_to_utf8(item.path().filename());
        std::string name_esc  = util::html_escape(name_utf8);

        fs::path item_rel = fs::relative(item.path(), g_cfg.share_dir);
        std::string url = util::path_to_utf8(item_rel);
        for (auto& c : url) if (c == '\\') c = '/';
        url = "/" + util::url_encode_path(url);

        int delay = std::min(idx * 30, 600);
        std::string ds = fmt::format(" style=\"animation-delay:{}ms\"", delay);

        if (item.is_directory()) {
            entries_html += fmt::format(
                "    <li{}>"
                "<span class=\"ico\">&#128194;</span>"
                "<span class=\"nm\"><a class=\"dl\" href=\"{}\">{}/</a></span>"
                "<span class=\"mt\">Klasor</span>"
                "</li>\n", ds, url, name_esc);
        } else {
            std::string size_s = "?";
            std::string mtime_s;
            std::error_code ec2;
            auto sz = item.file_size(ec2);
            if (!ec2) size_s = util::human_size(sz);
            auto t  = item.last_write_time(ec2);
            if (!ec2) mtime_s = util::format_local_time(t);
            entries_html += fmt::format(
                "    <li{}>"
                "<span class=\"ico\">&#128196;</span>"
                "<span class=\"nm\"><a href=\"{}\">{}</a></span>"
                "<span class=\"mt\">{}<br>{}</span>"
                "</li>\n", ds, url, name_esc, size_s, mtime_s);
        }
        ++idx;
    }

    if (items.empty()) {
        entries_html +=
            "    <li class=\"empty\">"
            "<span class=\"empty-ico\">&#128237;</span>"
            "Bu klasor bos"
            "</li>\n";
    }

    std::string upload_url = rel_str;
    while (upload_url.size() > 1 && upload_url.back() == '/') upload_url.pop_back();
    if (upload_url.empty()) upload_url = "/";

    std::string title_raw = util::path_to_utf8(path.filename());
    if (title_raw.empty()) title_raw = util::path_to_utf8(g_cfg.share_dir);

    std::string body = util::render_template(PAGE_TEMPLATE_SV, {
        {"title",        util::html_escape(title_raw)},
        {"rel_path",     util::html_escape(rel_str)},
        {"entries",      entries_html},
        {"upload_url",   upload_url},
        {"server_info",  util::html_escape(g_cfg.server_info)},
        {"max_parallel", std::to_string(g_cfg.parallel)},
    });

    crow::response res(200);
    res.add_header("Content-Type", "text/html; charset=utf-8");
    res.write(std::move(body));
    return res;
}

crow::response serve_file(const fs::path& path, const crow::request& req) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (ec) {
        crow::response r(500);
        r.write("Internal Server Error");
        return r;
    }

    std::string fname    = util::path_to_utf8(path.filename());
    std::string disp_val = fmt::format("attachment; filename=\"{}\"",
                                       util::url_encode_path(fname));

    auto range_h = req.get_header_value("Range");
    auto range   = range_h.empty() ? std::nullopt
                                   : util::parse_range(range_h, size);

    if (!range) {
        // Tam dosya — Crow'un kendi streamer'i Python'a gore daha kucuk
        // chunk + Nagle ile yavas. Orta boy dosyalar icin (< 512 MiB) RAM'e al,
        // cok buyukler icin Crow streamer'ina devret (RAM guvenligi).
        constexpr uint64_t INLINE_LIMIT = 512ull * 1024 * 1024;
        if (size <= INLINE_LIMIT) {
            std::string body;
            body.resize(static_cast<size_t>(size));
            std::ifstream f(path, std::ios::binary);
            if (!f) {
                crow::response r(500); r.write("Internal Server Error"); return r;
            }
            f.read(body.data(), static_cast<std::streamsize>(size));
            crow::response res(200);
            res.add_header("Content-Type",        "application/octet-stream");
            res.add_header("Content-Disposition", disp_val);
            res.add_header("Accept-Ranges",       "bytes");
            res.body = std::move(body);
            logf(LogLevel::Down, "{} ({})", fname, util::human_size(size));
            return res;
        }
        crow::response res;
        res.set_static_file_info_unsafe(util::path_to_utf8(path));
        res.add_header("Content-Disposition", disp_val);
        res.add_header("Accept-Ranges",       "bytes");
        logf(LogLevel::Down, "{} ({})", fname, util::human_size(size));
        return res;
    }

    uint64_t content_len = range->end - range->start + 1;
    std::string body;
    body.resize(static_cast<size_t>(content_len));
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        crow::response r(500);
        r.write("Internal Server Error");
        return r;
    }
    f.seekg(static_cast<std::streamoff>(range->start));
    f.read(body.data(), static_cast<std::streamsize>(content_len));

    crow::response res(206);
    res.add_header("Content-Type",        "application/octet-stream");
    res.add_header("Content-Range",
        fmt::format("bytes {}-{}/{}", range->start, range->end, size));
    res.add_header("Accept-Ranges",       "bytes");
    res.add_header("Content-Disposition", disp_val);
    res.body = std::move(body);

    logf(LogLevel::Down, "{} (range {}-{}/{}, {})",
         fname, range->start, range->end, size, util::human_size(content_len));
    return res;
}

crow::response dispatch_path(const crow::request& req, std::string raw_path) {
    if (raw_path.empty() || raw_path == "/") return serve_directory(g_cfg.share_dir);
    if (raw_path == "/favicon.ico")          return serve_favicon();

    std::string_view sv = raw_path;
    while (!sv.empty() && sv.front() == '/') sv.remove_prefix(1);

    auto resolved = util::safe_join_under(g_cfg.share_dir, sv);
    if (!resolved) {
        crow::response res(403); res.write("Forbidden"); return res;
    }

    std::error_code ec;
    auto stat = fs::status(*resolved, ec);
    if (ec || !fs::exists(stat)) {
        crow::response res(404); res.write("Not Found"); return res;
    }
    if (fs::is_directory(stat))    return serve_directory(*resolved);
    if (fs::is_regular_file(stat)) return serve_file(*resolved, req);
    crow::response res(404); res.write("Not Found"); return res;
}

// ──────────────────────────────────────────────
//  Routes
// ──────────────────────────────────────────────

void setup_routes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/ping")([] {
        nlohmann::json j{{"ok", true}, {"server", "lan_share/4.0 (C++)"}};
        crow::response r{j.dump()};
        r.add_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/upload").methods("POST"_method)
    ([](const crow::request& req) {
        auto rel_path  = req.url_params.get("path");
        auto dir_param = req.url_params.get("dir");
        auto name_p    = req.url_params.get("name");

        std::string rp = rel_path ? rel_path : "upload";
        std::string dp = dir_param ? dir_param : "/";
        if (rp == "upload" && name_p) rp = name_p;

        auto [target_dir, filename] = util::resolve_upload_path(
            g_cfg.share_dir, dp, rp);
        auto dest = util::safe_dest(target_dir, filename);

        auto t0 = std::chrono::steady_clock::now();
        std::ofstream f(dest, std::ios::binary);
        if (!f) {
            crow::response r(500); r.write("Upload Error: cannot open dest"); return r;
        }
        f.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
        f.close();
        if (!f.good()) {
            std::error_code ec; fs::remove(dest, ec);
            crow::response r(500); r.write("Upload Error: write failed"); return r;
        }

        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        double speed = req.body.size() / std::max(elapsed, 0.001);

        fs::path show_path = dest;
        std::error_code ec;
        auto rel = fs::relative(dest, g_cfg.share_dir, ec);
        if (!ec) show_path = rel;

        logf(LogLevel::Up, "{} ({}) {}/s",
             util::path_to_utf8(show_path),
             util::human_size(req.body.size()),
             util::human_size(static_cast<uint64_t>(speed)));

        nlohmann::json j{
            {"ok",   true},
            {"name", util::path_to_utf8(dest.filename())},
            {"size", req.body.size()},
        };
        crow::response r{j.dump()};
        r.add_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/check_existing").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            crow::response r(400); r.write("Invalid JSON"); return r;
        }
        std::string dir_param = body.value("dir", "/");
        auto files = body.value("files", nlohmann::json::array());
        nlohmann::json existing = nlohmann::json::array();
        for (auto& f : files) {
            std::string rel = f.value("path", std::string{});
            int64_t size    = f.value("size", int64_t{-1});
            if (rel.empty() || size < 0) continue;
            auto [td, fn] = util::resolve_upload_path(g_cfg.share_dir, dir_param, rel);
            fs::path dest = td / util::utf8_to_path(fn);
            std::error_code ec;
            if (fs::exists(dest, ec)
                && fs::is_regular_file(dest, ec)
                && static_cast<int64_t>(fs::file_size(dest, ec)) == size)
            {
                existing.push_back(rel);
            }
        }
        nlohmann::json j{{"existing", existing}};
        crow::response r{j.dump()};
        r.add_header("Content-Type", "application/json");
        return r;
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws/upload")
        .onaccept([](const crow::request& req, void** userdata) {
            auto* s = new WSState{};
            auto dp = req.url_params.get("dir");
            s->dir_param = dp ? dp : "/";
            *userdata = s;
            return true;
        })
        .onopen([](crow::websocket::connection& conn) {
            logf(LogLevel::Ws, "open ({})", conn.get_remote_ip());
        })
        .onmessage([](crow::websocket::connection& conn,
                      const std::string& data, bool is_binary)
        {
            auto* s = static_cast<WSState*>(conn.userdata());
            if (!s) return;

            if (!is_binary) {
                auto j = nlohmann::json::parse(data, nullptr, false);
                if (j.is_discarded()) {
                    conn.send_text(R"({"ok":false,"error":"Invalid JSON"})");
                    return;
                }
                std::string rel = j.value("path", std::string{"upload"});
                uint64_t sz     = j.value("size", uint64_t{0});

                auto [td, fn] = util::resolve_upload_path(
                    g_cfg.share_dir, s->dir_param, rel);
                s->dest     = util::safe_dest(td, fn);
                s->expected = sz;
                s->written  = 0;
                s->t0       = std::chrono::steady_clock::now();
                s->f.open(s->dest, std::ios::binary);
                if (!s->f.is_open()) {
                    conn.send_text(R"({"ok":false,"error":"Cannot open file"})");
                    return;
                }
                if (sz == 0) {
                    s->f.close();
                    nlohmann::json ack{
                        {"ok", true},
                        {"name", util::path_to_utf8(s->dest.filename())},
                        {"written", uint64_t{0}}
                    };
                    conn.send_text(ack.dump());
                    s->files_uploaded++;
                    s->reset_active();
                }
                return;
            }

            if (!s->f.is_open()) return;
            s->f.write(data.data(), static_cast<std::streamsize>(data.size()));
            s->written += data.size();

            if (s->written >= s->expected) {
                s->f.close();
                if (!s->f.good() || s->written != s->expected) {
                    std::error_code ec; fs::remove(s->dest, ec);
                    conn.send_text(fmt::format(
                        R"({{"ok":false,"error":"Yazma hatasi: {}/{}"}})",
                        s->written, s->expected));
                    s->reset_active();
                    return;
                }
                auto elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - s->t0).count();
                double speed = s->written / std::max(elapsed, 0.001);

                fs::path show = s->dest;
                std::error_code ec;
                auto rel_p = fs::relative(s->dest, g_cfg.share_dir, ec);
                if (!ec) show = rel_p;

                logf(LogLevel::Up, "{} ({}) {}/s [WS]",
                     util::path_to_utf8(show),
                     util::human_size(s->written),
                     util::human_size(static_cast<uint64_t>(speed)));

                nlohmann::json ack{
                    {"ok", true},
                    {"name", util::path_to_utf8(s->dest.filename())},
                    {"written", s->written}
                };
                conn.send_text(ack.dump());
                s->files_uploaded++;
                s->reset_active();
            }
        })
        .onclose([](crow::websocket::connection& conn,
                    const std::string&, uint16_t)
        {
            auto* s = static_cast<WSState*>(conn.userdata());
            if (!s) return;
            if (s->f.is_open()) {
                s->f.close();
                if (s->written < s->expected) {
                    std::error_code ec; fs::remove(s->dest, ec);
                }
            }
            logf(LogLevel::Ws, "close ({}) — {} dosya",
                 conn.get_remote_ip(), s->files_uploaded);
            delete s;
        });

    CROW_CATCHALL_ROUTE(app)
    ([](const crow::request& req) {
        const std::string& p = req.url;
        std::string decoded;
        decoded.reserve(p.size());
        for (size_t i = 0; i < p.size(); ++i) {
            if (p[i] == '%' && i + 2 < p.size()) {
                auto hex = p.substr(i + 1, 2);
                try {
                    decoded += static_cast<char>(std::stoi(hex, nullptr, 16));
                    i += 2;
                } catch (...) { decoded += p[i]; }
            } else if (p[i] == '+') decoded += ' ';
            else                    decoded += p[i];
        }
        return dispatch_path(req, decoded);
    });
}

} // anonymous namespace

// ──────────────────────────────────────────────
//  Lifecycle
// ──────────────────────────────────────────────

bool is_running() { return g_running.load(); }

bool start(const Config& cfg) {
    if (g_running.exchange(true)) return false;
    g_cfg = cfg;
    g_app = std::make_unique<crow::SimpleApp>();
    g_app->loglevel(crow::LogLevel::Warning);
    g_app->stream_threshold(512ull * 1024 * 1024);  // 512 MiB
    setup_routes(*g_app);

    auto threads = std::max(2u, std::thread::hardware_concurrency());
    g_app->bindaddr("0.0.0.0")
         .port(static_cast<uint16_t>(cfg.port))
         .concurrency(threads);

    g_thread = std::thread([] {
        try {
            g_app->multithreaded().run();
        } catch (const std::exception& e) {
            logf(LogLevel::Err, "server: {}", e.what());
        }
        g_running = false;
    });
    return true;
}

void stop() {
    if (!g_running.load() && !g_thread.joinable()) return;
    if (g_app) g_app->stop();
    if (g_thread.joinable()) g_thread.join();
    g_app.reset();
    g_running = false;
}

void run_blocking(const Config& cfg) {
    g_cfg = cfg;
    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);
    app.stream_threshold(512ull * 1024 * 1024);  // 512 MiB
    setup_routes(app);
    auto threads = std::max(2u, std::thread::hardware_concurrency());
    app.bindaddr("0.0.0.0")
       .port(static_cast<uint16_t>(cfg.port))
       .concurrency(threads)
       .multithreaded()
       .run();
}

} // namespace server
