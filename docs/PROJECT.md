# BitVoid (BTVD) — Project Specification

## Overview

BitVoid is a custom Layer-1 blockchain built from scratch, designed to fix Bitcoin's biggest flaws: energy waste, slow transactions, and unsustainable supply model. It uses a novel consensus mechanism called Proof of Energy (PoE), event-driven block production, and infinite supply with ramped emission.

**Name:** BitVoid
**Ticker:** BTVD
**Tagline:** "Energy when needed, not 24/7 waste"
**Type:** Custom Layer-1 blockchain (built from scratch, not a fork)

---

## 1. Consensus: Proof of Energy (PoE)

### Concept
Replace Bitcoin's meaningless hash puzzles with energy as the core measured metric. Miners pour energy into the network. Each block records exactly how many kWh were consumed to produce it. Energy is the measured, transparent, recorded input — not a hidden byproduct.

### How It Works
- Miners perform verifiable computation (cryptographic proof that energy was spent)
- Each block records: kWh consumed, watt-seconds, declared energy source
- The blockchain becomes a public ledger of energy consumption
- Competition metric is verified energy contribution, not just hash speed

### Block Energy Record
```
Block #850,001
├── Miner:          0x7a3f...b2c1
├── Energy used:    1,200 kWh
├── Watt-seconds:   4,320,000,000
├── Energy source:  hydro (declared)
├── Hash proof:     0x0000000004a2f...  (proves work was done)
├── Reward:         0.5 BTVD
└── Transactions:   [tx1, tx2, ...]
```

### Why PoE Over PoW
| | Bitcoin (PoW) | BitVoid (PoE) |
|---|---|---|
| What miners do | Solve useless hash puzzles | Provide/prove energy |
| Energy's role | Wasted side effect | The actual measured input |
| Transparency | Hidden behind hashes | Recorded per block, fully visible |
| Narrative | "Wastes energy" | "Backed by real watts" |

---

## 2. Block Production: Event-Driven

### Concept
No fixed block time. Blocks are created when the network needs them — based on transaction demand, not a clock.

### Triggers
A block is mined when ANY of these conditions are met:
1. **Transaction count threshold:** 100+ pending transactions
2. **Value threshold:** $100K+ pending transaction value
3. **Safety floor:** 60 seconds elapsed with no block (prevents chain stall)

### Timing
- **Minimum block gap:** 3 seconds (prevents propagation issues)
- **Maximum block Gap:** 60 seconds (safety floor)
- **Average block time:** 3-15 seconds (depending on traffic)
- **Confirmation time:** 6-9 seconds (2 blocks)
- **TPS:** 300-600

### Difficulty Adjustment
- Adjusts based on average block interval over last 1,000 blocks
- Target average: ~5 seconds
- If blocks come too fast → increase difficulty
- If blocks come too slow → decrease difficulty

### Spam Protection
- Minimum block interval (3 sec) prevents rapid-fire blocks
- Transaction fees required (no free transactions)
- Rate limiting per address
- Dynamic fee that rises when blocks come too fast
- Max block size: 2MB (keeps propagation fast)

