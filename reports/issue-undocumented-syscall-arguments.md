# Four reachable syscalls have undocumented argument shapes — documentation request

**Repo:** Unto-Labs/thru · **Kind:** documentation gap, not a bug report ·
**Network:** alphanet, CLI 0.3.2+54058649, SDK/toolchain 0.3.2 ·
**Measured:** 2026-08-02/03 (repo: github.com/Cloakie47/thru-beyond, raw
figures in `examples/12-ephemeral/results.json` and AUDIT.md)

## Summary

Four syscalls dispatch correctly from program code, validate their
arguments, and return specific error codes — they exist and are reachable.
What is missing in every case is the documented shape of one argument.
Nobody outside the team can currently call them. Each has a concrete,
small fix: publish the expected field layout.

All four were probed from deployed programs (via the upgrade path +
ephemeral accounts, so none of this depends on the current proof-service
state). Errors are the syscall's own return values, surfaced through
`tsdk_revert(marker + r)`.

## 1. `tsys_account_compress` (0x08) — rejects every obtainable proof, −43

- **Spec/SDK:** spec lists the syscall; the C header takes a proof
  pointer + size. Neither documents the expected proof *layout*.
- **Attempted:** (a) a fresh `creating` proof from
  `thru txn make-state-proof creating` (the only kind the proof service
  currently serves — `existing`/`updating` return "bintrie: key not
  found" even for live accounts); (b) proof size 0 / NULL, on a writable
  program-owned **ephemeral** account (the spec's any-party-GC case,
  where no bintrie state exists at all).
- **Rejection:** −43 in every combination, from two different programs.
- **Contrast:** the CLI's `thru account compress` works — the system
  program (`taAAAA…EB`) receives an internally-built proof embedded in
  instruction data (observed on-chain: instruction data = proof + 3-byte
  tag; 13 transactions verified). So a valid wire format exists; it just
  isn't published.
- **Unblock:** document the proof layout `0x08` expects (or state that
  the ephemeral/GC case wants size 0 with some flag), ideally with one
  worked example.

## 2. `tsys_account_decompress` (0x09) — same wall, −43

- **Spec/SDK:** listed; takes proof pointer + size.
- **Attempted:** creating proofs and size 0, mirroring the compress
  probes (the RPC refuses to serve `existing` proofs, so the presumably
  correct proof kind is unobtainable client-side).
- **Rejection:** −43.
- **Contrast:** the CLI decompression path works and its transactions
  show the wire format: instruction data = revived bytes + proof + 43-byte
  header (3 transactions verified, exact).
- **Unblock:** same as 0x08 — the proof layout, and which endpoint serves
  it.

## 3. `tsys_account_set_flags` (0x0E) — rejects flags 0 and 1, −41

- **Spec/SDK:** the spec's syscall table lists it; neither spec nor
  header documents a single legal flag value or precondition.
- **Attempted:** flags = 0 (×3) and flags = 1 (×1) on a writable
  ephemeral account owned by the calling program.
- **Rejection:** −41, all four transactions.
- **Unblock:** enumerate the legal flag bits and any account-kind /
  authorization preconditions.

## 4. `tsys_account_create_eoa` (0x0F) — rejects NULL signature/proof, −22

- **Spec/SDK:** **absent from the spec's syscall table entirely** (which
  ends at 0x0E); exists only in `tn_sdk_syscall.h`. Its signature takes
  what appear to be a signature/proof pointer + size.
- **Attempted:** NULL pointer, size 0, on a fresh derived address
  declared writable.
- **Rejection:** −22 — consistent with a mandatory signature or proof
  argument being validated and found missing.
- **Unblock:** document whether 0x0F is public API at all; if yes, the
  required signature/proof format; if no, note it as internal in the
  header.

## The common thread

None of these failures is an authorization refusal — each syscall runs
far enough to inspect its arguments and return a distinct validation
error (−43, −43, −41, −22). The wire formats demonstrably exist (two of
the four are exercised daily by the CLI through the system program). The
gap is purely documentation: four argument layouts. Publishing them would
turn four dead syscalls into usable API surface, and would let the
spec's own ephemeral-GC claim ("any program can compress a writable
ephemeral account") be verified by third parties — today it cannot be.
