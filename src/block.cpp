#include "block.h"
#include "sha256.h"
#include <sstream>
#include <chrono>
#include <cmath>

namespace bitvoid {

std::string Block::serialize() const {
    std::stringstream ss;
    ss << header.index
       << header.previous_hash
       << header.timestamp
       << header.nonce
       << header.merkle_root
       << header.difficulty
       << header.miner_address;

    for (const auto& tx : transactions) {
        ss << tx.hash;
    }

    return ss.str();
}

std::string Block::calculate_hash() const {
    return double_sha256(serialize());
}

std::string Block::calculate_merkle_root() const {
    if (transactions.empty()) {
        return double_sha256("");
    }

    std::vector<std::string> layer;
    for (const auto& tx : transactions) {
        layer.push_back(tx.hash);
    }

    while (layer.size() > 1) {
        std::vector<std::string> next_layer;
        for (size_t i = 0; i < layer.size(); i += 2) {
            std::string combined = layer[i];
            if (i + 1 < layer.size()) {
                combined += layer[i + 1];
            } else {
                combined += layer[i]; // duplicate last if odd
            }
            next_layer.push_back(double_sha256(combined));
        }
        layer = next_layer;
    }

    return layer[0];
}

bool Block::meets_difficulty() const {
    // Check if hash has enough leading zero bits.
    auto bytes = hex_to_bytes(hash);
    uint32_t zero_bits = 0;

    for (auto byte : bytes) {
        if (byte == 0) {
            zero_bits += 8;
        } else {
            // Count leading zero bits in this byte.
            for (int bit = 7; bit >= 0; bit--) {
                if ((byte >> bit) & 1) {
                    return zero_bits >= header.difficulty;
                }
                zero_bits++;
            }
        }
    }

    return zero_bits >= header.difficulty;
}

nlohmann::json Block::to_json() const {
    nlohmann::json j;
    j["index"] = header.index;
    j["previous_hash"] = header.previous_hash;
    j["timestamp"] = header.timestamp;
    j["nonce"] = header.nonce;
    j["merkle_root"] = header.merkle_root;
    j["difficulty"] = header.difficulty;
    j["energy_kwh"] = header.energy_kwh;
    j["watt_seconds"] = header.watt_seconds;
    j["energy_source"] = header.energy_source;
    j["miner_address"] = header.miner_address;
    j["hash"] = hash;

    nlohmann::json txs = nlohmann::json::array();
    for (const auto& tx : transactions) {
        txs.push_back(tx.to_json());
    }
    j["transactions"] = txs;

    return j;
}

Block Block::from_json(const nlohmann::json& j) {
    Block block;
    block.header.index = j["index"].get<uint64_t>();
    block.header.previous_hash = j["previous_hash"].get<std::string>();
    block.header.timestamp = j["timestamp"].get<uint64_t>();
    block.header.nonce = j["nonce"].get<uint64_t>();
    block.header.merkle_root = j["merkle_root"].get<std::string>();
    block.header.difficulty = j["difficulty"].get<uint32_t>();
    block.header.energy_kwh = j["energy_kwh"].get<uint64_t>();
    block.header.watt_seconds = j["watt_seconds"].get<uint64_t>();
    block.header.energy_source = j["energy_source"].get<std::string>();
    block.header.miner_address = j["miner_address"].get<std::string>();
    block.hash = j["hash"].get<std::string>();

    for (const auto& tx : j["transactions"]) {
        block.transactions.push_back(Transaction::from_json(tx));
    }

    return block;
}

Block create_genesis_block() {
    Block genesis;
    genesis.header.index = 0;
    genesis.header.previous_hash = "0000000000000000000000000000000000000000000000000000000000000000";
    genesis.header.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    genesis.header.nonce = 0;
    genesis.header.difficulty = 1;
    genesis.header.energy_kwh = 0;
    genesis.header.watt_seconds = 0;
    genesis.header.energy_source = "genesis";
    genesis.header.miner_address = "genesis";
    genesis.header.merkle_root = double_sha256("");
    genesis.hash = genesis.calculate_hash();

    return genesis;
}

} // namespace bitvoid
