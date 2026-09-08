#include "mempool.h"
#include <algorithm>
#include <sstream>

namespace bitvoid {

bool Mempool::add_transaction(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (transactions_.count(tx.hash)) {
        return false; // already in mempool
    }

    // Check for conflicting inputs (double-spend within mempool).
    for (const auto& in : tx.inputs) {
        std::string key = in.tx_id + ":" + std::to_string(in.output_index);
        if (spent_in_mempool_.count(key)) {
            return false; // input already being spent by another pending tx
        }
    }

    // Add to mempool.
    transactions_[tx.hash] = tx;

    // Mark inputs as spent in mempool.
    for (const auto& in : tx.inputs) {
        std::string key = in.tx_id + ":" + std::to_string(in.output_index);
        spent_in_mempool_[key] = in.output_index;
    }

    return true;
}

void Mempool::remove_transaction(const std::string& tx_hash) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = transactions_.find(tx_hash);
    if (it == transactions_.end()) return;

    // Unmark inputs.
    for (const auto& in : it->second.inputs) {
        std::string key = in.tx_id + ":" + std::to_string(in.output_index);
        spent_in_mempool_.erase(key);
    }

    transactions_.erase(it);
}

std::vector<Transaction> Mempool::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Transaction> result;
    result.reserve(transactions_.size());
    for (const auto& [hash, tx] : transactions_) {
        result.push_back(tx);
    }
    return result;
}

std::vector<Transaction> Mempool::get_for_block(size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Sort by fee (highest first) for block inclusion.
    std::vector<Transaction> sorted;
    sorted.reserve(transactions_.size());
    for (const auto& [hash, tx] : transactions_) {
        sorted.push_back(tx);
    }

    std::sort(sorted.begin(), sorted.end(), [](const Transaction& a, const Transaction& b) {
        return a.fee > b.fee;
    });

    if (sorted.size() > max_count) {
        sorted.resize(max_count);
    }

    return sorted;
}

size_t Mempool::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transactions_.size();
}

uint64_t Mempool::total_pending_value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    for (const auto& [hash, tx] : transactions_) {
        total += tx.total_output() + tx.fee;
    }
    return total;
}

bool Mempool::contains(const std::string& tx_hash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transactions_.count(tx_hash) > 0;
}

void Mempool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    transactions_.clear();
    spent_in_mempool_.clear();
}

bool Mempool::has_conflicting_inputs(const std::vector<TxInput>& inputs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& in : inputs) {
        std::string key = in.tx_id + ":" + std::to_string(in.output_index);
        if (spent_in_mempool_.count(key)) {
            return true;
        }
    }
    return false;
}

} // namespace bitvoid
