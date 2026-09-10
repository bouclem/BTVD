#include <gtest/gtest.h>
#include "blockchain.h"
#include "consensus.h"
#include <filesystem>
#include <chrono>

using namespace bitvoid;

// --- Basic chain operations ---

TEST(BlockchainTest, InitCreatesGenesisBlock) {
    Blockchain chain;
    chain.init();
    EXPECT_EQ(chain.height(), 0u);
    auto genesis = chain.get_block(0);
    ASSERT_TRUE(genesis.has_value());
    EXPECT_EQ(genesis->header.index, 0u);
}

TEST(BlockchainTest, EmptyChainHeightIsZero) {
    Blockchain chain;
    EXPECT_EQ(chain.height(), 0u);
}

TEST(BlockchainTest, GetLatestBlockOnEmptyChainReturnsGenesis) {
    Blockchain chain;
    Block latest = chain.get_latest_block();
    EXPECT_EQ(latest.header.index, 0u);
}

TEST(BlockchainTest, GetLatestBlockAfterInit) {
    Blockchain chain;
    chain.init();
    Block latest = chain.get_latest_block();
    EXPECT_EQ(latest.header.index, 0u);
}

TEST(BlockchainTest, GetBlockOutOfRangeReturnsNullopt) {
    Blockchain chain;
    chain.init();
    EXPECT_FALSE(chain.get_block(999).has_value());
}

TEST(BlockchainTest, GetBlockByHashNonexistentReturnsNullopt) {
    Blockchain chain;
    chain.init();
    EXPECT_FALSE(chain.get_block_by_hash("nonexistent").has_value());
}

TEST(BlockchainTest, GetBlockByHashFindsGenesis) {
    Blockchain chain;
    chain.init();
    Block genesis = chain.get_latest_block();
    auto found = chain.get_block_by_hash(genesis.hash);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->hash, genesis.hash);
}

// --- Adding blocks ---

class BlockchainWithChain : public ::testing::Test {
protected:
    void SetUp() override {
        chain.init();
        genesis = chain.get_latest_block();
        genesis_timestamp = genesis.header.timestamp;
    }

    Block mine_next_block(const std::string& miner_addr) {
        ConsensusEngine consensus;
        ConsensusParams params;
        params.difficulty = 1;
        auto block = consensus.mine_block(
            chain.get_latest_block(), {}, miner_addr, "cpu", params, genesis_timestamp);
        EXPECT_TRUE(block.has_value());
        return *block;
    }

    Blockchain chain;
    Block genesis;
    uint64_t genesis_timestamp;
};

TEST_F(BlockchainWithChain, AddValidBlockIncreasesHeight) {
    Block block = mine_next_block("miner_addr");
    EXPECT_TRUE(chain.add_block(block));
    EXPECT_EQ(chain.height(), 1u);
}

TEST_F(BlockchainWithChain, AddBlockWithWrongIndexRejected) {
    Block block = mine_next_block("miner_addr");
    block.header.index = 99;
    EXPECT_FALSE(chain.add_block(block));
}

TEST_F(BlockchainWithChain, AddBlockWithWrongPreviousHashRejected) {
    Block block = mine_next_block("miner_addr");
    block.header.previous_hash = std::string(64, 'x');
    EXPECT_FALSE(chain.add_block(block));
}

TEST_F(BlockchainWithChain, AddBlockWithTamperedHashRejected) {
    Block block = mine_next_block("miner_addr");
    block.hash = "tampered";
    EXPECT_FALSE(chain.add_block(block));
}

TEST_F(BlockchainWithChain, AddMultipleBlocks) {
    for (int i = 0; i < 5; i++) {
        Block block = mine_next_block("miner_addr");
        EXPECT_TRUE(chain.add_block(block));
    }
    EXPECT_EQ(chain.height(), 5u);
}

// --- UTXO set ---

TEST_F(BlockchainWithChain, CoinbaseCreatesUTXOForMiner) {
    Block block = mine_next_block("miner_addr");
    chain.add_block(block);

    // Coinbase tx is first transaction, output 0 goes to miner.
    auto utxo = chain.get_utxo(block.transactions[0].hash, 0);
    ASSERT_TRUE(utxo.has_value());
    EXPECT_EQ(utxo->address, "miner_addr");
    EXPECT_GT(utxo->amount, 0u);
}

TEST_F(BlockchainWithChain, GetBalanceReturnsCorrectAmount) {
    Block block = mine_next_block("miner_addr");
    chain.add_block(block);

    uint64_t balance = chain.get_balance("miner_addr");
    EXPECT_GT(balance, 0u);
    EXPECT_EQ(balance, 50000000u); // 0.5 BTVD = 50,000,000 base units
}

TEST_F(BlockchainWithChain, GetBalanceZeroForUnknownAddress) {
    chain.init();
    EXPECT_EQ(chain.get_balance("nonexistent"), 0u);
}

TEST_F(BlockchainWithChain, GetUTXOsForAddress) {
    Block block = mine_next_block("miner_addr");
    chain.add_block(block);

    auto utxos = chain.get_utxos_for_address("miner_addr");
    ASSERT_EQ(utxos.size(), 1u);
    EXPECT_EQ(utxos[0].address, "miner_addr");
}

TEST_F(BlockchainWithChain, HasUTXOReturnsTrue) {
    Block block = mine_next_block("miner_addr");
    chain.add_block(block);

    EXPECT_TRUE(chain.has_utxo(block.transactions[0].hash, 0));
}

TEST_F(BlockchainWithChain, HasUTXOReturnsFalse) {
    chain.init();
    EXPECT_FALSE(chain.has_utxo("nonexistent", 0));
}

// --- Disk persistence ---

TEST_F(BlockchainWithChain, SaveAndLoadPreservesChain) {
    for (int i = 0; i < 3; i++) {
        Block block = mine_next_block("miner_addr");
        chain.add_block(block);
    }

    std::string path = "test_chain_temp.chain";
    EXPECT_TRUE(chain.save_to_disk(path));
    EXPECT_TRUE(std::filesystem::exists(path));

    Blockchain loaded;
    EXPECT_TRUE(loaded.load_from_disk(path));
    EXPECT_EQ(loaded.height(), 3u);

    // Balance should be reconstructed from UTXO set.
    EXPECT_EQ(loaded.get_balance("miner_addr"), chain.get_balance("miner_addr"));

    std::filesystem::remove(path);
}

TEST(BlockchainTest, LoadNonexistentFileReturnsFalse) {
    Blockchain chain;
    EXPECT_FALSE(chain.load_from_disk("nonexistent_file.chain"));
}

// --- Validate transaction ---

TEST_F(BlockchainWithChain, ValidateTransactionRejectsCoinbase) {
    Block block = mine_next_block("miner_addr");
    chain.add_block(block);

    // The coinbase tx should not be valid via validate_transaction.
    EXPECT_FALSE(chain.validate_transaction(block.transactions[0]));
}

TEST_F(BlockchainWithChain, ValidateTransactionRejectsNonexistentInput) {
    Transaction tx;
    tx.timestamp = 1000;
    tx.fee = 100;
    TxInput in;
    in.tx_id = std::string(64, '0');
    in.output_index = 0;
    in.signature = "sig";
    in.public_key = "pk";
    tx.inputs.push_back(in);
    TxOutput out;
    out.address = "recipient";
    out.amount = 100;
    tx.outputs.push_back(out);
    tx.hash = tx.calculate_hash();

    EXPECT_FALSE(chain.validate_transaction(tx));
}
