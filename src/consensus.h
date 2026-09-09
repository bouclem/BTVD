#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include "block.h"
#include "mempool.h"

namespace bitvoid {

// Consensus parameters for BitVoid.
struct ConsensusParams {
    // Event-driven block triggers.
    uint32_t tx_count_threshold = 100;       // mine block when this many TXs pending
    uint64_t value_threshold = 10000000000;  // mine block when this much value pending (100 BTVD in base units)

    // Block timing.
    uint64_t min_block_gap_sec = 3;           // minimum seconds between blocks
    uint64_t max_block_gap_sec = 60;          // maximum seconds before forced block

    // Difficulty.
    uint32_t initial_difficulty = 1;           // starting difficulty (number of leading zero bits)
    uint32_t difficulty = 1;                   // current difficulty (adjusted during mining)
    uint32_t difficulty_window = 1000;         // blocks to look back for adjustment
    uint64_t target_block_time_sec = 5;        // target average block time

    // Block size.
    uint32_t max_block_size_bytes = 2000000;   // 2MB

    // Mining reward schedule (ramped emission, infinite supply).
    // Year 1: 0.5 BTVD/block, Year 2: 1, Year 3: 1.5, Year 4+: 2 forever.
    uint64_t get_block_reward(uint64_t block_index, uint64_t genesis_timestamp) const;
};

// Consensus engine: handles PoE mining and event-driven block production.
class ConsensusEngine {
public:
    ConsensusEngine(ConsensusParams params = ConsensusParams{});

    // Check if we should produce a block now (event-driven triggers).
    bool should_produce_block(const Mempool& mempool, uint64_t last_block_timestamp) const;

    // Calculate current difficulty based on recent block times.
    // current_difficulty: the difficulty of the latest block (adjusts from this, not from initial).
    uint32_t calculate_difficulty(const std::vector<uint64_t>& recent_block_timestamps, uint32_t current_difficulty) const;

    // Mine a block: find a nonce that meets difficulty.
    // Records energy consumption during mining.
    // Returns the mined block, or empty optional if cancelled.
    std::optional<Block> mine_block(
        const Block& previous_block,
        const std::vector<Transaction>& transactions,
        const std::string& miner_address,
        const std::string& energy_source,
        const ConsensusParams& params,
        uint64_t genesis_timestamp
    );

    // Get current block reward based on time since genesis.
    uint64_t get_current_reward(uint64_t genesis_timestamp) const;

    // Estimate energy consumption for a mining session.
    // This is a simplified model: assumes CPU power draw in watts.
    struct EnergyReport {
        uint64_t watt_seconds;   // total energy in watt-seconds
        uint64_t kwh;            // total energy in kWh
        double elapsed_sec;      // time spent mining
        double estimated_watts;  // estimated power draw
    };

    // Measure energy during mining (simplified: uses time * estimated CPU power).
    EnergyReport estimate_energy(double elapsed_sec, double estimated_watts) const;

private:
    ConsensusParams params_;
};

} // namespace bitvoid
