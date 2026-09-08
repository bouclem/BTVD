#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace bitvoid {

// UTXO model: transactions consume previous outputs and create new ones.

struct TxInput {
    std::string tx_id;        // hash of the transaction containing the output being spent
    uint32_t output_index;    // which output of that transaction
    std::string signature;   // signature proving ownership (hex)
    std::string public_key;  // public key for verification (hex)

    nlohmann::json to_json() const;
    static TxInput from_json(const nlohmann::json& j);
};

struct TxOutput {
    std::string address;     // recipient wallet address
    uint64_t amount;          // amount in base units (1 BTVD = 100,000,000)

    nlohmann::json to_json() const;
    static TxOutput from_json(const nlohmann::json& j);
};

struct Transaction {
    std::vector<TxInput> inputs;
    std::vector<TxOutput> outputs;
    uint64_t timestamp;       // unix timestamp
    uint64_t fee;              // fee paid to miner
    std::string hash;          // transaction ID (double SHA-256 of serialized tx)

    // Calculate transaction hash (ID).
    std::string calculate_hash() const;

    // Serialize for hashing (excludes signature fields to prevent circular hashing).
    std::string serialize() const;

    // Serialize full transaction including signatures.
    std::string serialize_full() const;

    // JSON serialization.
    nlohmann::json to_json() const;
    static Transaction from_json(const nlohmann::json& j);

    // Validate basic structure (non-empty, positive amounts).
    bool is_valid_structure() const;

    // Get total output amount.
    uint64_t total_output() const;

    // Get total input amount (requires looking up previous txs, done by caller).
    // This just sums what we know from the UTXO set passed in.
};

// UTXO entry: an unspent transaction output.
struct UTXO {
    std::string tx_id;
    uint32_t output_index;
    std::string address;
    uint64_t amount;

    nlohmann::json to_json() const;
    static UTXO from_json(const nlohmann::json& j);
};

} // namespace bitvoid
