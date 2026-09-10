#include "api.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace bitvoid {

ExplorerApi::ExplorerApi(Blockchain& chain, Mempool& mempool)
    : chain_(chain), mempool_(mempool) {}

std::string ExplorerApi::handle(const std::string& method, const std::string& path,
                                const std::string& query) {
    if (method != "GET" && method != "OPTIONS") {
        return R"({"error":"Method not allowed"})";
    }

    if (method == "OPTIONS") {
        return "{}";
    }

    // Route: /api/status
    if (path == "/api/status") {
        return get_status();
    }

    // Route: /api/blocks
    if (path == "/api/blocks") {
        return get_blocks(query);
    }

    // Route: /api/block/:height
    if (path.substr(0, 11) == "/api/block/") {
        std::string rest = path.substr(11);

        // Check if it's /api/block/hash/:hash
        if (rest.substr(0, 5) == "hash/") {
            return get_block_by_hash(rest.substr(5));
        }

        // Otherwise it's /api/block/:height
        try {
            uint64_t height = std::stoull(rest);
            return get_block_by_height(height);
        } catch (...) {
            return R"({"error":"Invalid block height"})";
        }
    }

    // Route: /api/tx/:hash
    if (path.substr(0, 8) == "/api/tx/") {
        return get_transaction(path.substr(8));
    }

    // Route: /api/address/:addr
    if (path.substr(0, 13) == "/api/address/") {
        return get_address(path.substr(13));
    }

    return R"({"error":"Not found"})";
}

std::string ExplorerApi::get_status() {
    Block latest = chain_.get_latest_block();

    nlohmann::json j;
    j["height"] = chain_.height();
    j["difficulty"] = latest.header.difficulty;
    j["latest_hash"] = latest.hash;
    j["latest_timestamp"] = latest.header.timestamp;
    j["latest_energy"] = latest.header.watt_seconds;
    j["latest_energy_kwh"] = latest.header.energy_kwh;
    j["mempool_size"] = mempool_.size();

    return j.dump();
}

std::string ExplorerApi::get_blocks(const std::string& query) {
    std::string limit_str = get_query_param(query, "limit");
    size_t limit = 20;
    if (!limit_str.empty()) {
        try {
            limit = std::stoul(limit_str);
            if (limit > 1000) limit = 1000;
        } catch (...) {}
    }

    uint64_t height = chain_.height();
    size_t count = 0;
    nlohmann::json blocks = nlohmann::json::array();

    // Iterate from latest backwards.
    for (uint64_t i = height + 1; i > 0 && count < limit; i--) {
        auto block = chain_.get_block(i - 1);
        if (!block) break;

        nlohmann::json b;
        b["index"] = block->header.index;
        b["hash"] = block->hash;
        b["timestamp"] = block->header.timestamp;
        b["difficulty"] = block->header.difficulty;
        b["energy_kwh"] = block->header.energy_kwh;
        b["watt_seconds"] = block->header.watt_seconds;
        b["energy_source"] = block->header.energy_source;
        b["miner_address"] = block->header.miner_address;
        b["tx_count"] = block->transactions.size();

        blocks.push_back(b);
        count++;
    }

    return blocks.dump();
}

std::string ExplorerApi::get_block_by_height(uint64_t height) {
    auto block = chain_.get_block(height);
    if (!block) {
        return R"({"error":"Block not found"})";
    }
    return block->to_json().dump();
}

std::string ExplorerApi::get_block_by_hash(const std::string& hash) {
    auto block = chain_.get_block_by_hash(hash);
    if (!block) {
        return R"({"error":"Block not found"})";
    }
    return block->to_json().dump();
}

std::string ExplorerApi::get_transaction(const std::string& hash) {
    // Search all blocks for this transaction.
    uint64_t height = chain_.height();
    for (uint64_t i = 0; i <= height; i++) {
        auto block = chain_.get_block(i);
        if (!block) continue;

        for (const auto& tx : block->transactions) {
            if (tx.hash == hash) {
                nlohmann::json j = tx.to_json();
                j["block_height"] = i;
                j["block_hash"] = block->hash;
                return j.dump();
            }
        }
    }

    // Also check mempool.
    if (mempool_.contains(hash)) {
        auto all = mempool_.get_all();
        for (const auto& tx : all) {
            if (tx.hash == hash) {
                nlohmann::json j = tx.to_json();
                j["mempool"] = true;
                return j.dump();
            }
        }
    }

    return R"({"error":"Transaction not found"})";
}

std::string ExplorerApi::get_address(const std::string& address) {
    nlohmann::json j;
    j["address"] = address;
    j["balance"] = chain_.get_balance(address);

    auto utxos = chain_.get_utxos_for_address(address);
    nlohmann::json utxo_array = nlohmann::json::array();
    for (const auto& u : utxos) {
        utxo_array.push_back(u.to_json());
    }
    j["utxos"] = utxo_array;

    return j.dump();
}

std::string ExplorerApi::get_query_param(const std::string& query, const std::string& key) const {
    std::string search = key + "=";
    size_t pos = query.find(search);
    if (pos == std::string::npos) return "";

    pos += search.size();
    size_t end = query.find('&', pos);
    if (end == std::string::npos) {
        return query.substr(pos);
    }
    return query.substr(pos, end - pos);
}

} // namespace bitvoid
