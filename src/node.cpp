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

    // Close all peer sockets.
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& peer : peers_) {
        CLOSE_SOCKET(peer.socket);
    }
    peers_.clear();

    std::cout << "Node stopped." << std::endl;
}

bool Node::is_running() const {
    return running_;
}

// --- Peer management ---

void Node::add_peer(socket_t socket, const std::string& address, bool incoming) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    // Check if already connected.
    for (const auto& p : peers_) {
        if (p.socket == socket) return;
    }
    peers_.push_back({socket, address, incoming});
    std::cout << "Peer added: " << address << " (total: " << peers_.size() << ")" << std::endl;
}

void Node::remove_peer(socket_t socket) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto it = peers_.begin(); it != peers_.end(); ++it) {
        if (it->socket == socket) {
            std::cout << "Peer disconnected: " << it->address << std::endl;
            CLOSE_SOCKET(it->socket);
            peers_.erase(it);
            return;
        }
    }
}

void Node::send_to_peer(socket_t socket, const std::string& msg) {
    std::string data = msg + "\n";
    send(socket, data.c_str(), (int)data.size(), 0);
}

void Node::send_to_all(const std::string& msg) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::string data = msg + "\n";
    for (auto& peer : peers_) {
        send(peer.socket, data.c_str(), (int)data.size(), 0);
    }
}

void Node::request_sync(socket_t socket) {
    nlohmann::json j;
    j["type"] = "sync_request";
    j["from_height"] = chain_.height();
    send_to_peer(socket, j.dump());
}

void Node::send_peer_list(socket_t socket) {
    nlohmann::json j;
    j["type"] = "peer_list";
    j["peers"] = nlohmann::json::array();

    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& p : peers_) {
        if (p.socket != socket) {
            j["peers"].push_back(p.address);
        }
    }

    send_to_peer(socket, j.dump());
}

void Node::connect_to_known_peers(const std::vector<std::string>& peer_addrs) {
    for (const auto& addr : peer_addrs) {
        // Don't connect to ourselves or already-connected peers.
        size_t colon = addr.find(':');
        if (colon == std::string::npos) continue;

        std::string host = addr.substr(0, colon);
        uint16_t peer_port = static_cast<uint16_t>(std::stoul(addr.substr(colon + 1)));

        // Skip our own port.
        if (peer_port == port_ && (host == "127.0.0.1" || host == "localhost")) continue;

        // Check if already connected.
        bool already = false;
        {
            std::lock_guard<std::mutex> lock(peers_mutex_);
            for (const auto& p : peers_) {
                if (p.address == addr) { already = true; break; }
            }
        }
        if (already) continue;

        connect_to_peer(host, peer_port);
    }
}

// --- Server loop ---

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
                std::string addr = std::string(ip) + ":" + std::to_string(ntohs(client_addr.sin_port));
                std::cout << "New connection from " << addr << std::endl;

                std::thread(&Node::handle_peer_connection, this, client_fd, addr, true).detach();
            }
        }
    }

    CLOSE_SOCKET(server_fd);
}

void Node::handle_peer_connection(socket_t socket, const std::string& address, bool incoming) {
    add_peer(socket, address, incoming);

    // Request sync if we're behind, and exchange peer lists.
    request_sync(socket);
    send_peer_list(socket);

    char buffer[4096];
    std::string accumulated;

    while (running_) {
        int bytes = recv(socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        accumulated.append(buffer, bytes);

        // Try to parse complete JSON messages (newline-delimited).
        size_t newline_pos;
        while ((newline_pos = accumulated.find('\n')) != std::string::npos) {
            std::string msg = accumulated.substr(0, newline_pos);
            accumulated.erase(0, newline_pos + 1);

            if (!msg.empty()) {
                handle_message(msg, socket);
            }
        }
    }

    remove_peer(socket);
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
                // Relay to other peers.
                broadcast_transaction(tx);
            } else {
                std::cout << "Rejected invalid transaction" << std::endl;
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

                // Relay to other peers.
                broadcast_block(block);
            } else {
                std::cout << "Rejected invalid block" << std::endl;
            }
        } else if (type == "sync_request") {
            // Peer wants blocks from a specific height.
            uint64_t from = j["from_height"].get<uint64_t>();
            auto blocks = chain_.get_blocks(from);

            nlohmann::json response;
            response["type"] = "sync_response";
            response["blocks"] = nlohmann::json::array();
            for (const auto& b : blocks) {
                response["blocks"].push_back(b.to_json());
            }

            send_to_peer(socket, response.dump());
        } else if (type == "sync_response") {
            // Peer sent us blocks to sync.
            auto blocks = j["blocks"];
            uint64_t added = 0;
            for (const auto& b_json : blocks) {
                Block block = Block::from_json(b_json);
                if (chain_.add_block(block)) {
                    added++;
                }
            }
            if (added > 0) {
                std::cout << "Synced " << added << " blocks from peer" << std::endl;
                chain_.save_to_disk("bitvoid.chain");
            }
        } else if (type == "get_height") {
            // Peer wants to know our chain height.
            nlohmann::json response;
            response["type"] = "height";
            response["height"] = chain_.height();
            send_to_peer(socket, response.dump());
        } else if (type == "height") {
            // Peer told us their height. If they're ahead, request sync.
            uint64_t peer_height = j["height"].get<uint64_t>();
            if (peer_height > chain_.height()) {
                request_sync(socket);
            }
        } else if (type == "peer_list") {
            // Peer sent us their known peers — connect to any we don't know.
            if (j.contains("peers")) {
                std::vector<std::string> known;
                for (const auto& p : j["peers"]) {
                    known.push_back(p.get<std::string>());
                }
                connect_to_known_peers(known);
            }
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
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return false;
    }

    std::string address = host + ":" + std::to_string(port);
    std::cout << "Connected to peer " << address << std::endl;

    // Handle this peer in a detached thread.
    std::thread(&Node::handle_peer_connection, this, sock, address, false).detach();

    return true;
}

void Node::broadcast_transaction(const Transaction& tx) {
    nlohmann::json j;
    j["type"] = "tx";
    j["data"] = tx.to_json();
    send_to_all(j.dump());
}

void Node::broadcast_block(const Block& block) {
    nlohmann::json j;
    j["type"] = "block";
    j["data"] = block.to_json();
    send_to_all(j.dump());
}

std::vector<std::string> Node::get_peers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<std::string> result;
    for (const auto& p : peers_) {
        result.push_back(p.address);
    }
    return result;
}

} // namespace bitvoid
