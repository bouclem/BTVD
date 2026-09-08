#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "transaction.h"

namespace bitvoid {

// 1 BTVD = 100,000,000 units (like Bitcoin's satoshi)
constexpr uint64_t COIN = 100000000;

struct BlockHeader {
    uint64_t index;              // block height
    std::string previous_hash;   // hash of previous block
    uint64_t timestamp;          // unix timestamp (seconds)
    uint64_t nonce;              // mining nonce
    std::string merkle_root;     // root of transaction Merkle tree
    uint32_t difficulty;         // current difficulty target

    // PoE specific fields
    uint64_t energy_kwh;         // kWh consumed to mine this block
    uint64_t watt_seconds;       // precise energy measurement
    std::string energy_source;   // declared source (solar, hydro, wind, etc.)
    std::string miner_address;   // miner's wallet address
};

struct Block {
    BlockHeader header;
    std::vector<Transaction> transactions;
    std::string hash;            // this block's hash

    // Calculate hash of the block header.
    std::string calculate_hash() const;

    // Calculate Merkle root of all transactions.
    std::string calculate_merkle_root() const;

    // Serialize to JSON for storage/transmission.
    nlohmann::json to_json() const;
    static Block from_json(const nlohmann::json& j);

    // Serialize to string for hashing.
    std::string serialize() const;

    // Validate the block hash meets difficulty.
    bool meets_difficulty() const;
};

// Create genesis block.
Block create_genesis_block();

} // namespace bitvoid
