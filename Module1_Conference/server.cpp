// MUNSphere — Module1_Conference/server.cpp
//
// A small single-threaded HTTP/1.1 server.
//
// Design decision: one request at a time. Every request runs to
// completion before the next is accepted, so the C linked lists never see
// concurrent access and need no locks. A conference has tens of users and
// each request takes well under a millisecond, so this is a sound trade:
// simplicity and correctness over throughput we don't need.
//
// Built on Winsock (Windows/MinGW, link with -lws2_32) or POSIX sockets.
#include "server.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
// inet_pton needs Windows Vista or later declarations.
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static const socket_t kBadSocket = INVALID_SOCKET;
static void closeSocket(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using socket_t = int;
static const socket_t kBadSocket = -1;
static void closeSocket(socket_t s) { close(s); }
#endif

#ifdef MSG_NOSIGNAL
static const int kSendFlags = MSG_NOSIGNAL;  // don't die if the browser hangs up
#else
static const int kSendFlags = 0;
#endif

namespace mun {

namespace {
const std::size_t kMaxHeaderBytes = 16 * 1024;
const std::size_t kMaxBodyBytes = 1024 * 1024;

socket_t toSock(long long v) { return static_cast<socket_t>(v); }

std::string lowerStr(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool sendAll(socket_t s, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        int chunk = static_cast<int>(std::min<std::size_t>(data.size() - sent, 64 * 1024));
        int n = send(s, data.data() + sent, chunk, kSendFlags);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

void sendResponse(socket_t s, const HttpResponse& res) {
    std::ostringstream head;
    head << "HTTP/1.1 " << res.status << ' ' << statusText(res.status) << "\r\n"
         << "Content-Type: " << res.contentType << "\r\n"
         << "Content-Length: " << res.body.size() << "\r\n"
         << "Connection: close\r\n"
         << "Cache-Control: no-store\r\n"
         << "X-Content-Type-Options: nosniff\r\n"
         << "Referrer-Policy: no-referrer\r\n";
    if (res.contentType.rfind("text/html", 0) == 0)
        head << "Content-Security-Policy: default-src 'self'; img-src 'self' data:; "
                "style-src 'self'; script-src 'self'; object-src 'none'; "
                "frame-ancestors 'none'; base-uri 'none'\r\n";
    for (const auto& h : res.headers) head << h.first << ": " << h.second << "\r\n";
    head << "\r\n";
    if (sendAll(s, head.str())) sendAll(s, res.body);
}

std::string mimeType(const std::string& path) {
    std::size_t dot = path.rfind('.');
    std::string ext = dot == std::string::npos ? "" : lowerStr(path.substr(dot + 1));
    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "text/javascript; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "ico") return "image/x-icon";
    if (ext == "txt") return "text/plain; charset=utf-8";
    if (ext == "csv") return "text/csv; charset=utf-8";
    return "application/octet-stream";
}

// Rejects any path that could escape the mounted folder.
bool safeRelativePath(const std::string& rel) {
    if (rel.find('\\') != std::string::npos || rel.find(':') != std::string::npos ||
        rel.find('\0') != std::string::npos)
        return false;
    std::size_t start = 0;
    while (start <= rel.size()) {
        std::size_t slash = rel.find('/', start);
        if (slash == std::string::npos) slash = rel.size();
        std::string seg = rel.substr(start, slash - start);
        if (seg == ".." || seg == ".") return false;
        start = slash + 1;
    }
    return true;
}
}  // namespace

HttpServer::HttpServer(const Router& router) : router_(router) {}

HttpServer::~HttpServer() {
    if (listenSocket_ != -1) closeSocket(toSock(listenSocket_));
#ifdef _WIN32
    if (winsockStarted_) WSACleanup();
#endif
}

void HttpServer::addStaticMount(const std::string& prefix, const std::string& folder) {
    mounts_.emplace_back(prefix, folder);
    std::sort(mounts_.begin(), mounts_.end(), [](const auto& a, const auto& b) {
        return a.first.size() > b.first.size();  // longest prefix first
    });
}

bool HttpServer::start(const std::string& host, int port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "Winsock could not start.\n");
        return false;
    }
    winsockStarted_ = true;
#endif
    socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kBadSocket) {
        std::fprintf(stderr, "Could not create a socket.\n");
        return false;
    }
#ifdef _WIN32
    // Stop another program from binding the same port while we run.
    BOOL excl = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&excl), sizeof excl);
#else
    int yes = 1;  // allow a quick restart after the server stops
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::fprintf(stderr, "'%s' is not a valid IPv4 address.\n", host.c_str());
        closeSocket(s);
        return false;
    }
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        std::fprintf(stderr,
                     "Port %d is busy or not allowed. Close the other program using it, "
                     "or start with --port <another number>.\n",
                     port);
        closeSocket(s);
        return false;
    }
    if (listen(s, 16) != 0) {
        std::fprintf(stderr, "Could not listen on port %d.\n", port);
        closeSocket(s);
        return false;
    }
    listenSocket_ = static_cast<long long>(s);
    return true;
}

