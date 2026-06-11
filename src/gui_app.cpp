// gui_app.cpp — Dear ImGui + DX11 + Win32 GUI.

#include <windows.h>
#include <shobjidl.h>      // IFileDialog
#include <shlobj.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "gui_app.hpp"
#include "server.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

// ImGui-Win32 backend ileri bildirim
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace gui {

namespace {

// ── DX11 ────────────────────────────────────────────
ID3D11Device*           g_pd3dDevice           = nullptr;
ID3D11DeviceContext*    g_pd3dDeviceContext    = nullptr;
IDXGISwapChain*         g_pSwapChain           = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width  = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevelArray, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevelArray, 2, D3D11_SDK_VERSION,
            &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }
    if (FAILED(hr)) return false;

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr,
                                              &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
    return true;
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr,
                                              &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

// ── Log panel ───────────────────────────────────────
struct LogEntry { server::LogLevel lvl; std::string text; };

struct LogPanel {
    std::mutex mtx;
    std::deque<LogEntry> entries;
    static constexpr size_t MAX_ENTRIES = 5000;
    bool auto_scroll = true;
    bool dirty       = false;

    void push(server::LogLevel lvl, std::string_view text) {
        std::lock_guard lk(mtx);
        entries.push_back({lvl, std::string(text)});
        if (entries.size() > MAX_ENTRIES) entries.pop_front();
        dirty = true;
    }

    void clear() {
        std::lock_guard lk(mtx);
        entries.clear();
        dirty = true;
    }
};

LogPanel g_log;

ImVec4 color_for(server::LogLevel lvl) {
    switch (lvl) {
        case server::LogLevel::Up:   return ImVec4(0.13f, 0.77f, 0.37f, 1.0f); // green
        case server::LogLevel::Down: return ImVec4(0.40f, 0.74f, 0.94f, 1.0f); // sky
        case server::LogLevel::Ws:   return ImVec4(0.36f, 0.55f, 0.96f, 1.0f); // blue
        case server::LogLevel::Ok:   return ImVec4(0.13f, 0.77f, 0.37f, 1.0f);
        case server::LogLevel::Warn: return ImVec4(0.96f, 0.62f, 0.04f, 1.0f);
        case server::LogLevel::Err:  return ImVec4(0.94f, 0.27f, 0.27f, 1.0f);
        case server::LogLevel::Info:
        default:                     return ImVec4(0.58f, 0.65f, 0.73f, 1.0f);
    }
}

const char* prefix_for(server::LogLevel lvl) {
    switch (lvl) {
        case server::LogLevel::Up:   return "<<";
        case server::LogLevel::Down: return ">>";
        case server::LogLevel::Ws:   return "WS";
        case server::LogLevel::Ok:   return "OK";
        case server::LogLevel::Warn: return "!!";
        case server::LogLevel::Err:  return "ER";
        case server::LogLevel::Info:
        default:                     return "..";
    }
}

// ── UI state ────────────────────────────────────────
char  g_dir_buf[1024] = {};
int   g_port          = 8000;
char  g_status[512]   = "Hazir";
ImVec4 g_status_color = ImVec4(0.58f, 0.65f, 0.73f, 1.0f);

void set_status(const std::string& msg, ImVec4 color) {
    std::snprintf(g_status, sizeof(g_status), "%s", msg.c_str());
    g_status_color = color;
}

bool browse_folder(std::string& out) {
    IFileDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&pDialog));
    if (FAILED(hr)) return false;

    DWORD options = 0;
    pDialog->GetOptions(&options);
    pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    pDialog->SetTitle(L"Paylasilacak klasoru sec");

    hr = pDialog->Show(nullptr);
    if (FAILED(hr)) { pDialog->Release(); return false; }

    IShellItem* pItem = nullptr;
    hr = pDialog->GetResult(&pItem);
    if (FAILED(hr)) { pDialog->Release(); return false; }

    PWSTR pszPath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (SUCCEEDED(hr) && pszPath) {
        int n = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1,
                                    nullptr, 0, nullptr, nullptr);
        std::string utf8(n - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, pszPath, -1,
                            utf8.data(), n, nullptr, nullptr);
        out = std::move(utf8);
        CoTaskMemFree(pszPath);
    }
    pItem->Release();
    pDialog->Release();
    return !out.empty();
}

