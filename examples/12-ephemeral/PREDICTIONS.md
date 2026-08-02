# 12-ephemeral — predictions, committed BEFORE measurement

Date: 2026-08-02, during the alphanet proof-root desync (creates fail
−23; upgrades work). Spec read first (`/spec/accounts/account-model.md`):
ephemeral accounts require **no state proof** for creation
(`tsys_account_create_ephemeral(account_idx, seed)` — the signature has no
proof parameter), **cannot hold funds**, and "any program can compress
(delete) an ephemeral account if it's writable in the transaction" —
compression of an ephemeral "immediately deletes it".

## The gating hypothesis

**Ephemeral account creation survives the proof outage.** The desync
breaks only proof-consuming paths; ephemeral creation touches none.
Prediction: `tsys_account_create_ephemeral` from an upgraded ex08
SUCCEEDS while permanent creation fails −23 in the same session.
Refutation: any error — which would mean the no-proof claim hides an
internal proof dependency, itself a finding.

## Sub-predictions

- Cost of a bare ephemeral create: ≈ 4,608 floor + 512 (syscall) + ~150
  instructions ≈ **~5,3xx CU**; **SU 0** (nothing persists — refutation:
  SU 1 like permanent creates); Pages/MU 1 (no data written).
- `thru transfer` of 1 token INTO the ephemeral address **fails**
  ("cannot hold funds").
- Program-side `tsys_account_compress` with **proof size 0** on the
  ephemeral (ex08's existing compress instruction) **succeeds and deletes
  it** — which would also be this repo's first successful program-side
  compress call (the −43 failures were permanent accounts wanting real
  proofs). Account gone from live state afterward.
- CLI `thru account compress <ephemeral>` (any-party path): under the
  desync the CLI's own proof-fetch step may block it even though the
  operation shouldn't need one — observe and report which.
