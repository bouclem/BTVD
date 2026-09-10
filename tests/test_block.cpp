#include <gtest/gtest.h>
#include "block.h"
#include "sha256.h"
#include <chrono>

using namespace bitvoid;

// --- Genesis block ---

TEST(BlockTest, GenesisHasIndexZero) {
    Block genesis = create_genesis_block();
    EXPECT_EQ(genesis.header.index, 0u);
}

TEST(BlockTest, GenesisHasNullPreviousHash) {
    Block genesis = create_genesis_block();
    EXPECT_EQ(genesis.header.previous_hash,
              std::string(64, '0'));
}

TEST(BlockTest, GenesisHasDifficultyOne) {
    Block genesis = create_genesis_block();
    EXPECT_EQ(genesis.header.difficulty, 1u);
}

TEST(BlockTest, GenesisHasNoEnergy) {
    Block genesis = create_genesis_block();
    EXPECT_EQ(genesis.header.energy_kwh, 0u);
    EXPECT_EQ(genesis.header.watt_seconds, 0u);
}

TEST(BlockTest, GenesisHashIsNotEmpty) {
    Block genesis = create_genesis_block();
    EXPECT_FALSE(genesis.hash.empty());
    EXPECT_EQ(genesis.hash.length(), 64u); // SHA-256 hex = 64 chars
}

TEST(BlockTest, GenesisHashMatchesCalculatedHash) {
    Block genesis = create_genesis_block();
    EXPECT_EQ(genesis.hash, genesis.calculate_hash());
}

// --- Hashing ---

TEST(BlockTest, CalculateHashIsDeterministic) {
    Block genesis = create_genesis_block();
    EXPECT_EQ(genesis.calculate_hash(), genesis.calculate_hash());
}

TEST(BlockTest, DifferentBlocksHaveDifferentHashes) {
    Block a = create_genesis_block();
    Block b = create_genesis_block();
    b.header.timestamp = a.header.timestamp + 1;
    EXPECT_NE(a.calculate_hash(), b.calculate_hash());
}

// --- Merkle root ---

TEST(BlockTest, MerkleRootEmptyTransactions) {
    Block block;
    block.transactions.clear();
    std::string root = block.calculate_merkle_root();
    EXPECT_FALSE(root.empty());
    EXPECT_EQ(root.length(), 64u);
}

TEST(BlockTest, MerkleRootSingleTransaction) {
    Block block;
    Transaction tx;
    tx.timestamp = 1000;
    tx.fee = 100;
    tx.hash = double_sha256("test");
    block.transactions.push_back(tx);

    std::string root = block.calculate_merkle_root();
    EXPECT_EQ(root, tx.hash); // single tx: merkle root = tx hash
}

TEST(BlockTest, MerkleRootTwoTransactions) {
    Block block;
    Transaction tx1, tx2;
    tx1.hash = double_sha256("tx1");
    tx2.hash = double_sha256("tx2");
    block.transactions.push_back(tx1);
    block.transactions.push_back(tx2);

    std::string root = block.calculate_merkle_root();
    std::string expected = double_sha256(tx1.hash + tx2.hash);
    EXPECT_EQ(root, expected);
}

TEST(BlockTest, MerkleRootOddTransactionsDuplicatesLast) {
    Block block;
    Transaction tx1, tx2, tx3;
    tx1.hash = double_sha256("tx1");
    tx2.hash = double_sha256("tx2");
    tx3.hash = double_sha256("tx3");
    block.transactions.push_back(tx1);
    block.transactions.push_back(tx2);
    block.transactions.push_back(tx3);

    std::string root = block.calculate_merkle_root();
    // Layer 1: hash(tx1+tx2), hash(tx3+tx3)
    // Layer 2: hash(hash(tx1+tx2) + hash(tx3+tx3))
    std::string l1a = double_sha256(tx1.hash + tx2.hash);
    std::string l1b = double_sha256(tx3.hash + tx3.hash);
    std::string expected = double_sha256(l1a + l1b);
    EXPECT_EQ(root, expected);
}

// --- Difficulty ---

TEST(BlockTest, MeetsDifficultyZeroBits) {
    Block block;
    block.header.difficulty = 0;
    block.hash = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    EXPECT_TRUE(block.meets_difficulty());
}

TEST(BlockTest, MeetsDifficultyEightBits) {
    Block block;
    block.header.difficulty = 8;
    block.hash = "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    EXPECT_TRUE(block.meets_difficulty());
}

TEST(BlockTest, DoesNotMeetDifficulty) {
    Block block;
    block.header.difficulty = 8;
    block.hash = "ff00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    EXPECT_FALSE(block.meets_difficulty());
}

TEST(BlockTest, MeetsDifficultyPartialByte) {
    Block block;
    block.header.difficulty = 4;
    block.hash = "0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    EXPECT_TRUE(block.meets_difficulty()); // 0x0f = 4 leading zero bits
}

// --- JSON serialization ---

TEST(BlockTest, JsonRoundTripPreservesData) {
    Block genesis = create_genesis_block();
    nlohmann::json j = genesis.to_json();
    Block restored = Block::from_json(j);

    EXPECT_EQ(restored.header.index, genesis.header.index);
    EXPECT_EQ(restored.header.previous_hash, genesis.header.previous_hash);
    EXPECT_EQ(restored.header.timestamp, genesis.header.timestamp);
    EXPECT_EQ(restored.header.difficulty, genesis.header.difficulty);
    EXPECT_EQ(restored.hash, genesis.hash);
    EXPECT_EQ(restored.transactions.size(), genesis.transactions.size());
}

TEST(BlockTest, JsonRoundTripWithTransactions) {
    Block genesis = create_genesis_block();

    Transaction tx;
    tx.timestamp = 12345;
    tx.fee = 500;
    tx.hash = double_sha256("test_tx");

    TxInput in;
    in.tx_id = std::string(64, 'a');
    in.output_index = 0;
    in.signature = "sig";
    in.public_key = "pk";
    tx.inputs.push_back(in);

    TxOutput out;
    out.address = "recipient";
    out.amount = 100000000;
    tx.outputs.push_back(out);

    genesis.transactions.push_back(tx);
    genesis.header.merkle_root = genesis.calculate_merkle_root();

    nlohmann::json j = genesis.to_json();
    Block restored = Block::from_json(j);

    EXPECT_EQ(restored.transactions.size(), 1u);
    EXPECT_EQ(restored.transactions[0].hash, tx.hash);
    EXPECT_EQ(restored.transactions[0].fee, tx.fee);
    EXPECT_EQ(restored.transactions[0].inputs[0].tx_id, in.tx_id);
    EXPECT_EQ(restored.transactions[0].outputs[0].address, out.address);
    EXPECT_EQ(restored.transactions[0].outputs[0].amount, out.amount);
}
