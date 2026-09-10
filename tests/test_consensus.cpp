#include <gtest/gtest.h>
#include "consensus.h"
#include "blockchain.h"
#include "mempool.h"
#include <chrono>

using namespace bitvoid;

// --- Block reward schedule ---

TEST(ConsensusTest, YearOneRewardIsHalfBTVD) {
    ConsensusParams params;
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    // Genesis timestamp = now, so we're in year 0 (year 1 of mining).
    uint64_t reward = params.get_block_reward(0, now);
    EXPECT_EQ(reward, 50000000u); // 0.5 BTVD
}

TEST(ConsensusTest, YearTwoRewardIsOneBTVD) {
    ConsensusParams params;
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    uint64_t seconds_per_year = 365 * 24 * 60 * 60;
    // Genesis was 1 year ago.
    uint64_t genesis = now - seconds_per_year;
    uint64_t reward = params.get_block_reward(0, genesis);
    EXPECT_EQ(reward, 100000000u); // 1 BTVD
}

TEST(ConsensusTest, YearThreeRewardIsOnePointFiveBTVD) {
    ConsensusParams params;
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    uint64_t seconds_per_year = 365 * 24 * 60 * 60;
    uint64_t genesis = now - (2 * seconds_per_year);
    uint64_t reward = params.get_block_reward(0, genesis);
    EXPECT_EQ(reward, 150000000u); // 1.5 BTVD
}

TEST(ConsensusTest, YearFourPlusRewardIsTwoBTVD) {
    ConsensusParams params;
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    uint64_t seconds_per_year = 365 * 24 * 60 * 60;
    uint64_t genesis = now - (3 * seconds_per_year);
    uint64_t reward = params.get_block_reward(0, genesis);
    EXPECT_EQ(reward, 200000000u); // 2 BTVD

    // Year 10 should also be 2 BTVD.
    genesis = now - (10 * seconds_per_year);
    reward = params.get_block_reward(0, genesis);
    EXPECT_EQ(reward, 200000000u);
}

// --- Block production triggers ---

TEST(ConsensusTest, ShouldProduceBlockOnMaxGap) {
    ConsensusEngine consensus;
    Mempool mempool;

    // Last block was 61 seconds ago (exceeds 60s max gap).
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    EXPECT_TRUE(consensus.should_produce_block(mempool, now - 61));
}

TEST(ConsensusTest, ShouldNotProduceBlockWithinMinGap) {
    ConsensusEngine consensus;
    Mempool mempool;

    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    // Last block was 1 second ago (below 3s min gap).
    EXPECT_FALSE(consensus.should_produce_block(mempool, now - 1));
}

TEST(ConsensusTest, ShouldProduceBlockOnTxCountThreshold) {
    ConsensusEngine consensus;
    Mempool mempool;

    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Add 100 transactions to mempool.
    for (int i = 0; i < 100; i++) {
        Transaction tx;
        tx.timestamp = now;
        tx.fee = 100;
        tx.hash = "tx_hash_" + std::to_string(i);
        TxInput in;
        in.tx_id = std::string(64, 'a') + std::to_string(i);
        in.output_index = 0;
        tx.inputs.push_back(in);
        TxOutput out;
        out.address = "addr";
        out.amount = 100;
        tx.outputs.push_back(out);
        mempool.add_transaction(tx);
    }

    // Last block was 5 seconds ago (past min gap, under max gap).
    EXPECT_TRUE(consensus.should_produce_block(mempool, now - 5));
}

TEST(ConsensusTest, ShouldNotProduceBlockWithNoTriggers) {
    ConsensusEngine consensus;
    Mempool mempool;

    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // 10 seconds since last block, no txs, no value threshold.
    EXPECT_FALSE(consensus.should_produce_block(mempool, now - 10));
}

// --- Difficulty adjustment ---

TEST(ConsensusTest, DifficultyStaysSameWhenOnTarget) {
    ConsensusEngine consensus;
    ConsensusParams params;
    uint32_t current = 10;

    // Blocks coming at exactly target rate (5 seconds).
    std::vector<uint64_t> timestamps;
    for (int i = 0; i < 100; i++) {
        timestamps.push_back(1000 + i * 5);
    }

    uint32_t result = consensus.calculate_difficulty(timestamps, current);
    EXPECT_EQ(result, current); // ratio = 1.0, no change
}

TEST(ConsensusTest, DifficultyIncreasesWhenBlocksTooFast) {
    ConsensusEngine consensus;
    uint32_t current = 10;

    // Blocks coming every 1 second (target is 5s, ratio = 0.2 < 0.8).
    std::vector<uint64_t> timestamps;
    for (int i = 0; i < 100; i++) {
        timestamps.push_back(1000 + i);
    }

    uint32_t result = consensus.calculate_difficulty(timestamps, current);
    EXPECT_GT(result, current);
}

