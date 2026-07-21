# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`lan_share` is a Windows-only, single-file, statically-linked C++20 LAN file-sharing server. Any device on the same network connects via browser to upload/download files and folders. Ships as one exe with zero external DLLs (no Python/.NET runtime). Comments and user-facing strings are in Turkish; keep that convention when editing.

## Build & run

Requires MSVC v14.51+ (VS 2022/2026 Build Tools), Windows 11 SDK, CMake 3.21+, and vcpkg with `%VCPKG_ROOT%` set.

```cmd
build.bat            :: Release into build/bin/lan_share.exe (calls vcvars64 + CMake presets)
build.bat debug      :: Debug into build-debug/
```

`build.bat` hardcodes the vcvars path to `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\...`; adjust if your VS install differs. First build is ~15 min (vcpkg compiles deps from source), later builds use the binary cache.

Direct CMake (once a dev shell has MSVC + vcpkg on PATH): `cmake --preset default && cmake --build --preset default` (or `debug`). Presets use Ninja + `x64-windows-static` triplet + static MSVC runtime.

`baslat.bat` just launches the built exe. Dependencies are declared in `vcpkg.json` (Crow, nlohmann-json, fmt, CLI11, imgui with dx11+win32 bindings).

## Running the two modes

The exe is tri-mode, decided in [src/main.cpp](src/main.cpp) by argc/argv[1]:
- **No args → GUI**: ImGui + DX11 + Win32 window ([src/gui_app.cpp](src/gui_app.cpp)) with two tabs: "Paylas" (server control + log) and "Al (Indir)" (peer download: LAN device discovery via "Tara", remote file explorer over `/api/browse`, destination picker defaulting to Downloads, progress bar with cancel). This is the primary end-user flow — two users each run the exe and transfer both ways with no terminal.
- **`get` subcommand → download client** ([src/client.cpp](src/client.cpp)): `lan_share.exe get <host[:port]> <remote-path> [-o <dir>] [-j <parallel>]`. Connects to another lan_share server and pulls a file or whole folder over WS, writing directly to disk preserving structure, with parallel connections. This is how you get **fast, folder-structured downloads onto a second PC** without browser limitations.
- **Any other args → CLI headless server**: `lan_share.exe -p 9090 -d D:\share -j 3 -b 4` (port / dir / parallel / buffer-MB). Runs blocking, stop with Ctrl+C.

## Tests

No unit test framework. Verification is via Python integration scripts run against a **running** server (start the exe first on the expected port):
- `python test_ws.py` — WebSocket upload smoke test (small/medium/nested/multi-file/path-traversal cases). Expects server on port `18901`.
- `python benchmark.py` — C++ vs Python throughput comparison. Needs `httpx` + `websockets`; the Python reference numbers require a `main.py` prototype in the **parent** directory (skipped if absent).

## Architecture

Three translation units link into one exe ([CMakeLists.txt](CMakeLists.txt)):

- **[src/server.cpp](src/server.cpp)** — the core. Crow HTTP + WebSocket server, all routes, and lifecycle. `CROW_DISABLE_STATIC_DIR` is set; everything is served through explicit routes. Global state (`g_cfg`, `g_app`, `g_thread`, `g_running`) lives in an anonymous namespace.
- **[src/gui_app.cpp](src/gui_app.cpp)** — Dear ImGui/DX11/Win32 window that drives `server::start()`/`stop()`, folder picker (IFileDialog), and renders the live log.
- **[src/client.cpp](src/client.cpp)** — download-client core shared by CLI `get` and the GUI "Al" tab: `build_tasks` (fetch `/api/list` for a folder or treat the target as a single file) + `do_download` (worker-thread pool, each holding a `ws_client` connection that pulls files over `/ws/download` and writes to disk under the output dir, path-checked with `util::is_under`; one WS connection per worker reused across files). Also `client::discover` (UDP broadcast scan), `client::browse` (one-level remote listing), and `client::Downloader` (background download for the GUI with atomics-based `Snapshot` progress and cancel).
- **[src/ws_client.hpp](src/ws_client.hpp)** — header-only minimal synchronous WebSocket **client** over standalone Asio (handshake, client-side frame masking, fragment reassembly, auto-pong) plus a tiny blocking `http_get`. Crow only provides the server side; this is the client half. Include from exactly one TU (client.cpp).
- **[src/util.hpp](src/util.hpp)** — header-only pure helpers (UTF-8 ↔ `fs::path`, path-safety, HTTP range parse, template render, `local_ip`). Include from exactly one TU per compilation to avoid ODR issues.
- **[src/page_template.h](src/page_template.h)** — the entire browser UI (HTML/CSS/JS) as a string literal plus the favicon SVG, filled by `util::render_template` (Python-`str.format`-style `{name}` substitution, `{{`/`}}` escaping).

