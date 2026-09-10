# BitVoid (BTVD) — Technical Specification

**Version:** 0.1
**Date:** September 2026
**Ticker:** BTVD

This document specifies the BitVoid protocol in full detail. It is intended for developers implementing compatible nodes, wallets, or tools.

---

## 1. Overview

BitVoid is a custom Layer-1 blockchain with three core innovations:

1. **Proof of Energy (PoE)** — energy is the measured, recorded input per block
2. **Event-Driven Block Production** — blocks produced on demand, not on a clock
3. **Infinite Supply with Ramped Emission** — miners paid forever, no halvings

**Language:** C++ (C++20)
**Crypto:** OpenSSL 3.4+ (SHA-256, ML-DSA-65/Dilithium)
**Serialization:** JSON (nlohmann/json)
**Networking:** TCP, newline-delimited JSON messages

---

## 2. Data Structures

### 2.1 Base Units

```
1 BTVD = 100,000,000 base units (satoshis)
```

All internal amounts are stored as `uint64_t` in base units.

### 2.2 Block Header

```cpp
struct BlockHeader {
    uint64_t index;              // block height (0 = genesis)
    string previous_hash;        // double SHA-256 of previous block (64 hex chars)
    uint64_t timestamp;          // Unix timestamp (seconds)
    uint64_t nonce;              // mining nonce
    string merkle_root;          // Merkle root of all transactions (64 hex chars)
    uint32_t difficulty;         // required leading zero bits in block hash

    // PoE fields
    uint64_t energy_kwh;         // kWh consumed to mine this block
    uint64_t watt_seconds;       // precise energy measurement
    string energy_source;        // declared source (solar, hydro, wind, cpu, etc.)
    string miner_address;        // miner's wallet address (40 hex chars)
};
```

### 2.3 Block

```cpp
struct Block {
    BlockHeader header;
    vector<Transaction> transactions;  // first tx is always coinbase
    string hash;                       // double SHA-256 of serialized header + tx hashes
};
```

### 2.4 Transaction

```cpp
struct Transaction {
    vector<TxInput> inputs;
    vector<TxOutput> outputs;
    uint64_t timestamp;    // Unix timestamp (seconds)
    uint64_t fee;          // fee paid to miner (base units)
    string hash;           // double SHA-256 of serialized tx (tx ID)
};
```

### 2.5 TxInput

```cpp
struct TxInput {
    string tx_id;         // hash of the transaction containing the output being spent
    uint32_t output_index; // which output of that transaction (0-based)
    string signature;     // ML-DSA-65 signature (hex)
    string public_key;    // signer's public key (hex)
};
```

### 2.6 TxOutput

```cpp
struct TxOutput {
    string address;   // recipient wallet address (40 hex chars, SHA-256 of pubkey)
    uint64_t amount;   // amount in base units
};
```

### 2.7 UTXO

```cpp
struct UTXO {
    string tx_id;
    uint32_t output_index;
    string address;   // owner's address
    uint64_t amount;
};
```

### 2.8 Coinbase Transaction

The first transaction in every block is the coinbase. It has:
- Exactly 1 input with `tx_id = "0000...0000"` (64 zeros), `signature = "coinbase"`, `public_key = "coinbase"`
- 1 output to the miner's address containing `block_reward + total_fees`

Coinbase transactions are exempt from signature verification.

---

## 3. Cryptography

### 3.1 Hashing

- **Algorithm:** SHA-256 (via OpenSSL EVP)
- **Block hash:** `double_sha256(serialize(block))` — SHA-256 applied twice (like Bitcoin)
- **Transaction hash:** `double_sha256(serialize(tx))` — excludes signature fields to prevent circular hashing
- **Address derivation:** `SHA-256(public_key_bytes).substr(0, 40)` — first 20 bytes of SHA-256 of public key, hex-encoded

### 3.2 Signatures

- **Algorithm:** ML-DSA-65 (Dilithium) — NIST FIPS 204
- **Provider:** OpenSSL 3.4+ with `EVP_PKEY` API
- **Key size:** 1952 bytes (public key), 4032 bytes (private key)
- **Signature size:** ~3309 bytes
- **Signing:** `EVP_PKEY_sign_message_init` + `EVP_PKEY_sign`
- **Verification:** `EVP_PKEY_verify_message_init` + `EVP_PKEY_verify`
- **Quantum resistance:** Yes — immune to Shor's algorithm

### 3.3 Address Format

```
address = SHA-256(public_key_bytes)[0:40]  (hex string, 40 chars)
```

Addresses are not base58 or bech32 encoded — they are raw hex of the first 20 bytes of SHA-256 of the public key.

---

## 4. Consensus: Proof of Energy (PoE)

### 4.1 Mining Process

1. Miner assembles a block with pending transactions from the mempool
2. Creates a coinbase transaction paying `reward + fees` to the miner
3. Calculates the Merkle root of all transactions
4. Iterates nonce from 0 until `double_sha256(serialize(block))` has enough leading zero bits to meet difficulty
5. Records energy consumption: `watt_seconds = elapsed_seconds * estimated_watts` (default: 65W CPU)
6. Converts to kWh: `kwh = watt_seconds / 3,600,000`

