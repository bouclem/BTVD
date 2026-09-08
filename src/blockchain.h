#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include "block.h"
#include "transaction.h"

namespace bitvoid {

// Blockchain: manages the chain of blocks and the UTXO set.
class Blockchain {
public:
    Blockchain();

    // Initialize with genesis block.
    void init();

    // Add a block to the chain. Returns false if invalid.
    bool add_block(const Block& block);

    // Get the latest block.
    Block get_latest_block() const;

    // Get block by index.
    std::optional<Block> get_block(uint64_t index) const;

    // Get block by hash.
    std::optional<Block> get_block_by_hash(const std::string& hash) const;

    // Get current chain height.
    uint64_t height() const;

    // Validate a block against the current chain state.
    bool validate_block(const Block& block) const;

    // Validate a transaction against the UTXO set.
    bool validate_transaction(const Transaction& tx) const;

    // Get UTXO set for a given address.
    std::vector<UTXO> get_utxos_for_address(const std::string& address) const;

    // Get balance for an address.
    uint64_t get_balance(const std::string& address) const;

    // Check if a UTXO exists.
    bool has_utxo(const std::string& tx_id, uint32_t output_index) const;

    // Get the UTXO for a specific output.
    std::optional<UTXO> get_utxo(const std::string& tx_id, uint32_t output_index) const;

    // Save/load chain to/from disk.
    bool save_to_disk(const std::string& path) const;
    bool load_from_disk(const std::string& path);

    // Get all blocks (for sync).
    std::vector<Block> get_blocks(uint64_t from_index) const;

private:
    mutable std::mutex mutex_;
    std::vector<Block> chain_;

    // UTXO set: key = "tx_id:output_index", value = UTXO.
    std::unordered_map<std::string, UTXO> utxo_set_;

    // Index: block hash -> block index in chain.
    std::unordered_map<std::string, uint64_t> hash_index_;

    // Update UTXO set when a block is added.
    void apply_block_to_utxo_set(const Block& block);

    // Rebuild UTXO set from scratch (for validation/loading).
    void rebuild_utxo_set();

    // UTXO key helper.
    static std::string utxo_key(const std::string& tx_id, uint32_t output_index);
};

} // namespace bitvoid
