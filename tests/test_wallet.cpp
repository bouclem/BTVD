#include <gtest/gtest.h>
#include "wallet.h"
#include "sha256.h"
#include <filesystem>

using namespace bitvoid;

// --- Key generation ---

TEST(WalletTest, GenerateKeysCreatesKeyPair) {
    Wallet wallet;
    wallet.generate_keys();
    EXPECT_TRUE(wallet.has_keys());
}

TEST(WalletTest, NewWalletHasNoKeys) {
    Wallet wallet;
    EXPECT_FALSE(wallet.has_keys());
}

TEST(WalletTest, GeneratedAddressIs40Chars) {
    Wallet wallet;
    wallet.generate_keys();
    std::string addr = wallet.get_address();
    EXPECT_EQ(addr.length(), 40u);
}

TEST(WalletTest, GeneratedPublicKeyIsNotEmpty) {
    Wallet wallet;
    wallet.generate_keys();
    EXPECT_FALSE(wallet.get_public_key_hex().empty());
}

TEST(WalletTest, DifferentWalletsHaveDifferentAddresses) {
    Wallet a, b;
    a.generate_keys();
    b.generate_keys();
    EXPECT_NE(a.get_address(), b.get_address());
}

TEST(WalletTest, SameWalletProducesSameAddress) {
    Wallet wallet;
    wallet.generate_keys();
    std::string addr1 = wallet.get_address();
    std::string addr2 = wallet.get_address();
    EXPECT_EQ(addr1, addr2);
}

// --- Sign and verify ---

TEST(WalletTest, SignProducesNonEmptySignature) {
    Wallet wallet;
    wallet.generate_keys();
    std::string sig = wallet.sign("test data");
    EXPECT_FALSE(sig.empty());
}

TEST(WalletTest, VerifyValidSignature) {
    Wallet wallet;
    wallet.generate_keys();
    std::string data = "hello world";
    std::string sig = wallet.sign(data);
    std::string pub = wallet.get_public_key_hex();

    EXPECT_TRUE(Wallet::verify(data, sig, pub));
}

TEST(WalletTest, VerifyWithWrongDataReturnsFalse) {
    Wallet wallet;
    wallet.generate_keys();
    std::string sig = wallet.sign("original data");
    std::string pub = wallet.get_public_key_hex();

    EXPECT_FALSE(Wallet::verify("different data", sig, pub));
}

TEST(WalletTest, VerifyWithWrongPublicKeyReturnsFalse) {
    Wallet wallet;
    wallet.generate_keys();
    std::string sig = wallet.sign("test data");
    std::string pub = wallet.get_public_key_hex();

    Wallet other;
    other.generate_keys();
    std::string other_pub = other.get_public_key_hex();

    EXPECT_FALSE(Wallet::verify("test data", sig, other_pub));
}

TEST(WalletTest, VerifyWithTamperedSignatureReturnsFalse) {
    Wallet wallet;
    wallet.generate_keys();
    std::string sig = wallet.sign("test data");
    std::string pub = wallet.get_public_key_hex();

    // Tamper with signature.
    if (sig.length() > 0) sig[0] = (sig[0] == '0') ? '1' : '0';
    EXPECT_FALSE(Wallet::verify("test data", sig, pub));
}

// --- Address derivation ---

TEST(WalletTest, AddressFromPublicKeyMatchesWalletAddress) {
    Wallet wallet;
    wallet.generate_keys();
    std::string pub = wallet.get_public_key_hex();
    std::string derived = Wallet::address_from_public_key(pub);
    std::string actual = wallet.get_address();
    EXPECT_EQ(derived, actual);
}

TEST(WalletTest, AddressFromPublicKeyIsDeterministic) {
    Wallet wallet;
    wallet.generate_keys();
    std::string pub = wallet.get_public_key_hex();
    std::string derived1 = Wallet::address_from_public_key(pub);
    std::string derived2 = Wallet::address_from_public_key(pub);
    EXPECT_EQ(derived1, derived2);
}

// --- Save and load ---

