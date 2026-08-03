# 11-budgets — the third budget, priced (and the second, nailed down)

Alphanet, thru CLI 0.3.2+54058649, 2026-08-02. Predictions committed at
`96efd57` before measurement. Chain caveat: alphanet spent this session in
the persistent proof-root desync (creates fail with −23) — which gated
Part C's CU measurements but nothing else, and incidentally demonstrated
that plain execution stays deterministic and proof-free operations are
unaffected (every re-run below reproduced its historical CU exactly).

## Part A — memory units: consumed MU = `Pages Used`. Exactly. Always.

The Explorer MCP reports consumed MU per transaction (`memoryUnits:
consumed`); the CLI never labels it. Sweeping 28 transactions — fresh
probes across examples 01/03/04/05/06/07 plus the resize ladder and
historical 08 compress/decompress signatures:

**MU = Pages Used in 28 of 28 transactions, no exceptions.** The CLI has
been printing consumed memory units all along, under the name `Pages Used`.

The deciding sub-case (predicted in advance): **event pages are charged
MU.** `emit_n(8)` → MU 9 (8 stack pages + 1 event page); `emit_n(4088)` →
MU 10. Event pages cost zero CU (example 05) but they are NOT free — they
consume the memory-unit budget. Also confirmed against every other page
source: 8-page stack grow → MU 8; CoW writes → 2/3; CPI depth 14 → MU 16;
resize 8→65536 → MU 17; noop → MU 1.

Sub-question CLOSED (same day): a `grow_shrink` instruction was added to
the 05 binary (grow the segment to 8 pages, set it back to 1 before
returning — the upgrade path works even during the desync). Result, 3×
identical, Explorer-confirmed: **MU = 8, Pages Used = 8 — MU charges
PEAK, not end-state**, and `Pages Used` tracks the same peak. CU = 34,665
= the return_only baseline + 538 (one extra segment syscall + setup): the
shrink refunds nothing and costs nothing. This CONFIRMS the spec's
"charged at peak usage, not cumulative" — and clarifies that the spec's
"shrinking releases them" refers to headroom for re-growth, not to the
billed figure.

## Part B — state units: SU = ceil(bytes grown / 4096), sharpened

Resize ladder on account d (ex02 program). Protocol: each growth target was
measured from a fresh reset to 8 bytes; the reset transactions themselves
are the shrink data. Every row states its actual starting size so the
ladder is reproducible as written (the full 13-transaction raw sequence,
including the interleaved resets, fits ceil(bytes grown/4096) at every
step):

| From | Target | Bytes grown | SU | CU above 5,995 baseline |
|---|---|---|---|---|
| 8 | 1 (shrink) | 0 | 0 | 0 |
| 1 | 8 | 7 | 1 | 7 |
| 8 | 4095 | 4,087 | 1 | 4,087 |
| 8 | 4096 | 4,088 | 1 | 4,088 |
| 8 | **4097** | 4,089 | **1** | 4,089 |
| 8 | 8192 | 8,184 | 2 | 8,184 |
| 8 | 65536 | 65,528 | 16 | 65,528 |
| any | 8 (reset/shrink) | 0 | 0 | 0 |

The 4097 target is the sharp discriminator: the *target* spans two pages
but SU = 1 — **SU bills ceil(bytes grown / 4096), not target pages.**
(CU meanwhile stays exactly 1 per byte grown, at every point.)
Create = 1, delete = 0, compress = 0, decompress = pages restored — all
historical figures; and **no operation ever releases or refunds SU**:
every shrink, delete, and compression observed consumed 0, never negative.

## Part C — optimization levels (sizes measured; CU gated on the desync)

Portable SHA-256 (04 arm A) compiled at six levels, same source, same
flags otherwise:

| Flag | Binary size | vs -O3 (default) |
|---|---|---|
| -O0 | 2,376 B | −32% |
| -O1 | 1,216 B | **−65%** |
| -O2 | 1,280 B | −63% |
| **-O3 (SDK default)** | **3,496 B** | — |
| -Os | 1,344 B | −62% |
| -Oz | 1,344 B | −62% |

**MEASURED (2026-08-02, via borrow-upgrade of 04A's own slot, restored
afterwards with the -O3 build reproducing the recorded 224,511 exactly):**