### Server lifecycle & threading

GUI path uses `server::start(cfg)` → spawns a background thread running `crow::SimpleApp::multithreaded().run()`; `server::stop()` calls `app.stop()` and joins. CLI path uses `server::run_blocking(cfg)` on the main thread. Thread count = `max(2, hardware_concurrency())`. Each HTTP request / WS connection is handled on its own Crow thread.

### Log abstraction (server.hpp)

All server output goes through `server::log()` / `logf()` into a swappable `LogSink`. Default sink writes ANSI-colored stdout (CLI); the GUI installs a sink writing to a thread-safe ring buffer that ImGui renders each frame. Never `printf`/`fmt::print` directly from server code — route through the sink so both modes work.

### Path safety (security-critical)

Every filesystem request is confined under `g_cfg.share_dir`. Key functions in util.hpp: `safe_join_under` (download/browse), `resolve_upload_path` + `safe_dest` (uploads, auto-creates nested dirs, dedupes name collisions), `is_under`. They reject `..`, absolute paths, and symlink escapes → 403 or clamp to base. Any new route touching the filesystem MUST go through these; do not build paths from raw request input directly.

### Upload paths

- `POST /api/upload?path=&dir=&name=` — buffered single-file upload (whole body in memory).
- `WS /ws/upload?dir=` — chunked streaming for large files: text frame `{path,size}` opens a file, binary frames stream data, server acks with JSON when `written == expected`. State per connection in `WSState` (`conn.userdata()`); partial uploads are removed on close. This is the high-throughput path.
- `POST /api/check_existing` — skip files already present at the same size.
- `WS /ws/download` — chunked streaming download that mirrors the upload path. Client sends `{path}`; server validates via `safe_join_under`, replies `{ok,name,size}`, then pushes binary chunks under a sliding window (`DL_WINDOW`, primed after meta, one more per client `{ack:true}`) so server RAM stays bounded to a few chunks regardless of file size. EOF → `{done:true}`. Browser side (in page_template.h `wsStream`/`wsDownloadFile`) streams chunks straight to disk via the File System Access API (`showSaveFilePicker`) when available, falling back to an in-memory Blob otherwise. This is the download path to prefer/extend.
- `GET /lan_share.exe` — serves the running exe itself (via `GetModuleFileNameW`) so a peer without the app can install it from the browser (bootstrap). Deliberately outside `safe_join_under` — it only ever reads the module's own path. The browser page links to it (header hint + footer in page_template.h).
- `GET /api/browse?dir=` — single-level directory listing (path-safe), returns `{ok,entries:[{name,dir,size}]}`. Used by the GUI's remote file explorer; keep it non-recursive so browsing huge shares stays cheap.
- **UDP discovery** — while the server runs, a responder thread listens on UDP `util::DISCOVERY_PORT` (48950) and answers `util::DISCOVERY_MAGIC` probes with `{lan_share,name,port,share}` JSON; `client::discover` broadcasts and collects replies so GUI users never need to type an IP. Constants live in [src/util.hpp](src/util.hpp) — keep server and client in sync through them.
- `GET /api/list?dir=` — recursive file listing (path-safe) under a directory, returns `{ok,name,count,total,files:[{path,size}]}`. Used by folder download (`wsDownloadFolder`): fetch the list, then pull each file over `/ws/download`. When File System Access API is available (secure context — localhost/https) files stream to a picked directory preserving structure; otherwise the client bundles them into a self-contained ZIP (store method, `ZipStore` in page_template.h, no external lib) and downloads a single `.zip`. **Note:** over a plain `http://<lan-ip>` origin the File System Access API is unavailable (insecure context), so single files use the Blob fallback and folders use the ZIP path.
- HTTP downloads (`serve_file`, still available via the `<a href>` links): files ≤512 MiB are read fully into RAM (avoids Crow's slow small-chunk streamer); larger files fall back to Crow's static-file streamer. HTTP Range requests (206) supported for resumable downloads.

Directory listings, downloads, and favicon are dispatched through `CROW_CATCHALL_ROUTE` → `dispatch_path` (which URL-decodes then calls `safe_join_under`), so explicit routes (`/ping`, `/api/*`, `/ws/*`) take precedence over path serving.
