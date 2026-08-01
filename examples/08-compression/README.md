# 08-compression — half a round trip: the other half is broken on alphanet

Program: `tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY` (996 B, seed
`example_08_compression`). Alphanet, thru CLI 0.3.2+54058649, node
0.0.0-local+599daf60, 2026-08-02, `--fee 0`. Predictions committed at
`a21831a` before measurement.

## THE BLOCKER (reported precisely, per the time-box rule)

**Decompression is currently impossible on alphanet.** Compression works and
removes the account from live state — and then no tool can produce the proof
needed to bring it back:

```
$ thru --url https://rpc.alphanet.thru.org account compress taJo3C0J33p3P-wQj9ZQ3wCWehUAI8c4fY3e251dPnVl9R
Success: Account compression completed successfully        # sig tsF7O6M5HE2k…, slot 553745

$ thru … getaccountinfo taJo3C0J33p3P-…
Error: Account not found for address: …                    # gone from live state, as designed

$ thru … txn make-state-proof existing taJo3C0J33p3P-…     # also: updating
Error: … RPC error: code: 'Unknown error', message: "bintrie: key not found"

$ thru … account prepare-decompression taJo3C0J33p3P-…
Error: Failed to prepare account decompression: … "bintrie: key not found"

$ thru … account decompress taJo3C0J33p3P-…
Error: … "bintrie: key not found"
```

Retried 60+ seconds later: identical. The query service cannot find the
compressed leaf that the chain just accepted, so `prepare-decompression`,
`decompress`, and every non-creating proof kind fail. **A compressed account
is, today, a one-way black hole.** Instructions 1–3 of this example
(decompress / modify / recompress) are therefore unmeasured — blocked, not
skipped. Three test accounts were knowingly sacrificed to establish this.

Also blocked, program-side: `tsys_account_compress` from our own program
reverts with syscall error −43 for both proof kinds we could fetch
(`creating`, and `existing` which fails at the RPC before a proof exists).
The proof shape the syscall expects is undocumented; the CLI's
`account compress` builds its transaction internally (requesting 100,316 CU)
and works — notably **signed only by the fee payer, on a program-owned
account**: compression is a system-level operation that does not involve the
owner program.

## What WAS measured: compression, at four sizes

Via `thru account compress` (CLI-built transaction), figures from
`txn get` on each signature:

| Account size | Compress CU | SU | Pages | Implied slope |
|---|---|---|---|---|
| 8 B | 6,061 | **0** | 1 | — |
| 1,000 B | 7,117 | **0** | 1 | 1.064 CU/B (vs 8) |
| 5,000 B | 11,053 | **0** | 1 | 0.984 CU/B (vs 1,000) |
| 65,536 B | 71,621 | **0** | 1 | 1.0005 CU/B (vs 8) |

**Compress ≈ 6,053 + 1 CU per byte of account data** — the account is hashed
at 1 CU/byte, consistent with the repo's per-byte law (Zknh-accelerated
hashing notwithstanding; the charge is per byte processed, not per hash
instruction).

## The economic question, answered plainly

**Compression refunds nothing. State Units Consumed = 0 at every size —
never negative.** The payer spends ~1 CU/byte to compress and gets no
state-unit credit back. Combined with delete also refunding nothing (ex02),
storage costs on Thru are one-way: compression relieves *validators* of
storage; it is not an economic mechanism for the account holder. (Whether
rent/SU pricing elsewhere makes it indirectly economic is outside what the
CLI exposes.)

## The 32 KiB decompression ceiling — analytic only (BLOCKED empirically)

Decompression must carry the full account data as instruction data:
6 (args) + 62 (meta) + 4 + S (data) + 4 + proof (observed creating proofs:
168–264 B; compressed-leaf proof sizes unobservable) + transaction envelope
(~300 B). Against the 32,768-byte transaction limit, the largest revivable
account in one transaction computes to **S ≈ 32,100–32,300 bytes**; 65,536
is impossible in a single transaction. **Unverified empirically** — the
decompression path is broken (above), so the failing size could not be
probed. If decompression cost follows the model, revival would also run
≈ 2–3 CU per byte revived (instruction-data byte + write byte + hash byte)
— likewise unverifiable today.

## Proof sizes and staleness

- Creating (absence) proofs observed across all accounts in this repo: 168,
  200 (×4), 232, 264 bytes — variable with tree position, consistent with
  the spec's path_bitset design. `existing`/`updating` proofs could not be
  obtained for ANY account, live or compressed ("bintrie: key not found")
  — the proof service currently serves absence proofs only.
- **A 90-second-old creating proof was accepted** (create succeeded,
  identical 6,593 CU) — refuting the prediction that proofs are tightly
  slot-bound. The tolerance boundary (time or tree-conflict) is untested.

## Reproduce

```bash
U="--url https://rpc.alphanet.thru.org"
P="tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY"
# create+resize via the program (types 4/5), then:
thru $U account compress <pda>          # works; account becomes unrecoverable
thru $U account prepare-decompression <pda>   # currently: "bintrie: key not found"
```
