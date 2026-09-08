#pragma once

#include <string>
#include <vector>
#include <memory>
#include "transaction.h"

namespace bitvoid {

// Wallet: manages keys, addresses, and transaction creation.
// Uses OpenSSL for key generation and signing (ECDSA secp256k1 for now,
// with architecture ready to swap to post-quantum signatures later).

class Wallet {
public:
    Wallet();
    ~Wallet();

    // Generate a new key pair.
    void generate_keys();

    // Load keys from a file.
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    // Get wallet address (derived from public key).
    std::string get_address() const;

    // Get public key (hex).
    std::string get_public_key_hex() const;

    // Sign data with private key.
    std::string sign(const std::string& data) const;

    // Verify a signature.
    static bool verify(const std::string& data, const std::string& signature, const std::string& public_key_hex);

    // Create a transaction sending amount to address.
    // Uses UTXOs from the blockchain to fund the transaction.
    Transaction create_transaction(
        const std::string& to_address,
        uint64_t amount,
        uint64_t fee,
        const std::vector<UTXO>& available_utxos
    ) const;

    // Check if wallet has keys.
    bool has_keys() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Utility: generate a new wallet and print info.
void generate_new_wallet(const std::string& save_path);

} // namespace bitvoid
