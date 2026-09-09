#include "blockchain.h"
#include "mempool.h"
#include "consensus.h"
#include "wallet.h"
#include "node.h"
#include "sha256.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using namespace bitvoid;

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

void print_help() {
    std::cout << "BitVoid Core v0.1.0" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  bitvoid-core mine     [--address X] [--energy Y]    Start mining" << std::endl;
    std::cout << "  bitvoid-core node     [--port P]                   Run a full node" << std::endl;
    std::cout << "  bitvoid-core wallet   [--new] [--balance] [--send]  Wallet operations" << std::endl;
    std::cout << "  bitvoid-core status                                Show chain status" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --address X    Miner/wallet address" << std::endl;
    std::cout << "  --energy Y     Declared energy source (solar, hydro, wind, etc.)" << std::endl;
    std::cout << "  --port P       P2P port (default: 8333)" << std::endl;
    std::cout << "  --connect H:P  Connect to peer at host:port" << std::endl;
    std::cout << "  --new          Create new wallet (saved to wallet.key or --wallet-file)" << std::endl;
    std::cout << "  --balance      Show wallet balance" << std::endl;
    std::cout << "  --send A:ADDR  Send A BTVD to ADDR (requires --wallet-file)" << std::endl;
    std::cout << "  --wallet-file F  Path to wallet key file (default: wallet.key)" << std::endl;
    std::cout << "  --help         Show this help" << std::endl;
}

std::string get_arg(int argc, char* argv[], const std::string& flag, const std::string& default_val = "") {
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == flag) {
            return argv[i + 1];
        }
    }
    return default_val;
}

bool has_arg(int argc, char* argv[], const std::string& flag) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == flag) return true;
    }
    return false;
}

