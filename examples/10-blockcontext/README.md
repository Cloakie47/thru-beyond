# 10-blockcontext — the rolling 512-block history window

**STATUS: program built (480 B), predictions committed (`bde3a78`),
deployment BLOCKED by the alphanet proof-root desync** — every `program
create` fails at the manager step with `Invalid state proof (-23)` (the
post-outage condition documented in the gotchas log; it has persisted
across sessions). All on-chain measurements below are pending chain
recovery; nothing in this file is a measured CU figure yet.

## What the spec says (read first, per process rules)

`/spec/vm/memory-layout.md`: the last **512 blocks** (TN_RUNTIME_CTX_BLOCK_SPAN)
map read-only at segment (0x00, 0x0004), `offset = blocks_ago * 0x1000`,
blocks_ago = 0 is the current block. Record: `slot` u64 @0x00,
`block_time` (Unix ns) u64 @0x08, `block_price` u64 @0x10, `state_root`
[32] @0x18, `cur_block_hash` [32] @0x38, `block_producer` [32] @0x58.
"Accessing further back or before slot 0 faults" — so the past-the-window
test is a CONFIRMS/CORRECTS test, not undocumented territory.

## What could be established without the deploy

**Timestamp resolution (Explorer side, 2026-08-02):** seven consecutive
blocks (slots 649113–649119) carry timestamps populated to the single
nanosecond — 1785699561720608707, …563167261219, …563698472820 — no
millisecond padding. Consecutive-slot deltas: 1.447 s, 0.531 s, 2.360 s,
1.592 s, 1.358 s, 2.999 s — block time on alphanet is variable, roughly
0.5–3 s. Preliminary verdict: **genuinely nanosecond-grade**, to be
corroborated by the program's own emitted `block_time` once deployed.

**Producer reality check:** all seven blocks were produced by the same key
(`taDLOptS8JTdTBtLWs_c…`). Alphanet currently has (at least effectively)
a single block producer — which makes the commit-reveal caveat below not
theoretical but literal: one party controls every block hash.

**Reference targets captured** for the match-verification (hash + producer
per slot) once the program can emit what the VM maps.

## The plan the predictions commit to

- `read_current` / `read_ago_n`: emit the whole 0x78-byte record; expected
  ≈ 5,4xx CU (floor + one emit + ~100 instructions) — **no syscall for the
  read itself** (the mapping claim under test).
- `read_many_n` at N = 1, 10, 100, 511: slope ~18–22 CU per block, exactly
  linear, **Pages Used flat at 1** even though N=511 touches 511 distinct
  read-only pages — the strongest re-test of the reads-are-never-charged
  law yet.
- `read_beyond` at N = 512, 513, 1024: predicted fault (`VM_FAILED`),
  confirming the spec; CU of failures unobservable (known CLI gap).
- `commit_reveal` (type 4): draws u64 = first 8 bytes of the hash N blocks
  back XOR a caller salt, emitted. **This is the draw primitive of a
  commit-reveal, and it is NOT safe randomness on its own: the block
  producer chooses transaction inclusion and can influence its own block's
  hash — and on today's alphanet a single producer makes every block.**
  The pattern is only meaningful with (a) the commitment fixed before the
  drawn block exists, (b) a reveal window enforced by slot, and (c) an
  economic assumption that the producer's manipulation value is below the
  stake at risk. The README ships the weakness statement, not a trap.

## Reproduce (once the chain accepts creates again)

```bash
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
thru $U program create example_10_blockcontext ./build/thruvm/bin/tn_example_10_blockcontext_c.bin
# args: <type u32><n u32><salt u64> LE, e.g.
thru $U txn execute --fee 0 <prog> 00000000000000000000000000000000   # read_current
```
