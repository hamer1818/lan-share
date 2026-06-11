// util.hpp — saf yardimci fonksiyonlar (header-only, tek TU'dan include edilmeli).
#pragma once

#include <fmt/core.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace util {

namespace fs = std::filesystem;

// UTF-8 <-> fs::path
inline fs::path utf8_to_path(std::string_view s) {
    return fs::path(std::u8string(
        reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

inline std::string path_to_utf8(const fs::path& p) {
    auto u8 = p.u8string();
    return std::string(
        reinterpret_cast<const char*>(u8.data()), u8.size());
}

inline std::string human_size(uint64_t size) {
    constexpr const char* units[] = {"B", "KB", "MB", "GB"};
    double v = static_cast<double>(size);
    for (int i = 0; i < 4; ++i) {
        if (v < 1024.0) return fmt::format("{:.0f} {}", v, units[i]);
        v /= 1024.0;
    }
    return fmt::format("{:.1f} TB", v);
}

inline std::string html_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) switch (c) {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&#39;";  break;
        default:   out += c;        break;
    }
    return out;
}

inline std::string url_encode_path(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        const bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_'
            || c == '.' || c == '~' || c == '/';
        if (keep) out += static_cast<char>(c);
        else      out += fmt::format("%{:02X}", c);
    }
    return out;
}

