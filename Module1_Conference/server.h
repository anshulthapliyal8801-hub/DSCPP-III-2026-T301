// MUNSphere — Module1_Conference/server.h
// The single HTTP server for the whole application. It serves the API
// through the Router and the frontend files from disk.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "API.h"

namespace mun {

class HttpServer {
public:
    explicit HttpServer(const Router& router);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // URL prefix -> folder, e.g. "/module1/" -> "Module1_Conference/frontend".
    // The longest matching prefix wins.
    void addStaticMount(const std::string& urlPrefix, const std::string& folder);

    bool start(const std::string& host, int port);  // prints the reason on failure
    void run();                                      // accept loop; never returns

private:
    const Router& router_;
    std::vector<std::pair<std::string, std::string>> mounts_;
    long long listenSocket_ = -1;
    bool winsockStarted_ = false;

    void handleClient(long long client);
    HttpResponse serveStatic(const std::string& path) const;
};

}  // namespace mun