TEST(WalletTest, SaveAndLoadPreservesKeys) {
    Wallet wallet;
    wallet.generate_keys();
    std::string original_addr = wallet.get_address();
    std::string original_pub = wallet.get_public_key_hex();

    std::string path = "test_wallet_temp.key";
    EXPECT_TRUE(wallet.save(path));
    EXPECT_TRUE(std::filesystem::exists(path));

    Wallet loaded;
    EXPECT_TRUE(loaded.load(path));
    EXPECT_EQ(loaded.get_address(), original_addr);
    EXPECT_EQ(loaded.get_public_key_hex(), original_pub);

    std::filesystem::remove(path);
}

TEST(WalletTest, LoadNonexistentFileReturnsFalse) {
    Wallet wallet;
    EXPECT_FALSE(wallet.load("nonexistent_wallet.key"));
}

TEST(WalletTest, SaveWithoutKeysReturnsFalse) {
    Wallet wallet;
    EXPECT_FALSE(wallet.save("test_wallet_nokeys.key"));
}

// --- Transaction creation ---

TEST(WalletTest, CreateTransactionWithSufficientFunds) {
    Wallet wallet;
    wallet.generate_keys();
    std::string my_addr = wallet.get_address();

    // Create fake UTXOs.
    std::vector<UTXO> utxos;
    utxos.push_back({std::string(64, 'a'), 0, my_addr, 200000000}); // 2 BTVD

    Transaction tx = wallet.create_transaction("recipient", 100000000, 1000000, utxos);
    EXPECT_FALSE(tx.hash.empty());
    EXPECT_EQ(tx.outputs.size(), 2u); // recipient + change
    EXPECT_EQ(tx.outputs[0].address, "recipient");
    EXPECT_EQ(tx.outputs[0].amount, 100000000u);
}

TEST(WalletTest, CreateTransactionWithExactAmount) {
    Wallet wallet;
    wallet.generate_keys();
    std::string my_addr = wallet.get_address();

    std::vector<UTXO> utxos;
    utxos.push_back({std::string(64, 'a'), 0, my_addr, 101000000}); // 1.01 BTVD

    Transaction tx = wallet.create_transaction("recipient", 100000000, 1000000, utxos);
    EXPECT_FALSE(tx.hash.empty());
    // No change output (exact amount).
    EXPECT_EQ(tx.outputs.size(), 1u);
}

TEST(WalletTest, CreateTransactionInsufficientFunds) {
    Wallet wallet;
    wallet.generate_keys();
    std::string my_addr = wallet.get_address();

    std::vector<UTXO> utxos;
    utxos.push_back({std::string(64, 'a'), 0, my_addr, 50000000}); // 0.5 BTVD

    Transaction tx = wallet.create_transaction("recipient", 100000000, 1000000, utxos);
    EXPECT_TRUE(tx.hash.empty()); // failed
}

TEST(WalletTest, CreateTransactionWithMultipleUTXOs) {
    Wallet wallet;
    wallet.generate_keys();
    std::string my_addr = wallet.get_address();

    std::vector<UTXO> utxos;
    utxos.push_back({std::string(64, 'a'), 0, my_addr, 50000000});
    utxos.push_back({std::string(64, 'b'), 0, my_addr, 50000000});
    utxos.push_back({std::string(64, 'c'), 0, my_addr, 50000000});

    // Need 1.5 BTVD (150M), have 1.5 BTVD in 3 UTXOs.
    Transaction tx = wallet.create_transaction("recipient", 149000000, 1000000, utxos);
    EXPECT_FALSE(tx.hash.empty());
    EXPECT_EQ(tx.inputs.size(), 3u); // all 3 UTXOs needed
}

// --- Sign/verify roundtrip on transaction ---

TEST(WalletTest, SignedTransactionVerifies) {
    Wallet wallet;
    wallet.generate_keys();
    std::string my_addr = wallet.get_address();

    std::vector<UTXO> utxos;
    utxos.push_back({std::string(64, 'a'), 0, my_addr, 200000000});

    Transaction tx = wallet.create_transaction("recipient", 100000000, 1000000, utxos);
    ASSERT_FALSE(tx.hash.empty());

    // Verify each input signature.
    std::string data = tx.serialize();
    for (const auto& in : tx.inputs) {
        EXPECT_TRUE(Wallet::verify(data, in.signature, in.public_key));
    }
}