void start_server_btn() {
    fs::path share = util::utf8_to_path(g_dir_buf);
    std::error_code ec;
    share = fs::canonical(share, ec);
    if (ec || !fs::is_directory(share)) {
        set_status("Hata: gecerli bir klasor sec.",
                   ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
        return;
    }
    if (g_port < 1 || g_port > 65535) {
        set_status("Hata: port araligi 1-65535.",
                   ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
        return;
    }
    std::string ip = util::local_ip();
    server::Config cfg{
        .share_dir   = share,
        .port        = g_port,
        .parallel    = 3,
        .buffer_mb   = 4,
        .server_info = fmt::format("http://{}:{}", ip, g_port),
    };
    if (server::start(cfg)) {
        set_status(fmt::format("Calisiyor — http://{}:{}", ip, g_port),
                   ImVec4(0.13f, 0.77f, 0.37f, 1.0f));
        server::logf(server::LogLevel::Ok,
            "Sunucu basladi — Yerel: http://localhost:{}  Ag: http://{}:{}",
            g_port, ip, g_port);
        server::logf(server::LogLevel::Info,
            "Paylasilan: {}", util::path_to_utf8(share));
    } else {
        set_status("Server zaten calisiyor.", ImVec4(0.96f, 0.62f, 0.04f, 1.0f));
    }
}

void stop_server_btn() {
    set_status("Durduruluyor...", ImVec4(0.58f, 0.65f, 0.73f, 1.0f));
    server::stop();
    set_status("Durduruldu", ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
    server::logf(server::LogLevel::Warn, "Sunucu durduruldu.");
}

void apply_dark_theme() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    // Tkinter karsiligi: bg #0f172a, card #1e293b, accent #6366f1, ok #22c55e, err #ef4444
    auto rgba = [](int r, int g, int b, float a = 1.0f) {
        return ImVec4(r/255.0f, g/255.0f, b/255.0f, a);
    };

    c[ImGuiCol_WindowBg]            = rgba(15, 23, 42);
    c[ImGuiCol_ChildBg]             = rgba(10, 14, 26);
    c[ImGuiCol_FrameBg]             = rgba(30, 41, 59);
    c[ImGuiCol_FrameBgHovered]      = rgba(45, 58, 79);
    c[ImGuiCol_FrameBgActive]       = rgba(99, 102, 241, 0.4f);
    c[ImGuiCol_TitleBg]             = rgba(15, 23, 42);
    c[ImGuiCol_TitleBgActive]       = rgba(30, 41, 59);
    c[ImGuiCol_Border]              = rgba(99, 102, 241, 0.25f);
    c[ImGuiCol_Button]              = rgba(99, 102, 241);
    c[ImGuiCol_ButtonHovered]       = rgba(124, 124, 255);
    c[ImGuiCol_ButtonActive]        = rgba(79, 70, 229);
    c[ImGuiCol_Header]              = rgba(30, 41, 59);
    c[ImGuiCol_HeaderHovered]       = rgba(45, 58, 79);
    c[ImGuiCol_HeaderActive]        = rgba(99, 102, 241, 0.6f);
    c[ImGuiCol_Text]                = rgba(241, 245, 249);
    c[ImGuiCol_TextDisabled]        = rgba(100, 116, 139);
    c[ImGuiCol_ScrollbarBg]         = rgba(15, 23, 42, 0.0f);
    c[ImGuiCol_ScrollbarGrab]       = rgba(99, 102, 241, 0.5f);
    c[ImGuiCol_ScrollbarGrabHovered]= rgba(99, 102, 241, 0.8f);

    s.WindowRounding   = 8.0f;
    s.FrameRounding    = 6.0f;
    s.GrabRounding     = 6.0f;
    s.WindowPadding    = ImVec2(16, 16);
    s.FramePadding     = ImVec2(8, 6);
    s.ItemSpacing      = ImVec2(8, 8);
    s.ScrollbarSize    = 10.0f;
    s.ScrollbarRounding= 5.0f;
}

void render_main_window() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::Begin("lan_share", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Baslik
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.98f, 1.0f));
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("LAN Dosya Paylasimi");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::TextDisabled("v4.0 — C++ Port (Crow + Dear ImGui)");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    bool running = server::is_running();

    // Klasor
    ImGui::TextUnformatted("Paylasilacak Klasor");
    ImGui::PushItemWidth(-110);
    ImGui::BeginDisabled(running);
    ImGui::InputText("##dir", g_dir_buf, sizeof(g_dir_buf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Sec...", ImVec2(100, 0))) {
        std::string picked;
        if (browse_folder(picked)) {
            std::snprintf(g_dir_buf, sizeof(g_dir_buf), "%s", picked.c_str());
        }
    }
    ImGui::EndDisabled();

    // Port
    ImGui::Spacing();
    ImGui::TextUnformatted("Port");
    ImGui::PushItemWidth(120);
    ImGui::BeginDisabled(running);
    ImGui::InputInt("##port", &g_port, 1, 100);
    if (g_port < 1)     g_port = 1;
    if (g_port > 65535) g_port = 65535;
    ImGui::EndDisabled();
    ImGui::PopItemWidth();

    // Start / Stop
    ImGui::Spacing(); ImGui::Spacing();
    if (!running) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.77f, 0.37f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.85f, 0.44f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.65f, 0.30f, 1.0f));
        if (ImGui::Button("▶  Sunucuyu Baslat", ImVec2(220, 36))) start_server_btn();
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.35f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.18f, 0.18f, 1.0f));
        if (ImGui::Button("■  Sunucuyu Durdur", ImVec2(220, 36))) stop_server_btn();
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(g_status_color, "%s", g_status);

    // Log paneli
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextUnformatted("Log");
    ImGui::SameLine();
    if (ImGui::SmallButton("Temizle")) g_log.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Otomatik kaydir", &g_log.auto_scroll);

    ImGui::BeginChild("LogScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard lk(g_log.mtx);
        for (const auto& e : g_log.entries) {
            ImGui::TextColored(color_for(e.lvl), "%s", prefix_for(e.lvl));
            ImGui::SameLine();
            ImGui::TextWrapped("%s", e.text.c_str());
        }
        if (g_log.auto_scroll && g_log.dirty) {
            ImGui::SetScrollHereY(1.0f);
            g_log.dirty = false;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// ── WndProc ─────────────────────────────────────────
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                                        DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // ALT app menu disable
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // anonymous namespace

// ──────────────────────────────────────────────
//  run()
// ──────────────────────────────────────────────

int run() {
    // Log sink -> g_log
    server::set_log_sink([](server::LogLevel lvl, std::string_view text) {
        g_log.push(lvl, text);
    });

    // Default dir = mevcut calisma dizini
    {
        std::string cwd = util::path_to_utf8(fs::current_path());
        std::snprintf(g_dir_buf, sizeof(g_dir_buf), "%s", cwd.c_str());
    }

    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = ::GetModuleHandleW(nullptr);
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"LanShareWindow";
    HICON appIcon = (HICON)::LoadImageW(
        wc.hInstance, MAKEINTRESOURCEW(1),
        IMAGE_ICON, 0, 0,
        LR_DEFAULTSIZE | LR_SHARED);
    wc.hIcon   = appIcon;
    wc.hIconSm = appIcon;
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(
        0, wc.lpszClassName, L"LAN Dosya Paylasimi",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 820, 640,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // .ini olusturma
    apply_dark_theme();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        render_main_window();

        ImGui::Render();
        const float bg[4] = {0.04f, 0.05f, 0.10f, 1.00f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, bg);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);  // V-sync
    }

    // Sunucuyu temiz kapat
    server::stop();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    ::CoUninitialize();
    return 0;
}

} // namespace gui
