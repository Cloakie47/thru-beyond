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

## 6,303 vs ~2,611: two different quantities, both correct

Example 09's fleet created 950 ephemeral accounts at ≈2,611 CU each;
this example reports 6,303. They measure different things:

- **6,303 (here)** = one complete transaction doing one create: 4,608
  floor + 512 create syscall + ~1,183 internal (derivation/hashing). No
  writable, no resize. Re-confirmed 2026-08-03 on the current binary:
  6,303, SU 0.
- **≈2,611 (fleet, 09)** = the *marginal* per-account cost inside one
  transaction: create 512 + `set_account_data_writable` 512 + resize
  512+8 + ~1,070 internal — the floor amortizes over 950 accounts
  (~5 CU each).

Like-for-like, the create-syscall-plus-internals term is ~1,695 here vs
~1,587 in the fleet; the ~108 CU difference is per-iteration codegen /
seed-handling between the two code paths (the fleet derives seeds from a
u16 index in a loop), the size of codegen deltas seen between binaries
throughout this repo. **Conditions to quote:** 6,303 = one account, own
transaction, no resize; ~2,611 = each account in a large batch, resized
to 8 B, floor amortized.

## Syscall probes hosted on this binary (2026-08-03)

The 08/12 binary also carried the coverage probes for three unexercised
syscalls (results in `results.json`; map in the root README):

- **0x03 `account_transfer` with SOURCE = fee payer: rejected, −10** (3×).
  The owner-side direction works — example 02's `pay_out` moves balance
  from a program-owned account to the fee payer at 5,453 CU (3×, balance
  change confirmed 100→85). Programs spend their own accounts' balance,
  never the fee payer's.
- **0x0E `account_set_flags`: blocked, −41** for flags 0 (3×) and 1 (1×)
  on a writable ephemeral this program owns. Expected flag values or
  preconditions are undocumented — UNVERIFIED what this syscall wants.
- **0x0F `account_create_eoa`: blocked, −22** with NULL signature/proof
  (size 0) on a fresh derived address. The error suggests a required
  signature or proof argument; its shape is undocumented and the syscall
  is absent from the spec (SDK header only). What it does remains
  UNVERIFIED beyond "exists, dispatches, and validates its arguments".

## Why this matters

Ephemeral accounts are the only account-creation path that keeps working
when the proof infrastructure degrades — scratch state for CPI pipelines,
session data, and intermediate computation stays available through
exactly the class of incident that froze everything else for hours.
