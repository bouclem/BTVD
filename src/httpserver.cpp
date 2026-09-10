#include "httpserver.h"
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
    #define INVALID_SOCK INVALID_SOCKET
    struct WinSockInit {
        WinSockInit() { WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa); }
        ~WinSockInit() { WSACleanup(); }
    };
    static WinSockInit winsock_init;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/select.h>
    #define CLOSE_SOCKET close
    #define INVALID_SOCK -1
#endif

namespace bitvoid {

HttpServer::HttpServer(uint16_t port) : port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::set_handler(RequestHandler handler) {
    handler_ = std::move(handler);
}

void HttpServer::start() {
    running_ = true;
    server_thread_ = std::thread(&HttpServer::server_loop, this);
    std::cout << "HTTP server started on port " << port_ << std::endl;
}

void HttpServer::stop() {
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    std::cout << "HTTP server stopped." << std::endl;
}

bool HttpServer::is_running() const {
    return running_;
}

void HttpServer::server_loop() {
    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCK) {
        std::cerr << "Failed to create HTTP socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind HTTP server on port " << port_ << std::endl;
        CLOSE_SOCKET(server_fd);
        return;
    }

    listen(server_fd, 10);

    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select((int)server_fd + 1, &read_fds, nullptr, nullptr, &timeout);

        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            socket_t client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd != INVALID_SOCK) {
                std::thread(&HttpServer::handle_connection, this, client_fd).detach();
            }
        }
    }

    CLOSE_SOCKET(server_fd);
}

void HttpServer::handle_connection(socket_t client_socket) {
    char buffer[4096];
    int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        CLOSE_SOCKET(client_socket);
        return;
    }

    buffer[bytes] = '\0';
    std::string request(buffer, bytes);

    // Parse the request line: "GET /path?query HTTP/1.1"
    size_t first_space = request.find(' ');
    if (first_space == std::string::npos) {
        send(client_socket, build_response(400, "text/plain", "Bad Request").c_str(),
             (int)build_response(400, "text/plain", "Bad Request").size(), 0);
        CLOSE_SOCKET(client_socket);
        return;
    }

    std::string method = request.substr(0, first_space);

    size_t second_space = request.find(' ', first_space + 1);
    if (second_space == std::string::npos) {
        CLOSE_SOCKET(client_socket);
        return;
    }

    std::string full_path = request.substr(first_space + 1, second_space - first_space - 1);

    std::string query;
    std::string path = parse_path(full_path, method, query);

    std::string response;
    if (handler_) {
        std::string body = handler_(method, path, query);
        response = build_response(200, "application/json", body);
    } else {
        response = build_response(404, "text/plain", "Not Found");
    }

    send(client_socket, response.c_str(), (int)response.size(), 0);
    CLOSE_SOCKET(client_socket);
}

std::string HttpServer::parse_path(const std::string& full_path, std::string& method, std::string& query) {
    size_t qmark = full_path.find('?');
    if (qmark != std::string::npos) {
        query = full_path.substr(qmark + 1);
        return full_path.substr(0, qmark);
    }
    query = "";
    return full_path;
}

std::string HttpServer::build_response(int status_code, const std::string& content_type,
                                        const std::string& body) {
    std::string status_text;
    switch (status_code) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "OK"; break;
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    return response.str();
}

} // namespace bitvoid
