#include "transaction.h"
#include "sha256.h"
#include <sstream>

namespace bitvoid {

// TxInput
nlohmann::json TxInput::to_json() const {
    return {
        {"tx_id", tx_id},
        {"output_index", output_index},
        {"signature", signature},
        {"public_key", public_key}
    };
}

TxInput TxInput::from_json(const nlohmann::json& j) {
    TxInput in;
    in.tx_id = j["tx_id"].get<std::string>();
    in.output_index = j["output_index"].get<uint32_t>();
    in.signature = j["signature"].get<std::string>();
    in.public_key = j["public_key"].get<std::string>();
    return in;
}

// TxOutput
nlohmann::json TxOutput::to_json() const {
    return {
        {"address", address},
        {"amount", amount}
    };
}

TxOutput TxOutput::from_json(const nlohmann::json& j) {
    TxOutput out;
    out.address = j["address"].get<std::string>();
    out.amount = j["amount"].get<uint64_t>();
    return out;
}

// Transaction
std::string Transaction::serialize() const {
    // Serialize without signature fields (for signing).
    std::stringstream ss;
    ss << timestamp << fee;
    for (const auto& in : inputs) {
        ss << in.tx_id << in.output_index << in.public_key;
    }
    for (const auto& out : outputs) {
        ss << out.address << out.amount;
    }
    return ss.str();
}

std::string Transaction::serialize_full() const {
    std::stringstream ss;
    ss << timestamp << fee;
    for (const auto& in : inputs) {
        ss << in.tx_id << in.output_index << in.signature << in.public_key;
    }
    for (const auto& out : outputs) {
        ss << out.address << out.amount;
    }
    return ss.str();
}

std::string Transaction::calculate_hash() const {
    return double_sha256(serialize());
}

nlohmann::json Transaction::to_json() const {
    nlohmann::json j;
    j["timestamp"] = timestamp;
    j["fee"] = fee;
    j["hash"] = hash;

    nlohmann::json ins = nlohmann::json::array();
    for (const auto& in : inputs) {
        ins.push_back(in.to_json());
    }
    j["inputs"] = ins;

    nlohmann::json outs = nlohmann::json::array();
    for (const auto& out : outputs) {
        outs.push_back(out.to_json());
    }
    j["outputs"] = outs;

    return j;
}

Transaction Transaction::from_json(const nlohmann::json& j) {
    Transaction tx;
    tx.timestamp = j["timestamp"].get<uint64_t>();
    tx.fee = j["fee"].get<uint64_t>();
    tx.hash = j["hash"].get<std::string>();

    for (const auto& in : j["inputs"]) {
        tx.inputs.push_back(TxInput::from_json(in));
    }
    for (const auto& out : j["outputs"]) {
        tx.outputs.push_back(TxOutput::from_json(out));
    }

    return tx;
}

bool Transaction::is_valid_structure() const {
    if (inputs.empty()) return false;
    if (outputs.empty()) return false;

    for (const auto& out : outputs) {
        if (out.amount == 0) return false;
        if (out.address.empty()) return false;
    }

    for (const auto& in : inputs) {
        if (in.tx_id.empty()) return false;
    }

    return true;
}

bool Transaction::is_coinbase() const {
    static const std::string null_hash(64, '0');
    return inputs.size() == 1
        && inputs[0].tx_id == null_hash
        && inputs[0].signature == "coinbase";
}

uint64_t Transaction::total_output() const {
    uint64_t total = 0;
    for (const auto& out : outputs) {
        total += out.amount;
    }
    return total;
}

// UTXO
nlohmann::json UTXO::to_json() const {
    return {
        {"tx_id", tx_id},
        {"output_index", output_index},
        {"address", address},
        {"amount", amount}
    };
}

UTXO UTXO::from_json(const nlohmann::json& j) {
    UTXO u;
    u.tx_id = j["tx_id"].get<std::string>();
    u.output_index = j["output_index"].get<uint32_t>();
    u.address = j["address"].get<std::string>();
    u.amount = j["amount"].get<uint64_t>();
    return u;
}

} // namespace bitvoid
