# 11-budgets — predictions, committed BEFORE measurement

Date: 2026-08-02. Chain state: alphanet remains in the proof-root desync
(creates fail with −23), which gates Part C's deploys; Parts A/B use the
Explorer and proof-free executions on existing programs.

## Part A — memory units

**Headline prediction: consumed MU = `Pages Used`, exactly, in every
transaction.** The CLI's mystery meter and the unreported budget are the
same number. Evidence seed: the one MU value ever observed (ex08 compress,
via Explorer) was MU 1 with Pages 1. Refutation: any transaction where the
two diverge.

Consequences, each a sub-prediction:
- 01-noop: MU 1 (the entry stub's stack page).
- 05 `return_only` (8-page stack grow): MU 8.
- **Event pages ARE charged MU** (the interesting question): 05 `emit_n(8)`
  → MU 9; `emit_n(4088)` → MU 10 — event pages are CU-free but MU-metered,
  i.e. "counted in Pages" = "charged in MU". Alternative outcome: MU stays
  8 and event pages are free in both meters; the Pages column would then
  diverge from MU, refuting the headline.
- Account CoW writes: 03 `write_p0` MU 2, `write_p0_p1` MU 3.
- Resize up: 02's 8→65536 → MU 17 (1 + 16 zero-filled pages); shrink → MU
  1–2 (peak within the txn; never negative — "release on shrink" is only
  visible inside a single transaction's peak, not as a refund).
- CPI: 07 deep N=14 → MU 16 (one stack page per depth).

## Part B — state units

- Resize from 8 to {1, 4095, 4096, 4097, 8192, 65536}: SU =
  ceil(bytes grown / 4096) = {0 (shrink), 1, 1, **1**, 2, 16}. The 4097
  target sharpens the grown-bytes vs target-pages discriminator (target
  spans 2 pages; grown 4,089 bytes → predict SU 1).
- create = 1, delete = 0, compress = 0, decompress = pages restored
  (historical figures; fresh ones gated on the desync).
- **SU is never released or refunded by any operation** — all observed
  shrinks, deletes, and compressions consumed SU 0, never negative.

## Part C — optimization levels (portable SHA-256 from 04, fixed input)

- Binary size: -O0 ≈ 2–3× the -O3 size; -O1/-O2 within ~10% of -O3;
  -Os and -Oz 10–25% smaller than -O3.
- Measured CU at a fixed input: tracks encoding bytes on the hot path —
  -O0 ≈ **2–3× the -O3 CU**; -Os/-Oz ≈ **5–15% cheaper than -O3** (denser
  encodings; slight risk -Os loses to -O3 by unrolling less profitably —
  if -Os is *more* expensive, that refutes the "smaller is cheaper on the
  hot path" shortcut and the recommendation stays -O3).
- Recommendation expected: -Os (or -Oz if supported) as default; the wrong
  extreme (-O0) predicted to cost ~2–3× per transaction forever.
- Deploy-side: smaller binary also cuts the one-time deploy cost
  (~1 CU/byte through the chunk/finalize pipeline).
- Measurement gated on chain recovery; sizes measurable now.
