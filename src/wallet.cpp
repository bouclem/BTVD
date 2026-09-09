#include "wallet.h"
#include "sha256.h"
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdint>

namespace bitvoid {

// ML-DSA-65 (Dilithium) post-quantum signatures via OpenSSL 3.4+.
// NIST FIPS 204 compliant.
// Immune to quantum computer attacks.

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

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-65", nullptr);
    if (!ctx) return;

    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_keygen(ctx, &impl_->key_pair);

    EVP_PKEY_CTX_free(ctx);
}

// Extract public key bytes.
static std::vector<uint8_t> get_public_key_bytes(EVP_PKEY* key) {
    size_t len = 0;
    EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &len);
    std::vector<uint8_t> buf(len);
    EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, buf.data(), len, &len);
    return buf;
}

std::string Wallet::get_public_key_hex() const {
    if (!impl_->key_pair) return "";
    auto bytes = get_public_key_bytes(impl_->key_pair);
    return bytes_to_hex(bytes);
}

std::string Wallet::get_address() const {
    if (!impl_->key_pair) return "";
    auto pub_bytes = get_public_key_bytes(impl_->key_pair);
    std::string pub_str(pub_bytes.begin(), pub_bytes.end());
    std::string hash = sha256(pub_str);
    return hash.substr(0, 40);
}

std::string Wallet::sign(const std::string& data) const {
    if (!impl_->key_pair) return "";

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, impl_->key_pair, nullptr);
    EVP_SIGNATURE* sig_alg = EVP_SIGNATURE_fetch(nullptr, "ML-DSA-65", nullptr);
    if (!ctx || !sig_alg) {
        if (sig_alg) EVP_SIGNATURE_free(sig_alg);
        if (ctx) EVP_PKEY_CTX_free(ctx);
        return "";
    }

    EVP_PKEY_sign_message_init(ctx, sig_alg, nullptr);

    size_t sig_len = 0;
    EVP_PKEY_sign(ctx, nullptr, &sig_len, reinterpret_cast<const unsigned char*>(data.data()), data.size());

    std::vector<uint8_t> sig(sig_len);
    EVP_PKEY_sign(ctx, sig.data(), &sig_len, reinterpret_cast<const unsigned char*>(data.data()), data.size());

    EVP_SIGNATURE_free(sig_alg);
    EVP_PKEY_CTX_free(ctx);

    return bytes_to_hex(sig);
}

bool Wallet::verify(const std::string& data, const std::string& signature_hex, const std::string& public_key_hex) {
    auto pub_bytes = hex_to_bytes(public_key_hex);
    auto sig_bytes = hex_to_bytes(signature_hex);

    // Reconstruct public key from raw bytes.
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, pub_bytes.data(), pub_bytes.size());
    params[1] = OSSL_PARAM_construct_end();

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-65", nullptr);
    EVP_PKEY* key = nullptr;
    EVP_PKEY_fromdata_init(pctx);
    EVP_PKEY_fromdata(pctx, &key, EVP_PKEY_PUBLIC_KEY, params);
    EVP_PKEY_CTX_free(pctx);

    if (!key) return false;

    EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
    EVP_SIGNATURE* sig_alg = EVP_SIGNATURE_fetch(nullptr, "ML-DSA-65", nullptr);

    if (!vctx || !sig_alg) {
        if (sig_alg) EVP_SIGNATURE_free(sig_alg);
        if (vctx) EVP_PKEY_CTX_free(vctx);
        EVP_PKEY_free(key);
        return false;
    }

    EVP_PKEY_verify_message_init(vctx, sig_alg, nullptr);

    int result = EVP_PKEY_verify(vctx, sig_bytes.data(), sig_bytes.size(),
        reinterpret_cast<const unsigned char*>(data.data()), data.size());

    EVP_SIGNATURE_free(sig_alg);
    EVP_PKEY_CTX_free(vctx);
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

std::string Wallet::address_from_public_key(const std::string& public_key_hex) {
    auto pub_bytes = hex_to_bytes(public_key_hex);
    std::string pub_str(pub_bytes.begin(), pub_bytes.end());
    return sha256(pub_str).substr(0, 40);
}

bool Wallet::save(const std::string& path) const {
    if (!impl_->key_pair) return false;

    // Save full key pair (private + public) to binary file.
    int len = i2d_PrivateKey(impl_->key_pair, nullptr);
    if (len <= 0) return false;

    std::vector<uint8_t> buf(len);
    unsigned char* ptr = buf.data();
    i2d_PrivateKey(impl_->key_pair, &ptr);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    return true;
}

bool Wallet::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    const unsigned char* ptr = buf.data();
    EVP_PKEY* key = d2i_PrivateKey(EVP_PKEY_NONE, nullptr, &ptr, buf.size());

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
    std::cout << "  (Post-quantum: ML-DSA-65 / Dilithium)" << std::endl;

    if (!save_path.empty()) {
        if (wallet.save(save_path)) {
            std::cout << "  Saved to: " << save_path << std::endl;
        } else {
            std::cout << "  Failed to save to: " << save_path << std::endl;
        }
    }
}

} // namespace bitvoid
