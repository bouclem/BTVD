#include "consensus.h"
#include "sha256.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>

namespace bitvoid {

ConsensusEngine::ConsensusEngine(ConsensusParams params)
    : params_(std::move(params)) {}

uint64_t ConsensusParams::get_block_reward(uint64_t block_index, uint64_t genesis_timestamp) const {
    // Calculate years since genesis.
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    uint64_t elapsed = now - genesis_timestamp;
    uint64_t seconds_per_year = 365 * 24 * 60 * 60;
    uint64_t years = elapsed / seconds_per_year;

    // Ramped emission:
    // Year 1: 0.5 BTVD/block, Year 2: 1, Year 3: 1.5, Year 4+: 2 forever.
    uint64_t reward_btvd;
    if (years == 0) {
        reward_btvd = 0.5 * COIN;
    } else if (years == 1) {
        reward_btvd = 1 * COIN;
    } else if (years == 2) {
        reward_btvd = 1.5 * COIN;
    } else {
        reward_btvd = 2 * COIN;
    }

    return reward_btvd;
}

bool ConsensusEngine::should_produce_block(const Mempool& mempool, uint64_t last_block_timestamp) const {
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Safety floor: must produce block if max gap exceeded.
    if (now - last_block_timestamp >= params_.max_block_gap_sec) {
        return true;
    }

    // Minimum block gap: don't produce too fast.
    if (now - last_block_timestamp < params_.min_block_gap_sec) {
        return false;
    }

    // Event-driven triggers.
    if (mempool.size() >= params_.tx_count_threshold) {
        return true;
    }

    if (mempool.total_pending_value() >= params_.value_threshold) {
        return true;
    }

    return false;
}

uint32_t ConsensusEngine::calculate_difficulty(const std::vector<uint64_t>& recent_block_timestamps) const {
    if (recent_block_timestamps.size() < 2) {
        return params_.initial_difficulty;
    }

    // Calculate average block time over recent blocks.
    uint64_t total_time = recent_block_timestamps.back() - recent_block_timestamps.front();
    uint64_t num_blocks = recent_block_timestamps.size() - 1;
    double avg_block_time = static_cast<double>(total_time) / num_blocks;

    // Compare to target.
    double ratio = avg_block_time / static_cast<double>(params_.target_block_time_sec);

    // If blocks are too fast (ratio < 0.8), increase difficulty.
    // If blocks are too slow (ratio > 1.2), decrease difficulty.
    // Otherwise keep the same.

    // This is a simplified adjustment. Real implementation would use a
    // more precise algorithm similar to Bitcoin's, but adapted for
    // event-driven blocks.

    uint32_t current = params_.initial_difficulty;

    if (ratio < 0.8) {
        // Blocks too fast, increase difficulty.
        uint32_t increase = std::max(1u, static_cast<uint32_t>(current * (1.0 - ratio) * 0.5));
        return current + increase;
    } else if (ratio > 1.2) {
        // Blocks too slow, decrease difficulty.
        if (current <= 1) return 1;
        uint32_t decrease = std::max(1u, static_cast<uint32_t>(current * (ratio - 1.0) * 0.5));
        return (current > decrease) ? current - decrease : 1;
    }

    return current;
}

ConsensusEngine::EnergyReport ConsensusEngine::estimate_energy(double elapsed_sec, double estimated_watts) const {
    EnergyReport report;
    report.elapsed_sec = elapsed_sec;
    report.estimated_watts = estimated_watts;
    report.watt_seconds = static_cast<uint64_t>(elapsed_sec * estimated_watts);
    report.kwh = report.watt_seconds / 3600000; // 1 kWh = 3,600,000 watt-seconds
    return report;
}

std::optional<Block> ConsensusEngine::mine_block(
    const Block& previous_block,
    const std::vector<Transaction>& transactions,
    const std::string& miner_address,
    const std::string& energy_source,
    const ConsensusParams& params,
    uint64_t genesis_timestamp
) {
    Block new_block;
    new_block.header.index = previous_block.header.index + 1;
    new_block.header.previous_hash = previous_block.hash;
    new_block.header.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    new_block.header.difficulty = params.difficulty;
    new_block.header.energy_source = energy_source;
    new_block.header.miner_address = miner_address;
    new_block.transactions = transactions;

    // Calculate merkle root.
    new_block.header.merkle_root = new_block.calculate_merkle_root();

    // Get block reward and create coinbase transaction (reward to miner).
    uint64_t reward = params.get_block_reward(new_block.header.index, genesis_timestamp);

    // Add fees from transactions.
    uint64_t total_fees = 0;
    for (const auto& tx : transactions) {
        total_fees += tx.fee;
    }

    // Create coinbase transaction (miner reward + fees).
    Transaction coinbase;
    coinbase.timestamp = new_block.header.timestamp;
    coinbase.fee = 0;
    // Coinbase has no inputs (it's the reward).
    TxInput fake_input;
    fake_input.tx_id = "0000000000000000000000000000000000000000000000000000000000000000";
    fake_input.output_index = 0;
    fake_input.signature = "coinbase";
    fake_input.public_key = "coinbase";
    coinbase.inputs.push_back(fake_input);

    TxOutput reward_output;
    reward_output.address = miner_address;
    reward_output.amount = reward + total_fees;
    coinbase.outputs.push_back(reward_output);
    coinbase.hash = coinbase.calculate_hash();

    // Insert coinbase as first transaction.
    new_block.transactions.insert(new_block.transactions.begin(), coinbase);

    // Recalculate merkle root with coinbase included.
    new_block.header.merkle_root = new_block.calculate_merkle_root();

    // Mining loop: find nonce that produces hash with enough leading zero bits.
    auto start_time = std::chrono::steady_clock::now();

    // Estimate CPU power draw (simplified: assume ~65W for typical CPU under load).
    double estimated_watts = 65.0;

    std::cout << "Mining block " << new_block.header.index << "..." << std::endl;
    std::cout << "Difficulty: " << params.difficulty << " leading zero bits" << std::endl;
    std::cout << "Reward: " << (reward + total_fees) / 100000000.0 << " BTVD" << std::endl;

    uint64_t nonce = 0;
    const uint64_t max_nonce = UINT64_MAX;

    while (nonce < max_nonce) {
        new_block.header.nonce = nonce;
        new_block.hash = new_block.calculate_hash();

        if (new_block.meets_difficulty()) {
            // Found a valid block!
            auto end_time = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(end_time - start_time).count();

            // Record energy consumption.
            EnergyReport energy = estimate_energy(elapsed, estimated_watts);
            new_block.header.watt_seconds = energy.watt_seconds;
            new_block.header.energy_kwh = energy.kwh;

            std::cout << "Block mined!" << std::endl;
            std::cout << "  Hash: " << new_block.hash << std::endl;
            std::cout << "  Nonce: " << nonce << std::endl;
            std::cout << "  Time: " << elapsed << " seconds" << std::endl;
            std::cout << "  Energy: " << energy.watt_seconds << " watt-seconds ("
                      << energy.kwh << " kWh)" << std::endl;

            return new_block;
        }

        nonce++;

        // Print progress every 1M hashes.
        if (nonce % 1000000 == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            double hash_rate = nonce / elapsed / 1000000.0;
            std::cout << "  " << nonce / 1000000 << "M hashes, "
                      << hash_rate << " MH/s, "
                      << elapsed << "s elapsed" << std::endl;
        }
    }

    // Should never reach here in practice.
    return std::nullopt;
}

uint64_t ConsensusEngine::get_current_reward(uint64_t genesis_timestamp) const {
    return params_.get_block_reward(0, genesis_timestamp);
}

} // namespace bitvoid
