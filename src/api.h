#pragma once

#include <string>
#include "blockchain.h"
#include "mempool.h"

namespace bitvoid {

// API: handles HTTP requests and returns JSON for the block explorer.
class ExplorerApi {
public:
    ExplorerApi(Blockchain& chain, Mempool& mempool);

    // Handle a request: method, path, query string -> JSON response.
    std::string handle(const std::string& method, const std::string& path,
                       const std::string& query);

private:
    Blockchain& chain_;
    Mempool& mempool_;

    // Endpoint handlers.
    std::string get_status();
    std::string get_blocks(const std::string& query);
    std::string get_block_by_height(uint64_t height);
    std::string get_block_by_hash(const std::string& hash);
    std::string get_transaction(const std::string& hash);
    std::string get_address(const std::string& address);

    // Parse query parameter value (e.g. "limit=20" -> "20").
    std::string get_query_param(const std::string& query, const std::string& key) const;
};

} // namespace bitvoid