### 4.2 Difficulty

- **Unit:** Number of leading zero bits required in the block hash
- **Initial difficulty:** 1 (1 leading zero bit)
- **Adjustment window:** Last 1,000 blocks
- **Target block time:** 5 seconds average
- **Adjustment algorithm:**
  - Calculate `ratio = avg_block_time / target_block_time`
  - If `ratio < 0.8` (blocks too fast): `difficulty += max(1, difficulty * (1 - ratio) * 0.5)`
  - If `ratio > 1.2` (blocks too slow): `difficulty -= max(1, difficulty * (ratio - 1) * 0.5)`
  - Minimum difficulty: 1
  - Otherwise: no change

### 4.3 Block Validation

A block is valid if:
1. `block.header.index == previous_block.index + 1`
2. `block.header.previous_hash == previous_block.hash`
3. `double_sha256(serialize(block)) == block.hash`
4. `block.meets_difficulty()` — hash has enough leading zero bits
5. `block.calculate_merkle_root() == block.header.merkle_root`
6. All transactions pass `is_valid_structure()`
7. All transactions have `calculate_hash() == tx.hash`
8. First transaction is coinbase (exempt from signature verification)
9. All non-coinbase transactions pass signature verification:
   - For each input: `address_from_public_key(input.public_key) == utxo.address`
   - For each input: `Wallet::verify(tx.serialize(), input.signature, input.public_key) == true`
   - For each input: the referenced UTXO exists in the UTXO set

---

## 5. Block Production: Event-Driven

### 5.1 Block Triggers

A block is produced when ANY of these conditions are met:

| Trigger | Condition |
|---|---|
| Transaction count | `mempool.size() >= 100` |
| Pending value | `mempool.total_pending_value() >= 100,000,000,000` (100 BTVD) |
| Safety floor | `now - last_block_timestamp >= 60 seconds` |

### 5.2 Block Timing

| Parameter | Value |
|---|---|
| Minimum block gap | 3 seconds |
| Maximum block gap | 60 seconds |
| Average block time | 3-15 seconds |
| Confirmation time | 6-9 seconds (2 blocks) |
| Throughput | 300-600 TPS |
| Max block size | 2 MB |

### 5.3 Spam Protection

- Minimum 3-second block gap prevents rapid-fire blocks
- Mandatory transaction fees (no free transactions)
- Per-address rate limiting (future)
- Dynamic fees that rise during rapid block production (future)
- 2 MB max block size

---

## 6. Tokenomics

### 6.1 Reward Schedule

| Period | Reward per block | Annual emission |
|---|---|---|
| Year 1 | 0.5 BTVD | ~5.25M BTVD |
| Year 2 | 1.0 BTVD | ~10.5M BTVD |
| Year 3 | 1.5 BTVD | ~15.75M BTVD |
| Year 4+ | 2.0 BTVD | ~21M BTVD/year forever |

### 6.2 Supply Projection

| Year | Total supply |
|---|---|
| 1 | 5.25M BTVD |
| 5 | 73.5M BTVD |
| 10 | 157.5M BTVD |
| 20 | 367.5M BTVD |
| 50 | ~1B BTVD |

### 6.3 Fair Launch Rules

- 0 coins created before public launch (no premine)
- No developer allocation
- No instamine
- Mining announced publicly before it begins
- Creator mines alongside everyone under the same rules

---

## 7. Transaction Model

### 7.1 UTXO Model

BitVoid uses a UTXO (Unspent Transaction Output) model like Bitcoin:
- Transactions consume existing UTXOs as inputs
- Transactions create new UTXOs as outputs
- The UTXO set is updated when blocks are added

### 7.2 Transaction Validation

A transaction is valid if:
1. `inputs` is non-empty
2. `outputs` is non-empty
3. All outputs have `amount > 0` and non-empty `address`
4. All inputs have non-empty `tx_id`
5. `calculate_hash() == tx.hash`
6. Not a coinbase (coinbase txs are only valid inside blocks, not in mempool)
7. All referenced UTXOs exist in the UTXO set
8. For each input: `address_from_public_key(public_key) == utxo.address`
9. For each input: `verify(serialize(tx), signature, public_key) == true`

### 7.3 Transaction Serialization (for hashing)

```
serialize(tx) = timestamp || fee
             || for each input: tx_id || output_index || public_key
             || for each output: address || amount
```

Note: Signature fields are excluded from serialization to prevent circular hashing (the signature signs the serialized data).

### 7.4 Transaction Creation Flow

1. Select UTXOs from sender's address to cover `amount + fee` (greedy, in order)
2. Create output to recipient address
3. Create change output back to sender (if any)
4. Calculate `tx.hash = double_sha256(serialize(tx))`
5. Sign each input: `signature = sign(serialize(tx))` using sender's private key
6. Recalculate `tx.hash` (signatures don't affect hash since they're excluded)

---

## 8. P2P Network Protocol

### 8.1 Connection

- **Transport:** TCP
- **Default port:** 8333
- **Message format:** Newline-delimited JSON (`{"type": "...", "data": {...}}\n`)

