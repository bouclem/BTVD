#include "blockchain.h"
#include "sha256.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace bitvoid {

std::string Blockchain::utxo_key(const std::string& tx_id, uint32_t output_index) {
    return tx_id + ":" + std::to_string(output_index);
}

Blockchain::Blockchain() {
    // Chain starts empty. Call init() to create genesis.
}

void Blockchain::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!chain_.empty()) return;

    Block genesis = create_genesis_block();
    chain_.push_back(genesis);
    hash_index_[genesis.hash] = 0;

    // Genesis has no transactions, so no UTXOs to add.
}

bool Blockchain::add_block(const Block& block) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Must connect to latest block.
    if (chain_.empty()) {
        // First block must be genesis.
        chain_.push_back(block);
        hash_index_[block.hash] = 0;
        apply_block_to_utxo_set(block);
        return true;
    }

    const Block& latest = chain_.back();

    // Check index is next.
    if (block.header.index != latest.header.index + 1) {
        return false;
    }

    // Check previous hash matches.
    if (block.header.previous_hash != latest.hash) {
        return false;
    }

    // Verify hash.
    std::string computed_hash = block.calculate_hash();
    if (computed_hash != block.hash) {
        return false;
    }

    // Check difficulty.
    if (!block.meets_difficulty()) {
        return false;
    }

    // Verify merkle root.
    std::string computed_merkle = block.calculate_merkle_root();
    if (computed_merkle != block.header.merkle_root) {
        return false;
    }

    // Validate all transactions.
    for (const auto& tx : block.transactions) {
        if (!tx.is_valid_structure()) {
            return false;
        }
        // Verify tx hash.
        if (tx.calculate_hash() != tx.hash) {
            return false;
        }
    }

    chain_.push_back(block);
    hash_index_[block.hash] = block.header.index;
    apply_block_to_utxo_set(block);

    return true;
}

void Blockchain::apply_block_to_utxo_set(const Block& block) {
    // Remove spent UTXOs (from inputs).
    for (const auto& tx : block.transactions) {
        for (const auto& in : tx.inputs) {
            std::string key = utxo_key(in.tx_id, in.output_index);
            utxo_set_.erase(key);
        }
    }

    // Add new UTXOs (from outputs).
    for (const auto& tx : block.transactions) {
        for (uint32_t i = 0; i < tx.outputs.size(); i++) {
            std::string key = utxo_key(tx.hash, i);
            utxo_set_[key] = UTXO{
                tx.hash,
                i,
                tx.outputs[i].address,
                tx.outputs[i].amount
            };
        }
    }
}

Block Blockchain::get_latest_block() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (chain_.empty()) {
        return create_genesis_block();
    }
    return chain_.back();
}

std::optional<Block> Blockchain::get_block(uint64_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= chain_.size()) return std::nullopt;
    return chain_[index];
}

std::optional<Block> Blockchain::get_block_by_hash(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = hash_index_.find(hash);
    if (it == hash_index_.end()) return std::nullopt;
    return chain_[it->second];
}

uint64_t Blockchain::height() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return chain_.empty() ? 0 : chain_.size() - 1;
}

bool Blockchain::validate_block(const Block& block) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (chain_.empty()) return false;

    const Block& latest = chain_.back();

    if (block.header.index != latest.header.index + 1) return false;
    if (block.header.previous_hash != latest.hash) return false;

    if (block.calculate_hash() != block.hash) return false;
    if (!block.meets_difficulty()) return false;
    if (block.calculate_merkle_root() != block.header.merkle_root) return false;

    return true;
}

bool Blockchain::validate_transaction(const Transaction& tx) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!tx.is_valid_structure()) return false;
    if (tx.calculate_hash() != tx.hash) return false;

    // Check all inputs exist in UTXO set.
    for (const auto& in : tx.inputs) {
        std::string key = utxo_key(in.tx_id, in.output_index);
        if (!utxo_set_.count(key)) return false;
    }

    return true;
}

std::vector<UTXO> Blockchain::get_utxos_for_address(const std::string& address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UTXO> result;
    for (const auto& [key, utxo] : utxo_set_) {
        if (utxo.address == address) {
            result.push_back(utxo);
        }
    }
    return result;
}

uint64_t Blockchain::get_balance(const std::string& address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    for (const auto& [key, utxo] : utxo_set_) {
        if (utxo.address == address) {
            total += utxo.amount;
        }
    }
    return total;
}

bool Blockchain::has_utxo(const std::string& tx_id, uint32_t output_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return utxo_set_.count(utxo_key(tx_id, output_index)) > 0;
}

std::optional<UTXO> Blockchain::get_utxo(const std::string& tx_id, uint32_t output_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = utxo_set_.find(utxo_key(tx_id, output_index));
    if (it == utxo_set_.end()) return std::nullopt;
    return it->second;
}

bool Blockchain::save_to_disk(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json j = nlohmann::json::array();
    for (const auto& block : chain_) {
        j.push_back(block.to_json());
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

bool Blockchain::load_from_disk(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(path);
    if (!file.is_open()) return false;

    nlohmann::json j;
    file >> j;

    chain_.clear();
    utxo_set_.clear();
    hash_index_.clear();

    for (const auto& block_json : j) {
        Block block = Block::from_json(block_json);
        chain_.push_back(block);
        hash_index_[block.hash] = block.header.index;
    }

    rebuild_utxo_set();
    return true;
}

void Blockchain::rebuild_utxo_set() {
    utxo_set_.clear();
    for (const auto& block : chain_) {
        apply_block_to_utxo_set(block);
    }
}

std::vector<Block> Blockchain::get_blocks(uint64_t from_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Block> result;
    for (uint64_t i = from_index; i < chain_.size(); i++) {
        result.push_back(chain_[i]);
    }
    return result;
}

} // namespace bitvoid