void mine_mode(int argc, char* argv[]) {
    std::string address = get_arg(argc, argv, "--address", "");
    std::string energy_source = get_arg(argc, argv, "--energy", "cpu");

    if (address.empty()) {
        // Generate a new wallet for mining.
        Wallet wallet;
        wallet.generate_keys();
        address = wallet.get_address();
        std::cout << "No address specified. Generated new wallet:" << std::endl;
        std::cout << "  Address: " << address << std::endl;
        std::cout << "  (Save this address to receive your mining rewards)" << std::endl;
        std::cout << std::endl;
    }

    Blockchain chain;
    if (!chain.load_from_disk("bitvoid.chain")) {
        chain.init();
    }
    Mempool mempool;
    ConsensusEngine consensus;
    ConsensusParams params;

    uint64_t genesis_timestamp = chain.get_block(0)->header.timestamp;

    std::cout << "BitVoid Miner" << std::endl;
    std::cout << "Mining to address: " << address << std::endl;
    std::cout << "Energy source: " << energy_source << std::endl;
    std::cout << std::endl;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    uint64_t blocks_mined = 0;

    while (g_running) {
        Block latest = chain.get_latest_block();

        // Check if we should produce a block.
        if (!consensus.should_produce_block(mempool, latest.header.timestamp)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Get transactions from mempool for the block.
        std::vector<Transaction> txs = mempool.get_for_block(100);

        // Update difficulty based on recent blocks.
        std::vector<uint64_t> timestamps;
        uint64_t chain_h = chain.height();
        uint64_t lookback = (std::min)(chain_h, static_cast<uint64_t>(1000));
        for (uint64_t i = 0; i < lookback; i++) {
            auto block = chain.get_block(chain_h - lookback + i);
            if (block) timestamps.push_back(block->header.timestamp);
        }
        if (timestamps.size() >= 2) {
            params.difficulty = consensus.calculate_difficulty(timestamps, latest.header.difficulty);
        }

        // Mine the block.
        auto block = consensus.mine_block(latest, txs, address, energy_source, params, genesis_timestamp);

        if (block) {
            if (chain.add_block(*block)) {
                blocks_mined++;
                std::cout << "Block added to chain! Total mined: " << blocks_mined << std::endl;
                chain.save_to_disk("bitvoid.chain");

                // Remove confirmed transactions from mempool.
                for (const auto& tx : block->transactions) {
                    mempool.remove_transaction(tx.hash);
                }
            } else {
                std::cerr << "Block rejected by chain!" << std::endl;
            }
        }
    }

    std::cout << std::endl;
    std::cout << "Mining stopped." << std::endl;
    std::cout << "Total blocks mined: " << blocks_mined << std::endl;

    // Save chain.
    chain.save_to_disk("bitvoid.chain");
    std::cout << "Chain saved to bitvoid.chain" << std::endl;
}

void node_mode(int argc, char* argv[]) {
    uint16_t port = std::stoul(get_arg(argc, argv, "--port", "8333"));
    std::string connect_str = get_arg(argc, argv, "--connect", "");

    Blockchain chain;
    chain.init();
    Mempool mempool;
    ConsensusEngine consensus;

    Node node(port, chain, mempool, consensus);
    node.start();

    if (!connect_str.empty()) {
        // Parse host:port
        size_t colon = connect_str.find(':');
        if (colon != std::string::npos) {
            std::string host = connect_str.substr(0, colon);
            uint16_t peer_port = std::stoul(connect_str.substr(colon + 1));
            node.connect_to_peer(host, peer_port);
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "Node running. Press Ctrl+C to stop." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    node.stop();
    chain.save_to_disk("bitvoid.chain");
}

void wallet_mode(int argc, char* argv[]) {
    if (has_arg(argc, argv, "--new")) {
        std::string wallet_file = get_arg(argc, argv, "--wallet-file", "wallet.key");
        Wallet wallet;
        wallet.generate_keys();
        std::cout << "New wallet created!" << std::endl;
        std::cout << "  Address: " << wallet.get_address() << std::endl;
        std::cout << "  Public key: " << wallet.get_public_key_hex().substr(0, 40) << "..." << std::endl;
        if (wallet.save(wallet_file)) {
            std::cout << "  Saved to: " << wallet_file << std::endl;
            std::cout << "  Keep this file safe! You need it to spend your BTVD." << std::endl;
        } else {
            std::cerr << "  Failed to save wallet to " << wallet_file << std::endl;
        }
        return;
    }

    if (has_arg(argc, argv, "--balance")) {
        std::string address = get_arg(argc, argv, "--address", "");
        if (address.empty()) {
            std::cerr << "Please specify --address" << std::endl;
            return;
        }

        Blockchain chain;
        if (!chain.load_from_disk("bitvoid.chain")) {
            chain.init();
        }

        uint64_t balance = chain.get_balance(address);
        std::cout << "Balance: " << balance / 100000000.0 << " BTVD" << std::endl;
        std::cout << "  (" << balance << " base units)" << std::endl;
        return;
    }

    if (has_arg(argc, argv, "--send")) {
        std::string send_str = get_arg(argc, argv, "--send", "");
        size_t colon = send_str.find(':');
        if (colon == std::string::npos) {
            std::cerr << "Usage: --send AMOUNT:ADDRESS" << std::endl;
            return;
        }

        double amount_btvd = std::stod(send_str.substr(0, colon));
        std::string to_address = send_str.substr(colon + 1);
        uint64_t amount = static_cast<uint64_t>(amount_btvd * 100000000);

        std::string wallet_file = get_arg(argc, argv, "--wallet-file", "wallet.key");
        Wallet wallet;
        if (!wallet.load(wallet_file)) {
            std::cerr << "Failed to load wallet from " << wallet_file << std::endl;
            std::cerr << "Create one first with: bitvoid-core wallet --new" << std::endl;
            return;
        }
        std::string my_address = wallet.get_address();

        Blockchain chain;
        if (!chain.load_from_disk("bitvoid.chain")) {
            chain.init();
        }

        auto utxos = chain.get_utxos_for_address(my_address);
        if (utxos.empty()) {
            std::cerr << "No UTXOs found for your address." << std::endl;
            return;
        }

        Transaction tx = wallet.create_transaction(to_address, amount, 1000000, utxos);
        if (tx.hash.empty()) {
            std::cerr << "Failed to create transaction (insufficient funds?)" << std::endl;
            return;
        }

        std::cout << "Transaction created:" << std::endl;
        std::cout << "  Hash: " << tx.hash << std::endl;
        std::cout << "  To: " << to_address << std::endl;
        std::cout << "  Amount: " << amount_btvd << " BTVD" << std::endl;
        std::cout << std::endl;
        std::cout << "Add this transaction to mempool by running a node." << std::endl;
        return;
    }

    print_help();
}

void status_mode() {
    Blockchain chain;
    if (!chain.load_from_disk("bitvoid.chain")) {
        chain.init();
    }

    std::cout << "BitVoid Chain Status" << std::endl;
    std::cout << "  Height: " << chain.height() << std::endl;

    Block latest = chain.get_latest_block();
    std::cout << "  Latest block hash: " << latest.hash.substr(0, 32) << "..." << std::endl;
    std::cout << "  Latest block time: " << latest.header.timestamp << std::endl;
    std::cout << "  Difficulty: " << latest.header.difficulty << std::endl;
    std::cout << "  Energy (last block): " << latest.header.watt_seconds << " watt-seconds" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "--help") {
        print_help();
        return 0;
    }

    std::string mode = argv[1];

    if (mode == "mine") {
        mine_mode(argc, argv);
    } else if (mode == "node") {
        node_mode(argc, argv);
    } else if (mode == "wallet") {
        wallet_mode(argc, argv);
    } else if (mode == "status") {
        status_mode();
    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        print_help();
        return 1;
    }

    return 0;
}
