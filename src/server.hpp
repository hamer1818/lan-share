// server.hpp — HTTP/WebSocket sunucu hayat dongusu ve loglama.
#pragma once

#include <fmt/core.h>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace server {

struct Config {
    std::filesystem::path share_dir;
    int          port      = 8000;
    int          parallel  = 3;
    int          buffer_mb = 4;
    std::string  server_info;  // "http://IP:PORT" — banner+template icin
};

// ─── Log abstraction ───────────────────────────────
// Server tarafindan uretilen tum mesajlar bu sink'e gider.
// CLI: ANSI renkli stdout. GUI: thread-safe ring buffer.

enum class LogLevel { Info, Ok, Warn, Err, Up, Down, Ws };

using LogSink = std::function<void(LogLevel, std::string_view)>;

void set_log_sink(LogSink sink);
void log(LogLevel lvl, std::string_view text);

// Renkli, kategori bazli loglama — fmt formatli.
template <typename... A>
void logf(LogLevel lvl, fmt::format_string<A...> fmt_str, A&&... args) {
    log(lvl, fmt::format(fmt_str, std::forward<A>(args)...));
}

// ─── Server lifecycle ──────────────────────────────

// Server'i background thread'de baslat. start() basariliysa true.
// Daha onceden calisiyorsa once stop() cagrilmali.
bool start(const Config& cfg);

// Server'i durdur ve thread'i join et.
void stop();

bool is_running();

// CLI modu: bu thread'de blocking calistir. Ctrl+C ile durur.
void run_blocking(const Config& cfg);

} // namespace server

#include <fmt/core.h>  // logf template icin
