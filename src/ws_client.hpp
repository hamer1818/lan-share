// ws_client.hpp — minimal senkron WebSocket istemcisi (RFC 6455, client tarafi).
// Yalnizca tek TU'dan (client.cpp) include edilmeli. Asio standalone kullanir
// (Crow ile ayni). Bizim /ws/download protokolu icin yeterli: text/binary
// gonder/al, client frame'leri maskeler, ping'e otomatik pong, fragmentasyon
// birlestirme.
#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace wsc {

// WebSocket opcode'lari
enum : uint8_t { OP_CONT = 0x0, OP_TEXT = 0x1, OP_BIN = 0x2, OP_CLOSE = 0x8, OP_PING = 0x9, OP_PONG = 0xA };

inline std::vector<unsigned char> random_bytes(size_t n) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> d(0, 255);
    std::vector<unsigned char> v(n);
    for (auto& b : v) b = static_cast<unsigned char>(d(rng));
    return v;
}

inline std::string base64(const std::vector<unsigned char>& in) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c; bits += 8;
        while (bits >= 0) { out.push_back(T[(val >> bits) & 0x3F]); bits -= 6; }
    }
    if (bits > -6) out.push_back(T[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

class WebSocketClient {
public:
    WebSocketClient() : socket_(io_) {}
    ~WebSocketClient() { asio::error_code ec; socket_.close(ec); }

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    void connect(const std::string& host, uint16_t port, const std::string& target) {
        asio::ip::tcp::resolver res(io_);
        auto endpoints = res.resolve(host, std::to_string(port));
        asio::connect(socket_, endpoints);
        socket_.set_option(asio::ip::tcp::no_delay(true));
        handshake(host, port, target);
    }

    void send_text(const std::string& s)          { send_frame(OP_TEXT, s.data(), s.size()); }
    void send_binary(const void* d, size_t n)      { send_frame(OP_BIN, d, n); }

    // Bir uygulama mesaji al (ping/pong icten yonetilir). Donen opcode
    // OP_TEXT / OP_BIN / OP_CLOSE olur; payload doldurulur.
    uint8_t recv(std::string& out) {
        out.clear();
        uint8_t data_op = 0;
        for (;;) {
            uint8_t op; bool fin; std::string p;
            read_frame(op, fin, p);
            if (op == OP_PING)  { send_frame(OP_PONG, p.data(), p.size()); continue; }
            if (op == OP_PONG)  { continue; }
            if (op == OP_CLOSE) { return OP_CLOSE; }
            if (op == OP_CONT)  { out += p; }
            else                { data_op = op; out.swap(p); }
            if (fin) return data_op;
        }
    }

    void close() {
        try { unsigned char c[2] = {0x03, 0xE8}; send_frame(OP_CLOSE, c, 2); } catch (...) {}
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

private:
    asio::io_context      io_;
    asio::ip::tcp::socket socket_;
    std::string           leftover_;  // handshake sonrasi buffer'da kalan frame baytlari

    void handshake(const std::string& host, uint16_t port, const std::string& target) {
        std::string key = base64(random_bytes(16));
        std::string req =
            "GET " + target + " HTTP/1.1\r\n"
            "Host: " + host + ":" + std::to_string(port) + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + key + "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        asio::write(socket_, asio::buffer(req));

        asio::streambuf resp;
        asio::read_until(socket_, resp, "\r\n\r\n");
        std::istream is(&resp);
        // Tum HTTP baslik satirlarini bos satira kadar tuket; sadece durum
        // satirini degil (aksi halde kalan basliklar frame verisi sanilir).
        std::string line;
        bool first = true, ok = false;
        while (std::getline(is, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (first) { ok = line.find(" 101") != std::string::npos; first = false; }
            if (line.empty()) break;  // basliklarin sonu
        }
        if (!ok) throw std::runtime_error("WS handshake basarisiz");
        // read_until fazladan bayt cekmis olabilir — bunlar frame verisidir, sakla.
        std::string rest((std::istreambuf_iterator<char>(is)),
                          std::istreambuf_iterator<char>());
        leftover_ = std::move(rest);
    }

    void read_exact(char* dst, size_t n) {
        size_t off = 0;
        if (!leftover_.empty()) {
            size_t take = std::min(n, leftover_.size());
            std::memcpy(dst, leftover_.data(), take);
            leftover_.erase(0, take);
            off += take;
        }
        if (off < n) asio::read(socket_, asio::buffer(dst + off, n - off));
    }

    void read_frame(uint8_t& opcode, bool& fin, std::string& payload) {
        unsigned char h[2];
        read_exact(reinterpret_cast<char*>(h), 2);
        fin    = (h[0] & 0x80) != 0;
        opcode = h[0] & 0x0F;
        bool masked = (h[1] & 0x80) != 0;
        uint64_t len = h[1] & 0x7F;
        if (len == 126) {
            unsigned char e[2]; read_exact(reinterpret_cast<char*>(e), 2);
            len = (static_cast<uint64_t>(e[0]) << 8) | e[1];
        } else if (len == 127) {
            unsigned char e[8]; read_exact(reinterpret_cast<char*>(e), 8);
            len = 0; for (int i = 0; i < 8; ++i) len = (len << 8) | e[i];
        }
        unsigned char mask[4] = {0, 0, 0, 0};
        if (masked) read_exact(reinterpret_cast<char*>(mask), 4);
        payload.resize(static_cast<size_t>(len));
        if (len) read_exact(payload.data(), static_cast<size_t>(len));
        if (masked) for (uint64_t i = 0; i < len; ++i) payload[i] ^= mask[i & 3];
    }

    void send_frame(uint8_t opcode, const void* data, size_t n) {
        std::vector<unsigned char> f;
        f.reserve(n + 14);
        f.push_back(0x80 | opcode);  // FIN + opcode
        const unsigned char M = 0x80;  // client -> maskeli
        if (n < 126) {
            f.push_back(M | static_cast<unsigned char>(n));
        } else if (n <= 0xFFFF) {
            f.push_back(M | 126);
            f.push_back((n >> 8) & 0xFF); f.push_back(n & 0xFF);
        } else {
            f.push_back(M | 127);
            for (int i = 7; i >= 0; --i) f.push_back((static_cast<uint64_t>(n) >> (8 * i)) & 0xFF);
        }
        auto mk = random_bytes(4);
        unsigned char mask[4] = {mk[0], mk[1], mk[2], mk[3]};
        f.insert(f.end(), mask, mask + 4);
        size_t hdr = f.size();
        f.resize(hdr + n);
        const unsigned char* src = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < n; ++i) f[hdr + i] = src[i] ^ mask[i & 3];
        asio::write(socket_, asio::buffer(f));
    }
};

// ─── Basit senkron HTTP GET (yalnizca kucuk JSON yanitlari icin) ───
// Content-Length varsa onu okur; yoksa baglanti kapanana kadar okur.
// status: HTTP durum kodu, body: govde. Baglanti timeout_ms icinde kurulamazsa
// firlatir (GUI'de yanlis IP girildiginde donmamak icin).
inline void http_get(const std::string& host, uint16_t port,
                     const std::string& path, int& status, std::string& body,
                     int timeout_ms = 4000) {
    asio::io_context io;
    asio::ip::tcp::socket sock(io);
    asio::ip::tcp::resolver res(io);
    auto endpoints = res.resolve(host, std::to_string(port));
    asio::error_code cec = asio::error::would_block;
    asio::async_connect(sock, endpoints,
        [&](const asio::error_code& ec, const asio::ip::tcp::endpoint&) { cec = ec; });
    io.run_for(std::chrono::milliseconds(timeout_ms));
    if (cec) {
        asio::error_code ig; sock.close(ig);
        throw std::runtime_error(cec == asio::error::would_block
            ? "baglanti zaman asimi" : "baglanti basarisiz");
    }
    sock.set_option(asio::ip::tcp::no_delay(true));

    std::string req =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + std::to_string(port) + "\r\n"
        "Connection: close\r\n"
        "\r\n";
    asio::write(sock, asio::buffer(req));

    asio::error_code ec;
    std::string data;
    std::array<char, 8192> buf;
    for (;;) {
        size_t n = sock.read_some(asio::buffer(buf), ec);
        if (n) data.append(buf.data(), n);
        if (ec) break;  // eof
    }

    status = 0;
    auto sp = data.find(' ');
    if (sp != std::string::npos) status = std::atoi(data.c_str() + sp + 1);
    auto hdr_end = data.find("\r\n\r\n");
    body = (hdr_end != std::string::npos) ? data.substr(hdr_end + 4) : std::string{};
}

} // namespace wsc
