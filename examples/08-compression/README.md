# 08-compression — the full round trip, and the lag that masqueraded as a black hole

Program: `tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY` (996 B, seed
`example_08_compression`). Alphanet, thru CLI 0.3.2+54058649, node
0.0.0-local+599daf60, 2026-08-02, `--fee 0`. Predictions committed at
`a21831a` before measurement.

## CORRECTION (2026-08-02, discrimination session): the "black hole" was an
## indexing lag

An earlier revision of this README reported decompression as **impossible**
("compressed accounts are one-way black holes"). That claim was wrong, and
the way it was wrong is instructive: every retry in the original session
stayed within ~60 seconds of compression. A timed re-run on a fresh account
(PDA `tarvoZu7fh587rXrOTtW5bM_zCUY1UdULUJa50B0jAi0it`, compress sig
`tsub2BBaYbgVOUNG1yBMsnA3bTEg9GRzkNGDycXV7Ea3Y5lk4qD0-_NH_x_2BKTAGwka4za_aKyOd93fnSFlu9ChvV`,
slot 604530) shows:

- **T+0 and T+~1 min:** `make-state-proof existing`, `updating`, and
  `prepare-decompression` ALL fail with
  `RPC error: "bintrie: key not found"` — an error that reads as fatal.
- **T+~5 min:** all three succeed. The compressed leaf appears; proofs and
  account data are served.
- Every previously "lost" account then decompressed successfully.

**The query service indexes compressed leaves with a ~1–5 minute lag, and
during that window every decompression surface returns a fatal-looking
error.** Neither the lag nor the transient nature of the error is
documented anywhere.

Independent evidence (Explorer MCP, `scan.thru.org/api/mcp`, queried inside
the lag window): `get_account` → NOT_FOUND, while `get_transaction` fully
indexes the compress transaction — revealing that compression is executed
by the **system program** (`taAAAA…EB`, instruction type 0x05, the ~200-byte
state proof embedded in its 203-byte instruction data), signed only by the
fee payer, on a program-owned account. The owner program is not involved.
(Bonus: the Explorer reports **consumed memory units** — 1 MU for this
compress — which the CLI never shows.)

Still blocked, program-side: `tsys_account_compress` from our own program
reverts with syscall error −43 for both obtainable proof kinds, and the
syscall's expected proof shape is documented nowhere we could find (the
spec has no page for it). Time-boxed out; reported as a docs gap.

## What WAS measured: compression, at five sizes

Via `thru account compress` (CLI-built transaction), figures from
`txn get` on each signature. Proof column = the creating-proof size for the
same key (same trie path ⇒ same proof size; the CLI does not print the
proof it embeds — this inference is noted in AUDIT.md):

| Account size | Compress CU | SU | Proof | CU − S − proof |
|---|---|---|---|---|
| 8 B | 6,061 | **0** | 200 | **5,853** |
| 100 B (repro, out-of-sample) | 6,153 | **0** | 200 | **5,853** |
| 1,000 B | 7,117 | **0** | 264 | **5,853** |
| 5,000 B | 11,053 | **0** | 200 | **5,853** |
| 65,536 B | 71,621 | **0** | 232 | **5,853** |

```
compress = 5,853 + 1 CU × account bytes + 1 CU × proof bytes
```

Exact at all five sizes, slope exactly 1.000 on both terms.
*Correction (2026-08-02):* this section originally published
"≈ 6,053 + 1 CU/byte, slope 1.0005" — that fit absorbed the proof bytes
(which example 02 had already established cost 1 CU each) into a fake slope
and an intercept anchored on the two accounts that happened to carry
200-byte proofs. Apply the repo's own established laws before fitting new
ones.

## The economic question, answered plainly

**Compression refunds nothing. State Units Consumed = 0 at every size —
never negative.** The payer spends ~1 CU/byte to compress and gets no
state-unit credit back. Combined with delete also refunding nothing (ex02),
storage costs on Thru are one-way: compression relieves *validators* of
storage; it is not an economic mechanism for the account holder. (Whether
rent/SU pricing elsewhere makes it indirectly economic is outside what the
CLI exposes.)

## The rest of the round trip — measured after the lag cleared

| Operation | CU | SU | Pages |
|---|---|---|---|
| decompress 100 B | 6,211 | 1 | 2 |
| decompress 1,000 B | 7,079 | 1 | 2 |
| decompress 5,000 B | 11,111 | 2 | 3 |
| decompress 65,536 B (final txn of chunked flow) | 72,037 | 16 | 17 |
| modify (1-byte write, 100 B account, ×3 identical) | 5,565 | 0 | 2 |
| recompress (post-modify, 100 B) | 6,153 | 0 | 1 |

- **Decompression ≈ base + ~1 CU per byte revived** (slopes 0.96–1.01
  between the single-transaction points), far below the predicted 2–3 CU/B.
  SU = pages of data restored (1/1/2/16 = ceil(S/4096)), mirroring resize
  growth. Exact decomposition into data + proof terms awaits per-transaction
  proof sizes, which the CLI does not print for this flow.
- **Recompress = compress**: 6,153 = 5,853 + 100 + 200, the same formula to
  the CU as the first compression.
- **The 32 KiB single-transaction ceiling is real but routed around**: for
  65,536 B the CLI switched to a chunked flow (buffer create, 3 chunk
  uploads, decompress-from-buffer — "all 3 chunks verified in buffer").
  Large accounts are revivable, at multi-transaction cost. The exact
  single-transaction size boundary (~32,100–32,300 B by payload arithmetic)
  remains unprobed: UNVERIFIED.
- One-shot operations (compress, decompress, recompress per state) cannot
  be run 3×; `modify` was (3 identical). Cross-checks instead: five exact
  points on the compress formula, four decompressions with consistent
  slopes.

## Proof sizes and staleness

- Creating (absence) proofs observed across all accounts in this repo: 168,
  200 (×4), 232, 264 bytes — variable with tree position, consistent with
  the spec's path_bitset design. `existing`/`updating` proofs are served
  only for keys in the compressed bintrie, and only after the ~1–5 min
  indexing lag; for LIVE accounts they fail with "bintrie: key not found"
  (live accounts are not in that trie at all).
- **A 90-second-old creating proof was accepted** (create succeeded,
  identical 6,593 CU) — refuting the prediction that proofs are tightly
  slot-bound. The tolerance boundary (time or tree-conflict) is untested.

## Reproduce

```bash
U="--url https://rpc.alphanet.thru.org"
P="tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY"
# create+resize via the program (types 4/5), then:
thru $U account compress <pda>            # works; account leaves live state
# WAIT ~5 minutes — during the indexing lag every decompression surface
# fails with a fatal-looking "bintrie: key not found"
thru $U account prepare-decompression <pda>
thru $U account decompress <pda>          # works after the lag
```
