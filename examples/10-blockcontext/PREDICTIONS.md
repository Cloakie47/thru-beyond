# 10-blockcontext — predictions, committed BEFORE measurement

Date: 2026-08-02. Spec read first (`/spec/vm/memory-layout.md`): BLOCK_CTX
at segment (0x00, 0x0004); record layout slot/time_ns/price/state_root/
hash/producer at 0x00/0x08/0x10/0x18/0x38/0x58; spacing 0x1000 per block;
window "512 blocks (TN_RUNTIME_CTX_BLOCK_SPAN)"; "Accessing further back or
before slot 0 faults."

- **No syscall for reads — CONFIRMS-or-refutes test:** `read_many_n`'s total
  must reconcile as floor (4,608) + pure instruction/data bytes with no
  third 512 anywhere. The emit variants add exactly one emit (512 + 120
  event bytes + call overhead ≈ 690).
- **`read_many_n` slope: ~18–22 CU per block read** (one 8-byte `ld` = 4+8,
  addressing ~4, loop ~4), exactly linear. Each block record is its own
  4KB page, so N=511 crosses 511 read-only pages: **Pages Used stays flat
  at 1** (read-only pages are never counted — the strongest re-test of the
  03/05 law yet). Refutation: Pages growing with N or a superlinear slope.
- **Past the window: fault, per spec.** N = 512, 513, 1024 → transaction
  fails with `VM_FAILED (−767)`, `user_error` echoing a fault-related
  register (the segment is sized 512 × 0x1000; the load at 0x200000+ is
  unmapped). CONFIRMS if so; zeros or wrapped data would CORRECT the spec.
  CU of the failures unobservable (known CLI gap).
- **Data is real:** emitted hash and producer at a given slot match the
  Explorer's values for that slot. Timestamp is genuinely nanosecond-grade:
  the Explorer already shows non-round values (…902595840630); predict
  sub-millisecond digits present and consecutive-slot deltas ≈ observed
  block time (~0.4–1 s).
- `read_current` ≈ **5,4xx CU** total (floor + emit(120 B) + ~100 instr).
- Chain-state caveat: alphanet is currently in the post-outage proof-root
  desync (creates fail with −23). Deploying this example needs the manager
  create flow, so measurement is gated on chain recovery; predictions are
  committed regardless.