// Python str.format() davranisi: {name} ikame, {{ ve }} kacis.
inline std::string render_template(
    std::string_view tmpl,
    const std::vector<std::pair<std::string_view, std::string>>& vars)
{
    std::string s(tmpl);
    for (const auto& [k, v] : vars) {
        std::string key = "{" + std::string(k) + "}";
        size_t pos = 0;
        while ((pos = s.find(key, pos)) != std::string::npos) {
            s.replace(pos, key.size(), v);
            pos += v.size();
        }
    }
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (i + 1 < s.size() && s[i] == '{' && s[i + 1] == '{') {
            out += '{'; ++i;
        } else if (i + 1 < s.size() && s[i] == '}' && s[i + 1] == '}') {
            out += '}'; ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

inline bool is_under(const fs::path& base, const fs::path& candidate) {
    std::error_code ec;
    auto base_canon = fs::weakly_canonical(base, ec);
    if (ec) base_canon = base.lexically_normal();
    auto cand_canon = fs::weakly_canonical(candidate, ec);
    if (ec) cand_canon = candidate.lexically_normal();
    auto rel = cand_canon.lexically_relative(base_canon);
    if (rel.empty()) return false;
    auto s = rel.native();
    return !(s.size() >= 2 && s[0] == L'.' && s[1] == L'.');
}

inline std::optional<fs::path> safe_join_under(
    const fs::path& base, std::string_view raw)
{
    std::string cleaned;
    cleaned.reserve(raw.size());
    for (char c : raw) cleaned += (c == '\\') ? '/' : c;
    while (!cleaned.empty() && cleaned.front() == '/') cleaned.erase(0, 1);
    fs::path candidate = base;
    if (!cleaned.empty()) candidate = base / utf8_to_path(cleaned);
    std::error_code ec;
    fs::path resolved = fs::weakly_canonical(candidate, ec);
    if (ec) resolved = candidate.lexically_normal();
    auto base_canon = fs::weakly_canonical(base, ec);
    if (ec) base_canon = base.lexically_normal();
    auto rel = resolved.lexically_relative(base_canon);
    if (rel.empty()) return std::nullopt;
    auto s = rel.native();
    if (s.size() >= 2 && s[0] == L'.' && s[1] == L'.') return std::nullopt;
    return resolved;
}

inline fs::path safe_dest(const fs::path& target_dir, std::string_view filename) {
    fs::path fname = utf8_to_path(filename).filename();
    if (fname.empty()) fname = fs::path("upload");
    fs::path dest = target_dir / fname;
    int counter = 1;
    fs::path stem_p = dest.stem();
    fs::path ext_p  = dest.extension();
    while (fs::exists(dest)) {
        std::string stem = path_to_utf8(stem_p);
        std::string ext  = path_to_utf8(ext_p);
        dest = target_dir / utf8_to_path(fmt::format("{}_{}{}", stem, counter, ext));
        ++counter;
    }
    return dest;
}

inline std::pair<fs::path, std::string> resolve_upload_path(
    const fs::path& base,
    std::string_view dir_param,
    std::string_view rel_path)
{
    std::string raw_dir(dir_param);
    while (!raw_dir.empty() && raw_dir.front() == '/') raw_dir.erase(0, 1);
    fs::path current = base;
    if (!raw_dir.empty()) {
        auto resolved = safe_join_under(base, raw_dir);
        if (resolved && fs::is_directory(*resolved)) current = *resolved;
    }
    if (!fs::is_directory(current)) current = base;

    std::string rp(rel_path);
    for (auto& c : rp) if (c == '\\') c = '/';
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= rp.size()) {
        auto j = rp.find('/', i);
        if (j == std::string::npos) { parts.push_back(rp.substr(i)); break; }
        parts.push_back(rp.substr(i, j - i));
        i = j + 1;
    }
    std::string filename = parts.empty() ? "upload" : parts.back();
    if (filename.empty()) filename = "upload";
    if (!parts.empty()) parts.pop_back();

    fs::path target_dir = current;
    for (auto& part : parts) {
        std::string clean = part;
        auto l = clean.find_first_not_of(" \t");
        auto r = clean.find_last_not_of(" \t");
        if (l == std::string::npos) continue;
        clean = clean.substr(l, r - l + 1);
        if (clean.empty() || clean == "." || clean == "..") continue;
        fs::path candidate = (target_dir / utf8_to_path(clean)).lexically_normal();
        if (!is_under(base, candidate)) break;
        std::error_code ec;
        fs::create_directories(candidate, ec);
        target_dir = candidate;
    }
    return {target_dir, filename};
}

inline std::string format_local_time(const fs::file_time_type& t) {
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(t);
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm{};
    localtime_s(&tm, &tt);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%d.%m %H:%M", &tm);
    return std::string(buf);
}

inline std::string local_ip() {
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return "127.0.0.1";
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (::connect(s, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) != 0) {
        closesocket(s);
        return "127.0.0.1";
    }
    sockaddr_in local{};
    int len = sizeof(local);
    getsockname(s, reinterpret_cast<sockaddr*>(&local), &len);
    std::array<char, INET_ADDRSTRLEN> buf{};
    inet_ntop(AF_INET, &local.sin_addr, buf.data(), buf.size());
    closesocket(s);
    return std::string(buf.data());
}

struct RangeSpec { uint64_t start; uint64_t end; };

inline std::optional<RangeSpec> parse_range(std::string_view h, uint64_t file_size) {
    if (file_size == 0) return std::nullopt;
    if (h.size() < 7 || h.substr(0, 6) != "bytes=") return std::nullopt;
    auto spec = h.substr(6);
    auto dash = spec.find('-');
    if (dash == std::string_view::npos) return std::nullopt;
    auto first = spec.substr(0, dash);
    auto last  = spec.substr(dash + 1);
    uint64_t start = 0, end = file_size - 1;
    try {
        if (first.empty()) {
            if (last.empty()) return std::nullopt;
            uint64_t n = std::stoull(std::string(last));
            if (n == 0) return std::nullopt;
            if (n > file_size) n = file_size;
            start = file_size - n;
        } else if (last.empty()) {
            start = std::stoull(std::string(first));
        } else {
            start = std::stoull(std::string(first));
            end   = std::stoull(std::string(last));
        }
    } catch (...) {
        return std::nullopt;
    }
    if (end >= file_size) end = file_size - 1;
    if (start >= file_size || start > end) return std::nullopt;
    return RangeSpec{start, end};
}

} // namespace util
