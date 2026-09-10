#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>

#ifdef _WIN32
    #include <winsock2.h>
    using socket_t = SOCKET;
#else
    using socket_t = int;
#endif

namespace bitvoid {

// Minimal HTTP/1.1 server for the block explorer API.
// Handles GET requests, returns JSON responses with CORS headers.
class HttpServer {
public:
    using RequestHandler = std::function<std::string(const std::string& method,
                                                      const std::string& path,
                                                      const std::string& query)>;

    HttpServer(uint16_t port);
    ~HttpServer();

    // Set the handler for all requests.
    void set_handler(RequestHandler handler);

    // Start the server.
    void start();

    // Stop the server.
    void stop();

    // Check if running.
    bool is_running() const;

private:
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    RequestHandler handler_;

    void server_loop();
    void handle_connection(socket_t client_socket);
    std::string parse_path(const std::string& request_line, std::string& method, std::string& query);
    std::string build_response(int status_code, const std::string& content_type,
                                const std::string& body);
};

} // namespace bitvoid
