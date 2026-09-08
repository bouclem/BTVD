#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace bitvoid {

// SHA-256 wrapper using OpenSSL.
// Returns hex string of the hash.
std::string sha256(const std::string& data);
std::string sha256(const std::vector<uint8_t>& data);

// Double SHA-256 (like Bitcoin) for extra security.
std::string double_sha256(const std::string& data);
std::string double_sha256(const std::vector<uint8_t>& data);

// Convert hex string to bytes and back.
std::vector<uint8_t> hex_to_bytes(const std::string& hex);
std::string bytes_to_hex(const std::vector<uint8_t>& bytes);

} // namespace bitvoid