### 8.2 Message Types

| Type | Direction | Description |
|---|---|---|
| `tx` | Both | Broadcast a transaction |
| `block` | Both | Broadcast a block |
| `sync_request` | Both | Request blocks from a height |
| `sync_response` | Response | Blocks to sync |
| `get_height` | Both | Request chain height |
| `height` | Response | Chain height |
| `peer_list` | Both | List of known peer addresses |

### 8.3 Message Formats

**Transaction broadcast:**
```json
{"type": "tx", "data": {transaction JSON}}
```

**Block broadcast:**
```json
{"type": "block", "data": {block JSON}}
```

**Sync request:**
```json
{"type": "sync_request", "from_height": 123}
```

**Sync response:**
```json
{"type": "sync_response", "blocks": [{block JSON}, ...]}
```

**Height query:**
```json
{"type": "get_height"}
```

**Height response:**
```json
{"type": "height", "height": 123}
```

**Peer list:**
```json
{"type": "peer_list", "peers": ["1.2.3.4:8333", "5.6.7.8:8333"]}
```

### 8.4 Peer Discovery

1. On connection, both peers exchange peer lists
2. Each node connects to unknown peers received from other peers
3. Self-connections and already-connected peers are skipped
4. On connect, a sync request is sent to get any missing blocks

### 8.5 Gossip Protocol

- Received transactions are validated and relayed to all other peers
- Received blocks are validated, added to chain, and relayed to all other peers
- Invalid transactions/blocks are rejected and not relayed

---

## 9. Explorer API

### 9.1 HTTP Server

- **Port:** 8545 (default)
- **Protocol:** HTTP/1.1
- **CORS:** `Access-Control-Allow-Origin: *`
- **Content-Type:** `application/json`

### 9.2 Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/api/status` | Chain height, difficulty, latest block info |
| GET | `/api/blocks?limit=N` | Recent N blocks (max 1000, default 20) |
| GET | `/api/block/:height` | Block by height |
| GET | `/api/block/hash/:hash` | Block by hash |
| GET | `/api/tx/:hash` | Transaction by hash (searches blocks + mempool) |
| GET | `/api/address/:addr` | Address balance + UTXOs |

### 9.3 Response Format

All responses are JSON. Errors return `{"error": "message"}`.

---

## 10. Storage

### 10.1 Chain File

- **Format:** JSON array of block objects
- **File:** `bitvoid.chain`
- **Save:** After each mined block, on shutdown
- **Load:** On startup, rebuilds UTXO set from all blocks

### 10.2 Wallet File

- **Format:** DER-encoded private key (binary)
- **File:** `wallet.key` (default, configurable with `--wallet-file`)
- **Contains:** Full ML-DSA-65 key pair (private + public)

---

## 11. CLI Commands

### 11.1 Commands

| Command | Description |
|---|---|
| `run` | Full node: P2P + mining + block broadcast |
| `mine` | Mining only (no P2P networking) |
| `node` | P2P node only (no mining) |
| `explorer` | Block explorer HTTP API |
| `wallet` | Wallet operations |
| `status` | Show chain status |

### 11.2 Flags

| Flag | Description |
|---|---|
| `--address X` | Miner/wallet address |
| `--energy Y` | Declared energy source (solar, hydro, wind, cpu, etc.) |
| `--port P` | P2P port (default 8333) or API port (default 8545) |
| `--connect H:P` | Connect to peer at host:port |
| `--new` | Create new wallet |
| `--balance` | Show wallet balance |
| `--send A:ADDR` | Send A BTVD to ADDR |
| `--wallet-file F` | Path to wallet key file (default: wallet.key) |

---

## 12. Genesis Block

```
index:          0
previous_hash:  0000000000000000000000000000000000000000000000000000000000000000
timestamp:      current Unix time at creation
nonce:          0
difficulty:     1
energy_kwh:     0
watt_seconds:   0
energy_source:  "genesis"
miner_address:  "genesis"
merkle_root:    double_sha256("")
transactions:   []
```

The genesis block has no transactions and no energy. It is created on first run if no chain file exists.

---

## 13. Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| OpenSSL | 3.4+ | SHA-256, ML-DSA-65 signatures |
| nlohmann/json | 3.11.3 | JSON serialization |
| GoogleTest | 1.14.0 | Unit tests (build only) |
| CMake | 3.16+ | Build system |
| C++ compiler | C++20 | MSVC, GCC, or Clang |

---

## 14. Build

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
cd build && ctest --output-on-failure

# Run node + miner
./build/Release/bitvoid-core run --energy cpu

# Run explorer API
./build/Release/bitvoid-core explorer --port 8545
```

---

## 15. Future Work

- Smart contract VM (for on-chain DEX and DeFi)
- Cross-chain bridge to Ethereum/BSC
- Block explorer live deployment
- DEX implementation (AMM)
- Wallet GUI
- Mobile wallet
- SPV (Simplified Payment Verification) nodes
- Dynamic block size growth
- Per-address rate limiting
- Dynamic fee adjustment

---

*This specification is a living document. Updates will be published as the protocol evolves.*
