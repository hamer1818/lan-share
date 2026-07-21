cmake_minimum_required(VERSION 3.21)
project(lan_share LANGUAGES CXX VERSION 4.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /Zc:__cplusplus /permissive-)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)

    # Compile .rc directly instead of through CMake's cmcldeps dependency
    # wrapper. On non-English MSVC locales the wrapper injects the localized
    # "show includes" prefix (e.g. "Not: eklenen dosya:") into the rc.exe
    # command line, which fails with RC1107. Dropping depfile tracking for
    # resources is harmless — app.rc rarely changes.
    set(CMAKE_NINJA_CMCLDEPS_RC OFF)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /fo <OBJECT> <SOURCE>")
endif()

# Statik link — vcpkg'in kopyalanacak DLL aramasina gerek yok. Bu adim ayrica
# pwsh.exe cagirdigi icin bazi ortamlarda link'i gereksiz yere bozuyor.
set(VCPKG_APPLOCAL_DEPS OFF)

find_package(Crow CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(lan_share
    src/main.cpp
    src/server.cpp
    src/gui_app.cpp
    src/client.cpp
)

if(WIN32)
    target_sources(lan_share PRIVATE assets/app.rc)
endif()

target_include_directories(lan_share PRIVATE src)

target_link_libraries(lan_share PRIVATE
    Crow::Crow
    nlohmann_json::nlohmann_json
    fmt::fmt
    CLI11::CLI11
    imgui::imgui
    ws2_32
    d3d11
    dwmapi
    dxgi
    d3dcompiler
)

set_target_properties(lan_share PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
