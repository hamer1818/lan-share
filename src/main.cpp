// main.cpp — entry point. Argumansiz → GUI, argumanli → CLI mode.

#include <CLI/CLI.hpp>
#include <fmt/color.h>
#include <fmt/core.h>

#include <winsock2.h>
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <string>

#include "server.hpp"
#include "gui_app.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace {

void cli_banner(const server::Config& cfg, const std::string& ip) {
    using namespace fmt;
    const auto purple = fg(color::medium_purple) | emphasis::bold;
    const auto cyan   = fg(color::cyan)          | emphasis::bold;
    const auto green  = fg(color::lime_green);
    const auto white  = fg(color::white);
    const auto dim    = fg(color::gray);
    const auto red    = fg(color::tomato);
    const auto line   = std::string(56, '=');

    print("\n");
    print(purple, "  {}\n", line);
    print(cyan,   "    LAN Dosya Paylasim Sunucusu  v4.0 (C++ Port)\n");
    print(purple, "  {}\n", line);
    print(dim, "  Klasor : "); print(white, "{}\n", util::path_to_utf8(cfg.share_dir));
    print(dim, "  Yerel  : "); print(green, "http://localhost:{}\n", cfg.port);
    print(dim, "  Ag     : "); print(green | emphasis::bold, "http://{}:{}\n", ip, cfg.port);
    print(dim, "  Buffer : "); print(white, "{} MB  ", cfg.buffer_mb);
    print(dim, "|  Paralel : "); print(white, "{} dosya\n", cfg.parallel);
    print(purple, "  {}\n", line);
    print(dim, "  Durdurmak icin "); print(red, "Ctrl+C\n");
    print(purple, "  {}\n\n", line);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // Argumansiz → GUI mode
    if (argc <= 1) {
        int rc = gui::run();
        WSACleanup();
        return rc;
    }

    // CLI mode
    CLI::App app{"High-performance LAN file sharing server"};
    server::Config cfg;
    std::string dir_str = ".";
    app.add_option("-p,--port",     cfg.port,      "Port (default 8000)");
    app.add_option("-d,--dir",      dir_str,       "Share directory (default .)");
    app.add_option("-j,--parallel", cfg.parallel,  "Parallel uploads (default 3)");
    app.add_option("-b,--buffer",   cfg.buffer_mb, "I/O buffer MB (default 4)");
    CLI11_PARSE(app, argc, argv);

    std::error_code ec;
    cfg.share_dir = fs::canonical(util::utf8_to_path(dir_str), ec);
    if (ec || !fs::is_directory(cfg.share_dir)) {
        fmt::print(stderr, "Hata: '{}' bir klasor degil.\n", dir_str);
        WSACleanup();
        return 1;
    }

    const std::string ip = util::local_ip();
    cfg.server_info = fmt::format("http://{}:{}", ip, cfg.port);

    cli_banner(cfg, ip);

    server::run_blocking(cfg);

    WSACleanup();
    return 0;
}
