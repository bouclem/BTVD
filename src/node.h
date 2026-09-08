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

    // Get list of connected peers.
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
    std::vector<std::string> peers_;

    void server_loop();
    void handle_peer_connection(socket_t socket);
    void handle_message(const std::string& msg, socket_t socket);
};

} // namespace bitvoid