void HttpServer::run() {
    for (;;) {
        socket_t client = accept(toSock(listenSocket_), nullptr, nullptr);
        if (client == kBadSocket) continue;
        // A client that connects and sends nothing must not block the server.
#ifdef _WIN32
        DWORD timeoutMs = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs),
                   sizeof timeoutMs);
#else
        timeval tv{5, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
        handleClient(static_cast<long long>(client));
        closeSocket(client);
    }
}

void HttpServer::handleClient(long long clientHandle) {
    socket_t client = toSock(clientHandle);
    auto t0 = std::chrono::steady_clock::now();
    std::string buf;
    char chunk[4096];

    // 1. Read until the end of the headers.
    std::size_t headerEnd;
    while ((headerEnd = buf.find("\r\n\r\n")) == std::string::npos) {
        if (buf.size() > kMaxHeaderBytes) {
            sendResponse(client, HttpResponse::error(431, "HEADERS_TOO_LARGE", "Request headers are too large."));
            return;
        }
        int n = recv(client, chunk, sizeof chunk, 0);
        if (n <= 0) return;  // closed or timed out before a full request
        buf.append(chunk, static_cast<std::size_t>(n));
    }

    // 2. Request line: METHOD /path?query HTTP/1.1
    HttpRequest req;
    std::istringstream head(buf.substr(0, headerEnd));
    std::string line, target, version;
    std::getline(head, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    {
        std::istringstream rl(line);
        rl >> req.method >> target >> version;
    }
    if (req.method.empty() || target.empty() || target[0] != '/' ||
        version.rfind("HTTP/1.", 0) != 0) {
        sendResponse(client, HttpResponse::error(400, "BAD_REQUEST", "Malformed request line."));
        return;
    }
    std::size_t qmark = target.find('?');
    req.path = urlDecode(target.substr(0, qmark), false);
    if (qmark != std::string::npos) req.query = parseQueryString(target.substr(qmark + 1));

    // 3. Headers
    while (std::getline(head, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        req.headers[lowerStr(validate::trim(line.substr(0, colon)))] =
            validate::trim(line.substr(colon + 1));
    }

    // 4. Body (Content-Length only; browsers send it for fetch() bodies)
    if (req.headers.count("transfer-encoding")) {
        sendResponse(client, HttpResponse::error(501, "NOT_IMPLEMENTED", "Chunked request bodies aren't supported."));
        return;
    }
    std::size_t contentLength = 0;
    if (req.headers.count("content-length")) {
        const std::string& cl = req.headers["content-length"];
        if (cl.empty() || cl.size() > 9 ||
            cl.find_first_not_of("0123456789") != std::string::npos) {
            sendResponse(client, HttpResponse::error(400, "BAD_REQUEST", "Invalid Content-Length."));
            return;
        }
        contentLength = static_cast<std::size_t>(std::stoul(cl));
        if (contentLength > kMaxBodyBytes) {
            sendResponse(client, HttpResponse::error(413, "TOO_LARGE", "Request body is larger than 1 MB."));
            return;
        }
    }
    req.body = buf.substr(headerEnd + 4);
    while (req.body.size() < contentLength) {
        int n = recv(client, chunk, sizeof chunk, 0);
        if (n <= 0) {
            sendResponse(client, HttpResponse::error(408, "TIMEOUT", "The request body didn't arrive in time."));
            return;
        }
        req.body.append(chunk, static_cast<std::size_t>(n));
    }
    if (req.body.size() > contentLength) req.body.resize(contentLength);

    // 5. Route: API or static file
    HttpResponse res;
    if (req.path == "/api" || req.path.rfind("/api/", 0) == 0) {
        res = router_.dispatch(req);
    } else if (req.method == "GET" || req.method == "HEAD") {
        res = serveStatic(req.path);
        if (req.method == "HEAD") res.body.clear();
    } else {
        res = HttpResponse::error(405, "METHOD_NOT_ALLOWED", "Only GET is allowed for pages.");
    }
    sendResponse(client, res);

    long long us = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - t0).count();
    std::printf("%-6s %-40s %d  %lld us\n", req.method.c_str(), req.path.c_str(), res.status, us);
    std::fflush(stdout);
}

HttpResponse HttpServer::serveStatic(const std::string& path) const {
    for (const auto& mount : mounts_) {
        const std::string& prefix = mount.first;
        // "/module1" without the trailing slash -> redirect so relative
        // links inside that page resolve correctly.
        if (prefix.size() > 1 && path + "/" == prefix) {
            HttpResponse r;
            r.status = 301;
            r.contentType = "text/plain; charset=utf-8";
            r.headers.emplace_back("Location", prefix);
            return r;
        }
        if (path.rfind(prefix, 0) != 0) continue;
        std::string rel = path.substr(prefix.size());
        if (rel.empty() || rel.back() == '/') rel += "index.html";
        if (!safeRelativePath(rel)) break;
        std::ifstream in(mount.second + "/" + rel, std::ios::binary);
        if (!in) break;
        std::ostringstream ss;
        ss << in.rdbuf();
        HttpResponse r;
        r.contentType = mimeType(rel);
        r.body = ss.str();
        return r;
    }
    HttpResponse r;
    r.status = 404;
    r.contentType = "text/plain; charset=utf-8";
    r.body = "Not found: " + path;
    return r;
}

}  // namespace mun
