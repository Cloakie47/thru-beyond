# 12-ephemeral — ephemeral accounts survive a proof outage

Alphanet, thru CLI 0.3.2+54058649, 2026-08-02, measured DURING the
proof-root desync (permanent creates failing −23 in the same session).
Predictions committed at `9ead3fd`. Program: ex08
(`tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY`), which gained a
`create_ephemeral` instruction via upgrade — upgrades work during the
desync.

## The gating hypothesis: CONFIRMED

`tsys_account_create_ephemeral(idx, seed)` has no proof parameter, and it
shows: **ephemeral creation succeeded 3× (three seeds) while permanent
creation failed −23 minutes apart in the same session.**

| Measurement | Value |
|---|---|
| create_ephemeral CU | **6,303** (×3 identical) |
| SU | **0** (permanent create consumes 1 — nothing persists in the trie) |
| Pages / MU | 1 |
| Account state after | exists, `Is Ephemeral: Yes`, balance 0, data 0, owner = creating program |

Decomposition: 6,303 = 4,608 floor + 512 syscall + ~1,183 internal work —
the same order as permanent create's ~1,430 residual (derivation/hashing),
minus proof processing.

## Cannot hold funds: CONFIRMED

`thru transfer default <ephemeral> 1` → transaction reverts
(`VM error: -765`). The spec's "cannot hold funds" holds at the transfer
level.

## Any-party garbage collection: BLOCKED, not confirmed

The spec says any program can compress (= delete) a writable ephemeral
account without owning it. Both available paths failed:

- Program-side `tsys_account_compress` with **proof size 0** → syscall
  −43, the same undocumented-proof-shape wall as every other program-side
  compress attempt. Even ephemeral deletion apparently demands a proof
  argument of unknown layout.
- CLI `thru account compress <ephemeral>` → blocked at its own proof-fetch
  step ("bintrie: key not found") under the desync.

So the GC claim is UNVERIFIED here — untestable until the compress
syscall's proof expectations are documented or the desync clears. The
three ephemeral accounts remain live.

## Why this matters

Ephemeral accounts are the only account-creation path that keeps working
when the proof infrastructure degrades — scratch state for CPI pipelines,
session data, and intermediate computation stays available through
exactly the class of incident that froze everything else for hours.
