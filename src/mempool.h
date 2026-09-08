#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "transaction.h"

namespace bitvoid {

// Mempool: holds pending transactions waiting to be included in a block.
class Mempool {
public:
    // Add a transaction to the mempool.
    bool add_transaction(const Transaction& tx);

    // Remove a transaction by hash.
    void remove_transaction(const std::string& tx_hash);

    // Get all pending transactions.
    std::vector<Transaction> get_all() const;

    // Get up to N transactions for a block (sorted by fee, highest first).
    std::vector<Transaction> get_for_block(size_t max_count) const;

    // Get total pending transaction count.
    size_t size() const;

    // Get total pending value (sum of all output amounts + fees).
    uint64_t total_pending_value() const;

    // Check if a transaction exists in the mempool.
    bool contains(const std::string& tx_hash) const;

    // Clear all transactions.
    void clear();

    // Check if any of these tx inputs are already in the mempool (double-spend check).
    bool has_conflicting_inputs(const std::vector<TxInput>& inputs) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Transaction> transactions_;

    // Track which (tx_id, output_index) pairs are being spent in the mempool.
    std::unordered_map<std::string, uint32_t> spent_in_mempool_;
};

} // namespace bitvoid