### Energy Proportionality
- Low demand → blocks come slowly → low energy use
- High demand → blocks come fast → high energy use
- Energy scales with actual network usage (unlike Bitcoin's constant 24/7 burn)

---

## 3. Supply Model: Infinite with Ramped Emission

### Concept
No max supply. No halvings. Steady, predictable emission forever. Miners are always paid.

### Reward Schedule
```
Year 1:    0.5 BTVD/block    → ~5.25M coins mined
Year 2:    1 BTVD/block      → ~10.5M more
Year 3:    1.5 BTVD/block    → ~15.75M more
Year 4+:   2 BTVD/block      → ~21M/year forever
```

### Supply Over Time
```
Year 1:    5.25M BTVD
Year 2:    15.75M BTVD
Year 3:    31.5M BTVD
Year 5:    73.5M BTVD
Year 10:   157.5M BTVD
Year 20:   367.5M BTVD
Year 50:   ~1B BTVD
Forever:   keeps going
```

### Inflation Rate (Naturally Decreasing)
```
Year 1:    High (but low supply = price appreciation potential)
Year 5:    ~20%
Year 10:   ~10%
Year 20:   ~5%
Year 50:   ~2%
Year 100:  ~1%
→ Asymptotically approaches 0%
```

### Why No Max Supply
1. Miners always paid — solves Bitcoin's 2140 problem
2. No halving cycles — no artificial speculation bubbles
3. Early scarcity preserved — ramped reward means year 1 coins are scarce
4. Designed to be spent, not hoarded — works as actual money
5. Predictable and transparent — everyone knows the emission schedule

---

## 4. Additional Technical Features

### Post-Quantum Signatures
- Use post-quantum cryptographic signatures from day 1
- Immune to future quantum computer attacks (Bitcoin is not)
- Candidate algorithms: CRYSTALS-Dilithium, FALCON, or SPHINCS+

### Dynamic Block Size
- Starts at 2MB
- Can grow over time as network capacity increases
- Governed by consensus rules (not arbitrary hard fork)

### Fungibility
- All coins are equal (no tainting/blacklisting)
- No address blacklisting at protocol level

### Declared Energy Source
- Miners can optionally declare their energy source (solar, hydro, wind, nuclear, etc.)
- Recorded on-chain per block
- Enables "green mining" verification
- Not enforced — purely transparent/optional

---

## 5. DEX Strategy

### Phase 1: Native DEX on BitVoid Chain
- Build a simple AMM DEX on the BitVoid chain (Uniswap V2-style logic adapted to BTVD)
- Provides BTVD/BTC, BTVD/USDT, BTVD/ETH pairs via wrapped assets
- Full control, no external dependencies

### Phase 2: Bridge to External Chains
- Create a trustless bridge to Ethereum and/or BSC
- Wrap BTVD as an ERC-20/BEP-20 token
- List on Uniswap (Ethereum) and/or PancakeSwap (BSC)
- Enables external liquidity and broader access

### Phase 3: CEX Listings
- Tier 3 exchanges (free or cheap listings)
- Tier 2 exchanges (Gate.io, MEXC) when volume justifies
- Tier 1 exchanges (Binance, Coinbase) at scale

### Liquidity Locking
- Lock initial DEX liquidity for 6-12 months
- Use a liquidity locker protocol
- Proves the project is not a rug pull
- Major trust signal for early investors

### Trackers
- Submit to CoinGecko (free)
- Submit to CoinMarketCap (free)
- Submit to DexScreener, DEXTools, GeckoTerminal (free/automatic)

---

## 6. Mining Strategy

### Early Days (Weeks 1-4)
- CPU-mineable algorithm (no ASIC advantage at start)
- Low initial difficulty — normal PCs can find blocks
- Creator mines alongside community (fair launch, same rules)
- 3-5 friends + community members mining from day 1
- One VPS node ($10-20/month) running 24/7 as backbone

### Growth Phase (Months 1-6)
- 20-100+ miners as word spreads
- Difficulty increases naturally
- Easy one-click miner software (download, double-click, mining)
- No config files, no command line
- Shows real-time earnings ("You've mined 2.3 BTVD today")

### Maturity (Months 6+)
- 1000+ miners
- Specialized mining may emerge (acceptable at this stage)
- Creator stops mining, focuses on development
- Network is self-sustaining

### Fair Launch Rules
- No premine (0 coins created before public launch)
- No instamine (no secret mining before announcement)
- No dev allocation (no special coins for developers)
- Announce first, mine second
- Same rules for everyone, including creator

---

## 7. Technical Stack

### Core Blockchain
- **Language:** C++ (confirmed)
- **Built from scratch:** Not a fork of any existing chain
- **Research-driven:** Extensive internet research during implementation for security best practices and production patterns

### Script Wrappers (User-Facing Tools)
Simple shell/batch scripts that wrap the C++ binary for easy use:

```
mine.bat / mine.sh     → Starts mining (double-click, done)
node.bat / node.sh     → Runs a full node (syncs chain, validates blocks)
wallet.bat / wallet.sh  → Creates wallet, sends transactions, checks balance
explorer.bat / explorer.sh → Runs a local block explorer (web UI)
```

- Scripts auto-download the C++ binary from GitHub releases if not present
- No installation, no config files, no command line needed
- Cross-platform: Windows (.bat), Linux/Mac (.sh)
- Transparent: anyone can read the script and see exactly what it does
- ~20-30 lines per script — simple, auditable, trustworthy

---

## 8. Website & Hosting

### Hosting
- **Platform:** GitHub Pages (free, Git-based, automatic SSL)
- **Domain:** To be purchased (~$10-15/year)
- **Site type:** Static landing page

### Website Content
```
Landing page:
├── Hero: "BitVoid — Energy when needed, not 24/7 waste"
├── Whitepaper link
├── How to mine (guide)
├── Roadmap
├── Discord link
├── GitHub link
└── Block explorer link (when live)
```

---

## 9. Community Strategy

### Step 1: Create Presence (Week 1)
- Discord server (free)
- GitHub repo with whitepaper (free)
- Twitter/X account (free)
- Reddit account (free)
- r/BitVoid subreddit (create early, even with 0 subscribers)

### Step 2: Seed with Friends (Week 1)
- Invite 3-5 friends to Discord
- Ask them to invite 1-2 people each
- Share whitepaper, get feedback, make them feel involved
- Target: 10-20 people to start

### Step 3: Post in Crypto Communities (Week 1-2)
- BitcoinTalk forum: "Alternate Cryptocurrencies" / "Announcements" section
- Reddit: DM moderators of r/CryptoCurrency, r/altcoin, r/CryptoTechnology, r/CryptoCurrencyMining asking permission to post technical content
- Twitter/X: post development progress with #crypto #blockchain hashtags
- Discord crypto servers: join mining communities, be helpful, mention project naturally

### Reddit Rules (Critical)
- Always disclose affiliation: "I'm the creator of BitVoid"
- No price predictions, no "get in early", no referral links
- Account age matters: most subs require 30-90 day old accounts with 50+ karma
- Don't spam across multiple subs
- Contribute first, mention project second
- Technical depth wins: "here's how my PoE consensus works" > "buy my coin"
- Create r/BitVoid early — your own subreddit, your own rules

### Community Growth Timeline
| Size | Timeline | How |
|---|---|---|
| 0 → 10 | Week 1 | Friends + direct invites |
| 10 → 50 | Month 1 | Reddit, BitcoinTalk, Twitter |
| 50 → 200 | Month 2-3 | Word of mouth, development progress |
| 200 → 1000 | Month 3-6 | Exchange listings, mining going live |
| 1000+ | Month 6+ | Narrative carries itself |

### Key Principle
Regular development progress attracts people. "Just implemented the block structure, here's a screenshot" gets more interest than any marketing. People want to see it's real.

---

## 10. Roadmap

### Phase 1: Design & Documentation (Current)
- [x] Concept design
- [x] Consensus design (PoE)
- [x] Block production design (event-driven)
- [x] Supply model (infinite ramped)
- [x] Ticker selection (BTVD)
- [x] Whitepaper
- [x] Technical stack decision (C++ from scratch)
- [x] Website hosting decision (GitHub Pages)
- [x] Community strategy
- [ ] Community setup (Discord, GitHub, Twitter/X, r/BitVoid)
- [ ] Technical specification document

### Phase 2: Development
- [ ] Core blockchain implementation (C++)
- [ ] PoE consensus implementation
- [ ] Event-driven block production
- [ ] Post-quantum signatures
- [ ] Wallet software (C++ core + script wrapper)
- [ ] One-click miner (script wrapper)
- [ ] Block explorer
- [ ] Native DEX
- [ ] Website (GitHub Pages landing page)

### Phase 3: Launch
- [ ] Public announcement
- [ ] Genesis block creation
- [ ] Software release (open source on GitHub)
- [ ] Mining begins (fair launch)
- [ ] VPS node deployed ($10-20/month)
- [ ] Community mining from day 1

### Phase 4: Growth
- [ ] DEX liquidity pool created
- [ ] Liquidity locked
- [ ] CoinGecko / CMC listing
- [ ] Tier 3 CEX listings
- [ ] Bridge to external chains
- [ ] Tier 2 CEX listings

### Phase 5: Scale
- [ ] 1000+ miners
- [ ] Tier 1 CEX listing
- [ ] Developer ecosystem
- [ ] Real-world adoption

---

## 11. Key Differences from Bitcoin

| Feature | Bitcoin | BitVoid |
|---|---|---|
| Consensus | PoW (useless hashes) | PoE (measured energy) |
| Energy | 24/7 waste, hidden | Proportional to usage, transparent |
| Block time | 10 min fixed | 3-60 sec event-driven |
| TPS | 7 | 300-600 |
| Confirmation | 10-60 min | 6-9 sec |
| Supply | 21M cap | Infinite (ramped) |
| Miners paid | Until ~2140 | Forever |
| Halvings | Yes (speculation cycles) | No (smooth emission) |
| Philosophy | Hoard | Spend |
| Quantum-safe | No | Yes |
| Inflation | → 0% | → 0% asymptotically |
| Block production | Time-driven | Event-driven |
| Energy transparency | None | Per-block kWh recording |

---

## 12. Narrative (3 Sentences)

1. **"Energy when needed, not 24/7 waste"** — event-driven blocks + PoE
2. **"Every coin backed by real watts"** — transparent energy per block
3. **"Confirm in 6 seconds, paid forever"** — fast blocks + infinite supply
