// MUNSphere — Shared/API.h
// Transport-independent HTTP types and the API router. The socket server
// (Module1_Conference/server.cpp) parses bytes into an HttpRequest and
// sends back an HttpResponse; everything here can be unit-tested without
// opening a socket.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Common.h"

namespace mun {

struct HttpRequest {
    std::string method;                          // "GET", "POST", ...
    std::string path;                            // decoded, no query string
    std::map<std::string, std::string> query;    // decoded ?a=b&c=d
    std::map<std::string, std::string> headers;  // keys lower-cased
    std::map<std::string, std::string> params;   // from route ":name" parts
    std::string route;                           // matched pattern, e.g. "/api/sessions/:id"
    std::string body;

    // Parses the body; throws ApiError(400) unless it is a JSON object.
    Json json() const;
    std::string queryParam(const std::string& key,
                           const std::string& fallback = "") const;
    std::string param(const std::string& key) const;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    // Success envelope: {"ok":true,"data":...}
    static HttpResponse ok(const Json& data, int status = 200);
    // Error envelope: {"ok":false,"error":{"code":...,"message":...}}
    static HttpResponse error(int status, const std::string& code,
                              const std::string& message);
};

using Handler = std::function<HttpResponse(const HttpRequest&)>;
// Called after every request that matched a route (success or error).
using Observer = std::function<void(const HttpRequest&, const HttpResponse&)>;

class Router {
public:
    // Patterns use ":name" for path parameters, e.g. "/api/sessions/:id".
    void add(const std::string& method, const std::string& pattern, Handler h);
    void get(const std::string& p, Handler h) { add("GET", p, std::move(h)); }
    void post(const std::string& p, Handler h) { add("POST", p, std::move(h)); }
    void put(const std::string& p, Handler h) { add("PUT", p, std::move(h)); }
    void del(const std::string& p, Handler h) { add("DELETE", p, std::move(h)); }

    // Finds the route, runs it and converts every exception into a JSON
    // error response. 404 if no pattern matches, 405 if the path matches
    // but the method does not.
    HttpResponse dispatch(HttpRequest& req) const;

    std::size_t routeCount() const { return routes_.size(); }
    void setObserver(Observer o) { observer_ = std::move(o); }
    // Every registered (method, pattern), e.g. for the authorization test.
    std::vector<std::pair<std::string, std::string>> routes() const;

private:
    struct Route {
        std::string method;
        std::string pattern;
        std::vector<std::string> segments;
        Handler handler;
    };
    std::vector<Route> routes_;
    Observer observer_;
    HttpResponse run(const Route& r, HttpRequest& req) const;

    static std::vector<std::string> split(const std::string& path);
    static bool match(const Route& r, const std::vector<std::string>& parts,
                      std::map<std::string, std::string>& params);
};

std::string statusText(int status);
std::string urlDecode(const std::string& s, bool plusIsSpace);
std::map<std::string, std::string> parseQueryString(const std::string& qs);

}  // namespace mun
