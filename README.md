# lan_share

Yüksek performanslı LAN dosya paylaşım sunucusu — Windows için tek dosya, statik linkli C++ uygulaması.

Aynı ağdaki herhangi bir cihaz (telefon, başka PC, tablet) tarayıcısından bağlanıp dosya/klasör gönderebilir ve indirebilir.

![exe boyutu](https://img.shields.io/badge/exe-1.98%20MB-brightgreen)
![bağımlılık](https://img.shields.io/badge/external%20DLL-0-blue)
![lisans](https://img.shields.io/badge/license-MIT-yellow)

## Özellikler

- **Modern web arayüzü** — drag-drop, klasör yükleme (yapı korunur), canlı ilerleme panosu, dark tema
- **WebSocket akış** — 4 MB chunk'lar halinde GB'lık dosyalar RAM şişirmeden
- **Range download** — yarıda kalan indirmeler devam edebilir
- **`/api/check_existing`** — aynı boyutlu dosyaları otomatik atla
- **Path traversal güvenliği** — `..` ile dış klasöre kaçış engellendi
- **UTF-8 dosya adları** — Türkçe, Arapça, CJK, emoji
- **GUI + CLI** — çift tıkla görsel arayüz, argümanla headless server
- **Tek statik exe** — sıfır external DLL, Python/.NET runtime gerekmez
- **Multi-thread** — donanım çekirdek sayısı kadar

## Kullanım

**GUI** — `lan_share.exe`'yi çift tıkla:
- Klasör seç (modern Windows folder dialog)
- Port belirle (default 8000)
- ▶ Sunucuyu Baslat
- Renkli canlı log

**CLI** — terminal'den:
```cmd
lan_share.exe                       # GUI açar
lan_share.exe -p 9090               # CLI, port 9090
lan_share.exe -d D:\paylasim        # belirli klasör
lan_share.exe -p 8080 -d D:\foo     # ikisi de
```

Aynı ağdaki cihazlardan: `http://<senin-IP>:8000` ile bağlan. Sunucu başlatıldığında banner LAN IP'sini gösterir.

## Performans

`benchmark.py` çıktısı (loopback, AMD Ryzen / NVMe SSD):

| Senaryo | Python (referans) | C++ | Hızlanma |
|---|---|---|---|
| WS upload 4 MB | 35.2 MB/s | **280.3 MB/s** | 7.96× |
| WS upload 256 MB | 50.9 MB/s | **313.3 MB/s** | 6.16× |
| WS bulk 5K × 1 KB | 1304 dosya/s | 1529 dosya/s | 1.17× |
| HTTP download 256 MB | 208.0 MB/s | 208.2 MB/s | 1.00× (loopback ceiling) |

Python karşılaştırması için `benchmark.py` üst dizinde bir `main.py` (orijinal Python prototipi) bekler. Yoksa script o ölçümü atlar.

## Build

Visual Studio 2022/2026 Build Tools (MSVC v14.51+, Windows 11 SDK), CMake 3.21+, ve [vcpkg](https://github.com/microsoft/vcpkg) gerekli.

```cmd
:: %VCPKG_ROOT% set olmali, vcpkg PATH'te olmali
build.bat                  :: Release (default)
build.bat debug            :: Debug
```

İlk build ~15 dk (vcpkg paketleri kaynaktan derlenir), sonrakiler binary cache ile saniyeler içinde.

## Mimari

| Dosya | İçerik |
|---|---|
| `src/main.cpp` | Entry: argümansız → GUI, argümanlı → CLI |
| `src/server.hpp/.cpp` | Crow setup, routes, lifecycle, log abstraction |
| `src/gui_app.hpp/.cpp` | Dear ImGui + DX11 + Win32 pencere |
| `src/util.hpp` | UTF-8 path, range parse, path safety, render_template, vs. |
| `src/page_template.h` | Tarayıcı HTML/CSS/JS — UI çekirdeği |
| `assets/app.rc` + `app.ico` | Pencere ikonu + version info |

**Server thread modeli**: Crow background thread'de `multithreaded().run()`. Her HTTP isteği ve WebSocket bağlantısı kendi thread'inde.

**Log abstraction**: `server::log()` → `LogSink`. CLI sink ANSI renkli stdout'a, GUI sink thread-safe ring buffer'a yazar (ImGui her frame render eder).

**Path safety**: `util::safe_join_under()` ve `util::resolve_upload_path()` her isteği base directory altına sıkıştırır; `..`, mutlak yollar, sembolik kaçış denemeleri 403 veya base'e kırpılır.

## Bağımlılıklar (vcpkg)

- [Crow](https://crowcpp.org/) 1.3 — HTTP + WebSocket
- [Asio](https://think-async.com/Asio) 1.32 — Crow'un I/O katmanı
- [Dear ImGui](https://github.com/ocornut/imgui) 1.92 (DX11 + Win32 binding)
- [nlohmann/json](https://github.com/nlohmann/json) 3.12
- [fmt](https://fmt.dev/) 12 — string format + ANSI renk
- [CLI11](https://github.com/CLIUtils/CLI11) 2.6 — argparse

## Lisans

MIT — bkz. [LICENSE](LICENSE).

## Yazar

**Hamza ORTATEPE** — [github.com/hamer1818](https://github.com/hamer1818)
