#include <gtest/gtest.h>
#include "transaction.h"
#include "sha256.h"

using namespace bitvoid;

// --- Structure validation ---

TEST(TransactionTest, EmptyInputsInvalid) {
    Transaction tx;
    tx.inputs.clear();
    TxOutput out;
    out.address = "addr";
    out.amount = 100;
    tx.outputs.push_back(out);
    EXPECT_FALSE(tx.is_valid_structure());
}

TEST(TransactionTest, EmptyOutputsInvalid) {
    Transaction tx;
    TxInput in;
    in.tx_id = std::string(64, '0');
    in.output_index = 0;
    tx.inputs.push_back(in);
    tx.outputs.clear();
    EXPECT_FALSE(tx.is_valid_structure());
}

TEST(TransactionTest, ZeroAmountInvalid) {
    Transaction tx;
    TxInput in;
    in.tx_id = std::string(64, '0');
    in.output_index = 0;
    tx.inputs.push_back(in);
    TxOutput out;
    out.address = "addr";
    out.amount = 0;
    tx.outputs.push_back(out);
    EXPECT_FALSE(tx.is_valid_structure());
}

TEST(TransactionTest, EmptyAddressInvalid) {
    Transaction tx;
    TxInput in;
    in.tx_id = std::string(64, '0');
    in.output_index = 0;
    tx.inputs.push_back(in);
    TxOutput out;
    out.address = "";
    out.amount = 100;
    tx.outputs.push_back(out);
    EXPECT_FALSE(tx.is_valid_structure());
}

TEST(TransactionTest, ValidTransaction) {
    Transaction tx;
    TxInput in;
    in.tx_id = std::string(64, '0');
    in.output_index = 0;
    tx.inputs.push_back(in);
    TxOutput out;
    out.address = "recipient";
    out.amount = 100000000;
    tx.outputs.push_back(out);
    EXPECT_TRUE(tx.is_valid_structure());
}

// --- Coinbase detection ---

TEST(TransactionTest, CoinbaseDetected) {
    Transaction tx;
    TxInput in;
    in.tx_id = std::string(64, '0');
    in.output_index = 0;
    in.signature = "coinbase";
    in.public_key = "coinbase";
    tx.inputs.push_back(in);

    TxOutput out;
    out.address = "miner";
    out.amount = 50000000;
    tx.outputs.push_back(out);

    EXPECT_TRUE(tx.is_coinbase());
}

TEST(TransactionTest, NonCoinbaseNotDetected) {
    Transaction tx;
    TxInput in;
    in.tx_id = std::string(64, 'a');
    in.output_index = 0;
    in.signature = "realsig";
    in.public_key = "realpk";
    tx.inputs.push_back(in);

    TxOutput out;
    out.address = "recipient";
    out.amount = 100000000;
    tx.outputs.push_back(out);

    EXPECT_FALSE(tx.is_coinbase());
}

TEST(TransactionTest, MultipleInputsNotCoinbase) {
    Transaction tx;
    for (int i = 0; i < 2; i++) {
        TxInput in;
        in.tx_id = std::string(64, '0');
        in.output_index = static_cast<uint32_t>(i);
        in.signature = "coinbase";
        in.public_key = "coinbase";
        tx.inputs.push_back(in);
    }
    TxOutput out;
    out.address = "miner";
    out.amount = 100;
    tx.outputs.push_back(out);

    EXPECT_FALSE(tx.is_coinbase()); // coinbase must have exactly 1 input
}

// --- Serialization & hashing ---

TEST(TransactionTest, SerializeIsDeterministic) {
    Transaction tx;
    tx.timestamp = 1000;
    tx.fee = 500;
    TxInput in;
    in.tx_id = std::string(64, 'a');
    in.output_index = 1;
    in.public_key = "pk";
    tx.inputs.push_back(in);
    TxOutput out;
    out.address = "addr";
    out.amount = 200;
    tx.outputs.push_back(out);

    EXPECT_EQ(tx.serialize(), tx.serialize());
}

TEST(TransactionTest, CalculateHashIsDeterministic) {
    Transaction tx;
    tx.timestamp = 1000;
    tx.fee = 500;
    TxInput in;
    in.tx_id = std::string(64, 'a');
    in.output_index = 1;
    in.public_key = "pk";
    tx.inputs.push_back(in);
    TxOutput out;
    out.address = "addr";
    out.amount = 200;
    tx.outputs.push_back(out);

    EXPECT_EQ(tx.calculate_hash(), tx.calculate_hash());
}

TEST(TransactionTest, DifferentTransactionsHaveDifferentHashes) {
    Transaction tx1, tx2;
    tx1.timestamp = 1000;
    tx1.fee = 500;
    tx2.timestamp = 2000;
    tx2.fee = 500;

    TxInput in;
    in.tx_id = std::string(64, 'a');
    in.output_index = 0;
    in.public_key = "pk";
    tx1.inputs.push_back(in);
    tx2.inputs.push_back(in);

    TxOutput out;
    out.address = "addr";
    out.amount = 100;
    tx1.outputs.push_back(out);
    tx2.outputs.push_back(out);

    EXPECT_NE(tx1.calculate_hash(), tx2.calculate_hash());
}

// --- Total output ---

TEST(TransactionTest, TotalOutputSumsCorrectly) {
    Transaction tx;
    TxOutput out1, out2, out3;
    out1.amount = 100;
    out2.amount = 200;
    out3.amount = 300;
    tx.outputs.push_back(out1);
    tx.outputs.push_back(out2);
    tx.outputs.push_back(out3);
    EXPECT_EQ(tx.total_output(), 600u);
}

TEST(TransactionTest, TotalOutputEmptyIsZero) {
    Transaction tx;
    EXPECT_EQ(tx.total_output(), 0u);
}

// --- JSON round-trip ---

TEST(TransactionTest, JsonRoundTripPreservesData) {
    Transaction tx;
    tx.timestamp = 99999;
    tx.fee = 12345;
    tx.hash = "abc123";

    TxInput in;
    in.tx_id = std::string(64, 'f');
    in.output_index = 3;
    in.signature = "sig_hex";
    in.public_key = "pk_hex";
    tx.inputs.push_back(in);

    TxOutput out;
    out.address = "recipient_addr";
    out.amount = 50000000;
    tx.outputs.push_back(out);

    nlohmann::json j = tx.to_json();
    Transaction restored = Transaction::from_json(j);

    EXPECT_EQ(restored.timestamp, tx.timestamp);
    EXPECT_EQ(restored.fee, tx.fee);
    EXPECT_EQ(restored.hash, tx.hash);
    EXPECT_EQ(restored.inputs.size(), 1u);
    EXPECT_EQ(restored.inputs[0].tx_id, in.tx_id);
    EXPECT_EQ(restored.inputs[0].output_index, in.output_index);
    EXPECT_EQ(restored.inputs[0].signature, in.signature);
    EXPECT_EQ(restored.inputs[0].public_key, in.public_key);
    EXPECT_EQ(restored.outputs.size(), 1u);
    EXPECT_EQ(restored.outputs[0].address, out.address);
    EXPECT_EQ(restored.outputs[0].amount, out.amount);
}
