#include "node.h"
#include "sha256.h"
#include <nlohmann/json.hpp>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define CLOSE_SOCKET closesocket
    #define INVALID_SOCK INVALID_SOCKET
    // Need WSAStartup on Windows.
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
    using socket_t = int;
    #define CLOSE_SOCKET close
    #define INVALID_SOCK -1
#endif

#include <cstring>
#include <iostream>
#include <sstream>

namespace bitvoid {

Node::Node(uint16_t port, Blockchain& chain, Mempool& mempool, ConsensusEngine& consensus)
    : port_(port), chain_(chain), mempool_(mempool), consensus_(consensus) {}

Node::~Node() {
    stop();
}

void Node::start() {
    running_ = true;
    server_thread_ = std::thread(&Node::server_loop, this);
    std::cout << "Node started on port " << port_ << std::endl;
}

void Node::stop() {
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    std::cout << "Node stopped." << std::endl;
}

bool Node::is_running() const {
    return running_;
}

void Node::server_loop() {
    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCK) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind on port " << port_ << std::endl;
        CLOSE_SOCKET(server_fd);
        return;
    }

    listen(server_fd, 10);

    std::cout << "Listening for connections..." << std::endl;

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
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
                std::cout << "New connection from " << ip << std::endl;

                std::thread(&Node::handle_peer_connection, this, client_fd).detach();
            }
        }
    }

    CLOSE_SOCKET(server_fd);
}

void Node::handle_peer_connection(socket_t socket) {
    char buffer[4096];
    std::string accumulated;

    while (running_) {
        int bytes = recv(socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        accumulated.append(buffer, bytes);

        // Try to parse complete JSON messages.
        size_t newline_pos;
        while ((newline_pos = accumulated.find('\n')) != std::string::npos) {
            std::string msg = accumulated.substr(0, newline_pos);
            accumulated.erase(0, newline_pos + 1);

            if (!msg.empty()) {
                handle_message(msg, socket);
            }
        }
    }

    CLOSE_SOCKET(socket);
}

void Node::handle_message(const std::string& msg, socket_t socket) {
    try {
        auto j = nlohmann::json::parse(msg);
        std::string type = j["type"].get<std::string>();

        if (type == "tx") {
            // Incoming transaction.
            Transaction tx = Transaction::from_json(j["data"]);
            if (chain_.validate_transaction(tx)) {
                mempool_.add_transaction(tx);
                std::cout << "Received valid transaction: " << tx.hash.substr(0, 16) << "..." << std::endl;
            }
        } else if (type == "block") {
            // Incoming block.
            Block block = Block::from_json(j["data"]);
            if (chain_.validate_block(block)) {
                chain_.add_block(block);
                std::cout << "Received valid block: " << block.header.index << std::endl;

                // Remove confirmed transactions from mempool.
                for (const auto& tx : block.transactions) {
                    mempool_.remove_transaction(tx.hash);
                }
            }
        } else if (type == "sync_request") {
            // Peer wants to sync from a specific height.
            uint64_t from = j["from_height"].get<uint64_t>();
            auto blocks = chain_.get_blocks(from);

            nlohmann::json response;
            response["type"] = "sync_response";
            response["blocks"] = nlohmann::json::array();
            for (const auto& b : blocks) {
                response["blocks"].push_back(b.to_json());
            }

            std::string response_str = response.dump() + "\n";
            send(socket, response_str.c_str(), (int)response_str.size(), 0);
        } else if (type == "get_height") {
            // Peer wants to know our chain height.
            nlohmann::json response;
            response["type"] = "height";
            response["height"] = chain_.height();
            std::string response_str = response.dump() + "\n";
            send(socket, response_str.c_str(), (int)response_str.size(), 0);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling message: " << e.what() << std::endl;
    }
}

bool Node::connect_to_peer(const std::string& host, uint16_t port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) return false;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        CLOSE_SOCKET(sock);
        return false;
    }

    std::lock_guard<std::mutex> lock(peers_mutex_);
    peers_.push_back(host + ":" + std::to_string(port));

    // Handle this peer in a detached thread.
    std::thread(&Node::handle_peer_connection, this, sock).detach();

    std::cout << "Connected to peer " << host << ":" << port << std::endl;
    return true;
}

void Node::broadcast_transaction(const Transaction& tx) {
    nlohmann::json j;
    j["type"] = "tx";
    j["data"] = tx.to_json();
    std::string msg = j.dump() + "\n";

    // In a full implementation, we'd send to all connected peer sockets.
    // For now, just log it.
    std::cout << "Broadcasting transaction " << tx.hash.substr(0, 16) << "..." << std::endl;
}

void Node::broadcast_block(const Block& block) {
    nlohmann::json j;
    j["type"] = "block";
    j["data"] = block.to_json();
    std::string msg = j.dump() + "\n";

    std::cout << "Broadcasting block " << block.header.index << std::endl;
}

std::vector<std::string> Node::get_peers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_;
}

} // namespace bitvoid
