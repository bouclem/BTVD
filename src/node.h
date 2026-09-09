#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "blockchain.h"
#include "mempool.h"
#include "consensus.h"

#ifdef _WIN32
    #include <winsock2.h>
    using socket_t = SOCKET;
#else
    using socket_t = int;
#endif

namespace bitvoid {

// Peer info: tracks a connected peer's socket and address.
struct PeerInfo {
    socket_t socket;
    std::string address;  // "host:port"
    bool incoming;         // true if peer connected to us, false if we connected out
};

// Node: P2P networking layer.
// Handles peer connections, block/transaction propagation, and chain sync.
class Node {
public:
    Node(uint16_t port, Blockchain& chain, Mempool& mempool, ConsensusEngine& consensus);
    ~Node();

    // Start the node (listening for peers).
    void start();

    // Stop the node.
    void stop();

    // Connect to a peer.
    bool connect_to_peer(const std::string& host, uint16_t port);

    // Broadcast a transaction to all peers.
    void broadcast_transaction(const Transaction& tx);

    // Broadcast a block to all peers.
    void broadcast_block(const Block& block);

    // Get list of connected peer addresses.
    std::vector<std::string> get_peers() const;

    // Check if node is running.
    bool is_running() const;

private:
    uint16_t port_;
    Blockchain& chain_;
    Mempool& mempool_;
    ConsensusEngine& consensus_;

    std::atomic<bool> running_{false};
    std::thread server_thread_;

    mutable std::mutex peers_mutex_;
    std::vector<PeerInfo> peers_;

    // Peer management.
    void add_peer(socket_t socket, const std::string& address, bool incoming);
    void remove_peer(socket_t socket);
    void send_to_peer(socket_t socket, const std::string& msg);
    void send_to_all(const std::string& msg);

    // Request chain sync from a peer.
    void request_sync(socket_t socket);

    // Exchange peer lists with a connected peer.
    void send_peer_list(socket_t socket);
    void connect_to_known_peers(const std::vector<std::string>& peer_addrs);

    // Server and message handling.
    void server_loop();
    void handle_peer_connection(socket_t socket, const std::string& address, bool incoming);
    void handle_message(const std::string& msg, socket_t socket);
};

} // namespace bitvoid
