#include "wallet.h"
#include "sha256.h"
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/obj_mac.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

namespace bitvoid {

// ECDSA key pair using secp256k1 (same curve as Bitcoin).
// Architecture is designed to swap to post-quantum (ML-DSA) later.

struct Wallet::Impl {
    EVP_PKEY* key_pair = nullptr;

    Impl() = default;
    ~Impl() {
        if (key_pair) {
            EVP_PKEY_free(key_pair);
        }
    }
};

Wallet::Wallet() : impl_(std::make_unique<Impl>()) {}
Wallet::~Wallet() = default;

void Wallet::generate_keys() {
    if (impl_->key_pair) {
        EVP_PKEY_free(impl_->key_pair);
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1);

    impl_->key_pair = nullptr;
    EVP_PKEY_keygen(ctx, &impl_->key_pair);

    EVP_PKEY_CTX_free(ctx);
}

// Extract public key bytes.
static std::vector<uint8_t> get_public_key_bytes(EVP_PKEY* key) {
    int len = i2d_PUBKEY(key, nullptr);
    std::vector<uint8_t> buf(len);
    uint8_t* ptr = buf.data();
    i2d_PUBKEY(key, &ptr);
    return buf;
}

std::string Wallet::get_public_key_hex() const {
    if (!impl_->key_pair) return "";
    auto bytes = get_public_key_bytes(impl_->key_pair);
    return bytes_to_hex(bytes);
}

std::string Wallet::get_address() const {
    if (!impl_->key_pair) return "";
    // Address = first 20 bytes of SHA-256(public_key), encoded as hex.
    auto pub_bytes = get_public_key_bytes(impl_->key_pair);
    std::string pub_str(pub_bytes.begin(), pub_bytes.end());
    std::string hash = sha256(pub_str);
    return hash.substr(0, 40); // first 20 bytes = 40 hex chars
}

std::string Wallet::sign(const std::string& data) const {
    if (!impl_->key_pair) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, impl_->key_pair);

    EVP_DigestSignUpdate(ctx, data.data(), data.size());

    size_t sig_len = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sig_len);

    std::vector<uint8_t> sig(sig_len);
    EVP_DigestSignFinal(ctx, sig.data(), &sig_len);

    EVP_MD_CTX_free(ctx);

    return bytes_to_hex(sig);
}

bool Wallet::verify(const std::string& data, const std::string& signature_hex, const std::string& public_key_hex) {
    // Reconstruct public key from hex.
    auto pub_bytes = hex_to_bytes(public_key_hex);

    const uint8_t* pub_ptr = pub_bytes.data();
    EVP_PKEY* key = d2i_PUBKEY(nullptr, &pub_ptr, pub_bytes.size());
    if (!key) return false;

    auto sig_bytes = hex_to_bytes(signature_hex);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key);
    EVP_DigestVerifyUpdate(ctx, data.data(), data.size());

    int result = EVP_DigestVerifyFinal(ctx, sig_bytes.data(), sig_bytes.size());

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);

    return result == 1;
}

Transaction Wallet::create_transaction(
    const std::string& to_address,
    uint64_t amount,
    uint64_t fee,
    const std::vector<UTXO>& available_utxos
) const {
    Transaction tx;
    tx.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    tx.fee = fee;

    std::string my_address = get_address();
    std::string my_pubkey = get_public_key_hex();

    // Select UTXOs to cover amount + fee (greedy: just use them in order).
    uint64_t total_needed = amount + fee;
    uint64_t total_selected = 0;

    for (const auto& utxo : available_utxos) {
        if (total_selected >= total_needed) break;

        TxInput in;
        in.tx_id = utxo.tx_id;
        in.output_index = utxo.output_index;
        in.public_key = my_pubkey;
        // Signature will be filled after we know the tx structure.
        tx.inputs.push_back(in);

        total_selected += utxo.amount;
    }

    if (total_selected < total_needed) {
        // Not enough funds.
        tx.hash = "";
        return tx;
    }

    // Create output to recipient.
    TxOutput out;
    out.address = to_address;
    out.amount = amount;
    tx.outputs.push_back(out);

    // Create change output back to self.
    uint64_t change = total_selected - total_needed;
    if (change > 0) {
        TxOutput change_out;
        change_out.address = my_address;
        change_out.amount = change;
        tx.outputs.push_back(change_out);
    }

    // Calculate tx hash (without signatures).
    tx.hash = tx.calculate_hash();

    // Sign each input.
    std::string data_to_sign = tx.serialize();
    for (auto& in : tx.inputs) {
        in.signature = sign(data_to_sign);
    }

    // Recalculate hash with signatures included.
    tx.hash = tx.calculate_hash();

    return tx;
}

bool Wallet::has_keys() const {
    return impl_->key_pair != nullptr;
}

bool Wallet::save(const std::string& path) const {
    if (!impl_->key_pair) return false;

    FILE* file = fopen(path.c_str(), "wb");
    if (!file) return false;

    int result = i2d_PUBKEY_fp(file, impl_->key_pair);
    fclose(file);
    return result == 1;
}

bool Wallet::load(const std::string& path) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return false;

    EVP_PKEY* key = nullptr;
    key = d2i_PUBKEY_fp(file, &key);
    fclose(file);

    if (!key) return false;

    if (impl_->key_pair) {
        EVP_PKEY_free(impl_->key_pair);
    }
    impl_->key_pair = key;
    return true;
}

void generate_new_wallet(const std::string& save_path) {
    Wallet wallet;
    wallet.generate_keys();

    std::cout << "New wallet created!" << std::endl;
    std::cout << "  Address: " << wallet.get_address() << std::endl;
    std::cout << "  Public key: " << wallet.get_public_key_hex().substr(0, 40) << "..." << std::endl;

    if (!save_path.empty()) {
        // Save private key in PEM format.
        // For now, just print the address.
        std::cout << "  Save path: " << save_path << std::endl;
    }
}

} // namespace bitvoid
