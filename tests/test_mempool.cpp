#include <gtest/gtest.h>
#include "mempool.h"

using namespace bitvoid;

// Helper: create a simple valid transaction.
static Transaction make_tx(const std::string& hash_suffix, uint64_t fee = 100, uint64_t amount = 1000) {
    Transaction tx;
    tx.timestamp = 1000;
    tx.fee = fee;
    tx.hash = "tx_" + hash_suffix;

    TxInput in;
    in.tx_id = "input_" + hash_suffix;
    in.output_index = 0;
    in.signature = "sig";
    in.public_key = "pk";
    tx.inputs.push_back(in);

    TxOutput out;
    out.address = "recipient";
    out.amount = amount;
    tx.outputs.push_back(out);

    return tx;
}

// --- Add and retrieve ---

TEST(MempoolTest, AddTransactionIncreasesSize) {
    Mempool pool;
    EXPECT_EQ(pool.size(), 0u);

    pool.add_transaction(make_tx("1"));
    EXPECT_EQ(pool.size(), 1u);
}

TEST(MempoolTest, AddMultipleTransactions) {
    Mempool pool;
    for (int i = 0; i < 10; i++) {
        pool.add_transaction(make_tx(std::to_string(i)));
    }
    EXPECT_EQ(pool.size(), 10u);
}

TEST(MempoolTest, AddDuplicateTransactionRejected) {
    Mempool pool;
    Transaction tx = make_tx("1");
    EXPECT_TRUE(pool.add_transaction(tx));
    EXPECT_FALSE(pool.add_transaction(tx)); // same hash
    EXPECT_EQ(pool.size(), 1u);
}

TEST(MempoolTest, ContainsReturnsTrue) {
    Mempool pool;
    Transaction tx = make_tx("1");
    pool.add_transaction(tx);
    EXPECT_TRUE(pool.contains(tx.hash));
}

TEST(MempoolTest, ContainsReturnsFalse) {
    Mempool pool;
    EXPECT_FALSE(pool.contains("nonexistent"));
}

// --- Remove ---

TEST(MempoolTest, RemoveTransactionDecreasesSize) {
    Mempool pool;
    pool.add_transaction(make_tx("1"));
    pool.add_transaction(make_tx("2"));
    EXPECT_EQ(pool.size(), 2u);

    pool.remove_transaction("tx_1");
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_FALSE(pool.contains("tx_1"));
}

TEST(MempoolTest, RemoveNonexistentDoesNothing) {
    Mempool pool;
    pool.add_transaction(make_tx("1"));
    pool.remove_transaction("nonexistent");
    EXPECT_EQ(pool.size(), 1u);
}

TEST(MempoolTest, RemoveFromEmptyPoolDoesNothing) {
    Mempool pool;
    pool.remove_transaction("nonexistent");
    EXPECT_EQ(pool.size(), 0u);
}

// --- Clear ---

TEST(MempoolTest, ClearEmptiesPool) {
    Mempool pool;
    for (int i = 0; i < 5; i++) {
        pool.add_transaction(make_tx(std::to_string(i)));
    }
    pool.clear();
    EXPECT_EQ(pool.size(), 0u);
}

// --- Double-spend detection ---

TEST(MempoolTest, RejectsDoubleSpendSameInput) {
    Mempool pool;

    Transaction tx1 = make_tx("1");
    Transaction tx2 = make_tx("2");
    // Make tx2 spend the same input as tx1.
    tx2.inputs[0].tx_id = tx1.inputs[0].tx_id;
    tx2.inputs[0].output_index = tx1.inputs[0].output_index;

    EXPECT_TRUE(pool.add_transaction(tx1));
    EXPECT_FALSE(pool.add_transaction(tx2)); // conflicting input
    EXPECT_EQ(pool.size(), 1u);
}

TEST(MempoolTest, HasConflictingInputsReturnsTrue) {
    Mempool pool;
    Transaction tx = make_tx("1");
    pool.add_transaction(tx);

    std::vector<TxInput> inputs = tx.inputs;
    EXPECT_TRUE(pool.has_conflicting_inputs(inputs));
}

TEST(MempoolTest, HasConflictingInputsReturnsFalse) {
    Mempool pool;
    pool.add_transaction(make_tx("1"));

    std::vector<TxInput> inputs;
    TxInput in;
    in.tx_id = "different_input";
    in.output_index = 0;
    inputs.push_back(in);

    EXPECT_FALSE(pool.has_conflicting_inputs(inputs));
}

TEST(MempoolTest, RemovingTxUnlocksInput) {
    Mempool pool;

    Transaction tx1 = make_tx("1");
    pool.add_transaction(tx1);
    pool.remove_transaction(tx1.hash);

    // Now we can add a tx with the same input.
    Transaction tx2 = make_tx("2");
    tx2.inputs[0].tx_id = tx1.inputs[0].tx_id;
    tx2.inputs[0].output_index = tx1.inputs[0].output_index;
    EXPECT_TRUE(pool.add_transaction(tx2));
}

// --- Fee-sorted block selection ---

TEST(MempoolTest, GetForBlockReturnsHighestFeeFirst) {
    Mempool pool;
    pool.add_transaction(make_tx("low", 50));
    pool.add_transaction(make_tx("high", 500));
    pool.add_transaction(make_tx("mid", 200));

    auto txs = pool.get_for_block(10);
    ASSERT_EQ(txs.size(), 3u);
    EXPECT_EQ(txs[0].fee, 500u); // highest first
    EXPECT_EQ(txs[1].fee, 200u);
    EXPECT_EQ(txs[2].fee, 50u);
}

TEST(MempoolTest, GetForBlockRespectsMaxCount) {
    Mempool pool;
    for (int i = 0; i < 10; i++) {
        pool.add_transaction(make_tx(std::to_string(i), static_cast<uint64_t>(100 - i)));
    }

    auto txs = pool.get_for_block(3);
    EXPECT_EQ(txs.size(), 3u);
}

TEST(MempoolTest, GetForBlockEmptyPool) {
    Mempool pool;
    auto txs = pool.get_for_block(10);
    EXPECT_TRUE(txs.empty());
}

// --- Total pending value ---

TEST(MempoolTest, TotalPendingValueSumsCorrectly) {
    Mempool pool;
    // make_tx creates tx with amount=1000, fee=100.
    pool.add_transaction(make_tx("1", 100, 1000));
    pool.add_transaction(make_tx("2", 200, 2000));

    // total = (1000 + 100) + (2000 + 200) = 3300
    EXPECT_EQ(pool.total_pending_value(), 3300u);
}

TEST(MempoolTest, TotalPendingValueEmptyPool) {
    Mempool pool;
    EXPECT_EQ(pool.total_pending_value(), 0u);
}

// --- Get all ---

TEST(MempoolTest, GetAllReturnsAllTransactions) {
    Mempool pool;
    for (int i = 0; i < 5; i++) {
        pool.add_transaction(make_tx(std::to_string(i)));
    }
    auto all = pool.get_all();
    EXPECT_EQ(all.size(), 5u);
}

TEST(MempoolTest, GetAllEmptyPool) {
    Mempool pool;
    auto all = pool.get_all();
    EXPECT_TRUE(all.empty());
}
