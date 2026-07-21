// client.hpp — indirme istemcisi: LAN kesfi, uzak listeleme ve WS indirme.
// CLI (`get` alt komutu) ve GUI ("Indir" sekmesi) ayni cekirdegi kullanir.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace client {

// Agda bulunan bir lan_share sunucusu.
struct Device {
    std::string name;         // bilgisayar adi
    std::string host;         // IPv4
    uint16_t    port = 8000;
    std::string share;        // paylasilan klasorun adi
};

// Uzak klasordeki tek bir girdi (GUI gezgini icin).
struct RemoteEntry {
    std::string name;
    bool        is_dir = false;
    uint64_t    size   = 0;
};

// UDP broadcast ile agdaki lan_share sunucularini bulur.
std::vector<Device> discover(int timeout_ms = 900);

// Uzak klasorun TEK seviyesini listeler. dir: "" (kok) veya "alt/klasor".
// server: "host", "host:port" veya URL. Hatada false doner, err doldurulur.
bool browse(const std::string& server, const std::string& dir,
            std::vector<RemoteEntry>& out, std::string& err);

// CLI `get` alt komutu — konsola ilerleme basar. 0 = basari.
int run_get(const std::string& server, const std::string& remote,
            const std::string& outdir, int parallel);

// GUI icin arkaplan indirme yoneticisi. Ayni anda tek indirme yurutur.
class Downloader {
public:
    struct Snapshot {
        bool        active   = false;  // indirme suruyor
        bool        finished = false;  // bitti (basari, hata veya iptal)
        bool        failed   = false;
        uint64_t    bytes = 0, total_bytes = 0;
        uint64_t    files = 0, total_files = 0;
        double      seconds = 0.0;
        std::string message;
        std::string current;  // su an inen dosyanin yerel tam yolu
        std::string dest;     // dosyalarin kaydedildigi kok klasor
    };

    ~Downloader();

    // Arkaplanda indirme baslatir. Zaten calisiyorsa false.
    // total_hint: tek dosya indirmede bilinen boyut (ilerleme cubugu icin).
    bool start(const std::string& server, const std::string& remote,
               const std::string& outdir, int parallel, uint64_t total_hint = 0);
    void cancel() { cancel_.store(true); }
    Snapshot snapshot();

private:
    void run(std::string server, std::string remote, std::string outdir,
             int parallel, uint64_t total_hint);

    std::thread th_;
    std::mutex  mtx_;   // message_, cur_, dest_ korumasi
    std::string message_;
    std::string cur_;    // su an inen dosyanin yerel yolu
    std::string dest_;   // hedef kok klasor (goruntuleme)
    std::atomic<bool>     active_{false};
    std::atomic<bool>     finished_{false};
    std::atomic<bool>     failed_{false};
    std::atomic<bool>     cancel_{false};
    std::atomic<uint64_t> bytes_{0}, files_{0};
    std::atomic<uint64_t> total_bytes_{0}, total_files_{0};
    std::atomic<double>   final_secs_{0.0};
    std::chrono::steady_clock::time_point t0_{};
};

} // namespace client
