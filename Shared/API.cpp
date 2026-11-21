// MUNSphere — Shared/API.cpp
#include "API.h"

#include <cstdio>
#include <exception>

namespace mun {

// ------------------------------------------------------------ Request
Json HttpRequest::json() const {
    if (body.empty())
        throw ApiError(400, "BAD_REQUEST", "Request body must be a JSON object.");
    Json j;
    try {
        j = Json::parse(body);
    } catch (const JsonError& e) {
        throw ApiError(400, "BAD_JSON", e.what());
    }
    if (!j.isObject())
        throw ApiError(400, "BAD_REQUEST", "Request body must be a JSON object.");
    return j;
}

std::string HttpRequest::queryParam(const std::string& key,
                                    const std::string& fallback) const {
    auto it = query.find(key);
    return it == query.end() ? fallback : it->second;
}

std::string HttpRequest::param(const std::string& key) const {
    auto it = params.find(key);
    return it == params.end() ? std::string() : it->second;
}

// ----------------------------------------------------------- Response
HttpResponse HttpResponse::ok(const Json& data, int status) {
    HttpResponse r;
    r.status = status;
    Json env = Json::object();
    env.set("ok", true).set("data", data);
    r.body = env.dump();
    return r;
}

HttpResponse HttpResponse::error(int status, const std::string& code,
                                 const std::string& message) {
    HttpResponse r;
    r.status = status;
    Json err = Json::object();
    err.set("code", code).set("message", message);
    Json env = Json::object();
    env.set("ok", false).set("error", err);
    r.body = env.dump();
    return r;
}

// ------------------------------------------------------------- Router
std::vector<std::string> Router::split(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : path) {
        if (c == '/') {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

void Router::add(const std::string& method, const std::string& pattern, Handler h) {
    routes_.push_back(Route{method, pattern, split(pattern), std::move(h)});
}

bool Router::match(const Route& r, const std::vector<std::string>& parts,
                   std::map<std::string, std::string>& params) {
    if (r.segments.size() != parts.size()) return false;
    std::map<std::string, std::string> found;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::string& seg = r.segments[i];
        if (!seg.empty() && seg[0] == ':')
            found[seg.substr(1)] = parts[i];
        else if (seg != parts[i])
            return false;
    }
    params = std::move(found);
    return true;
}

HttpResponse Router::dispatch(HttpRequest& req) const {
    std::vector<std::string> parts = split(req.path);
    bool pathMatched = false;
    for (const Route& r : routes_) {
        std::map<std::string, std::string> params;
        if (!match(r, parts, params)) continue;
        pathMatched = true;
        if (r.method != req.method) continue;
        req.params = std::move(params);
        req.route = r.pattern;
        HttpResponse res = run(r, req);
        if (observer_) observer_(req, res);
        return res;
    }
    if (pathMatched)
        return HttpResponse::error(405, "METHOD_NOT_ALLOWED",
                                   req.method + " isn't supported for " + req.path + ".");
    return HttpResponse::error(404, "NO_ROUTE", "No API endpoint at " + req.path + ".");
}

// Runs one handler and turns every exception into a JSON error response.
HttpResponse Router::run(const Route& r, HttpRequest& req) const {
    try {
        return r.handler(req);
    } catch (const ApiError& e) {
        return HttpResponse::error(e.status(), e.code(), e.what());
    } catch (const JsonError& e) {
        return HttpResponse::error(400, "BAD_JSON", e.what());
    } catch (const std::exception& e) {
        // Log details on the server; never leak internals to clients.
        std::fprintf(stderr, "[error] %s %s: %s\n", req.method.c_str(), req.path.c_str(), e.what());
        return HttpResponse::error(500, "INTERNAL", "The server hit an unexpected error.");
    }
}

std::vector<std::pair<std::string, std::string>> Router::routes() const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const Route& r : routes_) out.emplace_back(r.method, r.pattern);
    return out;
}

// -------------------------------------------------------------- Utils
std::string statusText(int s) {
    switch (s) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default:  return "Unknown";
    }
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& s, bool plusIsSpace) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '%' && i + 2 < s.size()) {
            int hi = hexVal(s[i + 1]);
            int lo = hexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>(hi * 16 + lo);
                i += 2;
                continue;
            }
        }
        out += (plusIsSpace && c == '+') ? ' ' : c;
    }
    return out;
}

std::map<std::string, std::string> parseQueryString(const std::string& qs) {
    std::map<std::string, std::string> out;
    std::size_t start = 0;
    while (start <= qs.size()) {
        std::size_t amp = qs.find('&', start);
        if (amp == std::string::npos) amp = qs.size();
        std::string pair = qs.substr(start, amp - start);
        if (!pair.empty()) {
            std::size_t eq = pair.find('=');
            std::string k = urlDecode(pair.substr(0, eq), true);
            std::string v = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1), true);
            out[k] = v;
        }
        start = amp + 1;
    }
    return out;
}

}  // namespace mun