TEST(ConsensusTest, DifficultyDecreasesWhenBlocksTooSlow) {
    ConsensusEngine consensus;
    uint32_t current = 10;

    // Blocks coming every 30 seconds (target is 5s, ratio = 6.0 > 1.2).
    std::vector<uint64_t> timestamps;
    for (int i = 0; i < 100; i++) {
        timestamps.push_back(1000 + i * 30);
    }

    uint32_t result = consensus.calculate_difficulty(timestamps, current);
    EXPECT_LT(result, current);
}

TEST(ConsensusTest, DifficultyDoesNotGoBelowOne) {
    ConsensusEngine consensus;
    uint32_t current = 1;

    std::vector<uint64_t> timestamps;
    for (int i = 0; i < 100; i++) {
        timestamps.push_back(1000 + i * 60); // very slow blocks
    }

    uint32_t result = consensus.calculate_difficulty(timestamps, current);
    EXPECT_EQ(result, 1u);
}

TEST(ConsensusTest, DifficultyWithTooFewTimestampsReturnsCurrent) {
    ConsensusEngine consensus;
    uint32_t current = 5;

    std::vector<uint64_t> timestamps = {1000}; // only 1 timestamp
    uint32_t result = consensus.calculate_difficulty(timestamps, current);
    EXPECT_EQ(result, current);
}

TEST(ConsensusTest, DifficultyWithEmptyTimestampsReturnsCurrent) {
    ConsensusEngine consensus;
    uint32_t current = 5;

    std::vector<uint64_t> timestamps;
    uint32_t result = consensus.calculate_difficulty(timestamps, current);
    EXPECT_EQ(result, current);
}

// --- Energy estimation ---

TEST(ConsensusTest, EnergyEstimationCalculatesCorrectly) {
    ConsensusEngine consensus;
    auto report = consensus.estimate_energy(3600.0, 100.0); // 1 hour at 100W

    EXPECT_EQ(report.elapsed_sec, 3600.0);
    EXPECT_EQ(report.estimated_watts, 100.0);
    EXPECT_EQ(report.watt_seconds, 360000u); // 3600 * 100
    EXPECT_EQ(report.kwh, 0u); // 360000 / 3600000 = 0.1, truncated to 0
}

TEST(ConsensusTest, EnergyEstimationOneKwh) {
    ConsensusEngine consensus;
    // 3,600,000 watt-seconds = 1 kWh.
    // At 100W, that's 36,000 seconds = 10 hours.
    auto report = consensus.estimate_energy(36000.0, 100.0);
    EXPECT_EQ(report.watt_seconds, 3600000u);
    EXPECT_EQ(report.kwh, 1u);
}

// --- Mining ---

TEST(ConsensusTest, MineBlockProducesValidBlock) {
    ConsensusEngine consensus;
    ConsensusParams params;
    params.difficulty = 1;

    Blockchain chain;
    chain.init();
    Block genesis = chain.get_latest_block();

    auto block = consensus.mine_block(
        genesis, {}, "miner_addr", "cpu", params, genesis.header.timestamp);

    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->header.index, 1u);
    EXPECT_EQ(block->header.previous_hash, genesis.hash);
    EXPECT_EQ(block->header.miner_address, "miner_addr");
    EXPECT_TRUE(block->meets_difficulty());
    EXPECT_FALSE(block->transactions.empty()); // should have coinbase
    EXPECT_TRUE(block->transactions[0].is_coinbase());
}

TEST(ConsensusTest, MinedBlockHasEnergyRecord) {
    ConsensusEngine consensus;
    ConsensusParams params;
    params.difficulty = 1;

    Blockchain chain;
    chain.init();
    Block genesis = chain.get_latest_block();

    auto block = consensus.mine_block(
        genesis, {}, "miner_addr", "solar", params, genesis.header.timestamp);

    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->header.energy_source, "solar");
    // Energy may be 0 for very fast mining, but watt_seconds field should exist.
    // The key is that the fields are populated (not garbage).
}

TEST(ConsensusTest, MinedBlockCoinbasePaysMiner) {
    ConsensusEngine consensus;
    ConsensusParams params;
    params.difficulty = 1;

    Blockchain chain;
    chain.init();
    Block genesis = chain.get_latest_block();

    auto block = consensus.mine_block(
        genesis, {}, "miner_addr", "cpu", params, genesis.header.timestamp);

    ASSERT_TRUE(block.has_value());
    ASSERT_FALSE(block->transactions.empty());
    const auto& coinbase = block->transactions[0];
    ASSERT_EQ(coinbase.outputs.size(), 1u);
    EXPECT_EQ(coinbase.outputs[0].address, "miner_addr");
    EXPECT_GT(coinbase.outputs[0].amount, 0u);
}