| Flag | Binary | Upgrade CU | Runtime CU @1,024 B | Runtime vs -O3 |
|---|---|---|---|---|
| -O0 | 2,376 B | 546,100 | 959,579 | **+327%** |
| **-O1** | **1,216 B** | **345,340** | **224,775** | **+0.12%** |
| -O2 | 1,280 B | 355,264 | 244,317 | +8.8% |
| -O3 (default) | 3,496 B | 734,292 | 224,511 | — |
| -Os | 1,344 B | 366,340 | 249,653 | +11.2% |
| -Oz | 1,344 B | 367,620 | 249,653 | +11.2% |

**The two-target verdict, outright:** deployment cost is minimized by
**-O1** (345,340 CU — -O3's default costs 2.13×); runtime is minimized by
**-O3** (224,511) — but **by only 264 CU (0.12%) over -O1**, which is
2.9× smaller. They are not the same flag, yet the choice is one-sided:
**-O1 is the recommendation** — near-identical runtime at a third of the
size and half the deploy cost. -Os/-Oz are dominated by -O1 on BOTH axes
on this target (bigger and 11% slower — the "optimize-for-size" flags
lose to plain -O1). -O0 is catastrophic: 4.27× runtime, forever. The
SDK's -O3 default buys 0.12% runtime for ~389,000 extra deploy CU per
upgrade of this program.

**Upgrade-cost law (all six points, least squares):**

```
4-txn subtotal ≈ 137,149 + 171.1 × binary bytes    (residuals ≤ ±0.43%)
```

It supersedes the earlier "≈170–195 CU per binary byte" phrasing, which
was wrong in form — quoting a pure per-byte rate for a line with a 137k
intercept doesn't reproduce any single point (345,340 / 1,216 = 284
CU/byte). Third occurrence of the same fitting error in this repo
(compress law, decompress law, now this): a constant absorbed into a
slope. Checked by `bench/verify.py` at 0.5% tolerance.

## The pipeline, dissected (2026-08-04) — deterministic, and 5 transactions, not 4

Two follow-ups dissolved what this section briefly published as an
"irreducible per-instance term":

**The recorded `upgrade_cu` figures are 4-txn subtotals.** The pipeline
runs FIVE transactions — buffer create, chunk write, finalize, upgrade,
cleanup — but the chunk-write signature is never printed (the same CLI
gap already known from `program create`), so the original sweep's
signature grep missed it. The temp-buffer account's seed is
deterministic, so its chain history holds every past run's five
transactions; the full breakdowns were reconstructed from there and the
4-txn sums reproduce all six recorded figures exactly.

| Stage | Law | Evidence |
|---|---|---|
| 1 buffer create | **11,723 + 1 CU/binary byte — exact** | 5 sizes, to the CU |
| 2 chunk write | **34,514 + 4.75 CU/byte — exact** (single-chunk regime, ≤30,720 B) | 5 sizes, to the CU |
| 3 finalize | ≈165–168 CU/byte, near-linear, NOT exact; the dominant stage | 252,644→629,830 across 1,216→3,496 B |
| 4 upgrade | depends on the SIZE TRANSITION old→new (see below) | 7 transitions |
| 5 cleanup | **35,782 flat** | 9/9 runs |

**Determinism: CONFIRMED.** Three consecutive upgrades of the identical
-O3 binary produced five identical stages, to the CU, all three times
(15,219 / 51,120 / 629,830 / 54,805 / 35,782). Stages 1–3 and 5 also
reproduced exactly across 110,000 slots (the 2026-08-02 -O3 run vs
2026-08-04 reruns).

**The -Os/-Oz "1,280 CU content difference" was neither content nor
noise.** Stage-by-stage, the two 1,344-byte binaries are identical in
four of five stages — including finalize, which processes the full
(different) contents. The whole difference sits in the Step-2 upgrade
transaction: -Os *grew* the program account 1,280→1,344 (43,303 CU, SU 1)
while -Oz *overwrote* 1,344→1,344 (44,583 CU, SU 0). Same effect on the
identical -O3 binary: grow 1,344→3,496 = 53,461 (SU 1); overwrite
3,496→3,496 = 54,805 (SU 0). The upgrade step is a deterministic function
of the size transition; **no content-dependence was observed anywhere in
the pipeline.** The 4-txn law above stands as a size-only approximation;
budget per stage (and per transition) when it matters.

## Reproduce

MU: any `thru txn get`-able signature via the Explorer MCP
(`get_transaction` → `memoryUnits.consumed`) — or just read the CLI's
`Pages Used`. SU ladder: ex02 `resize_to` on any owned account. O-levels:
compile `tn_example_04_hash_a.c` with the SDK's flag set, overriding the
trailing `-O` flag.
