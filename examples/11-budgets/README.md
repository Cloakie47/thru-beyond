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

**Two different targets, two different costs — say it explicitly:**
binary size drives **deployment** cost (~1 CU/byte through the
upload/finalize pipeline, paid once), while **runtime** CU depends on
*executed* instruction bytes on the hot path, paid every transaction.
These can rank the flags oppositely: **-O3 is plausibly the worst on the
first (largest binary, 2.9× -O1) and the best on the second** (its
unrolled SHA rounds may execute fewer total bytes per block than a rolled
loop's body-plus-branch iterations). The deployment half is measured
above; **the runtime half is pending** — deploys fail under the
proof-root desync — and no recommendation is issued until it exists.

## Reproduce

MU: any `thru txn get`-able signature via the Explorer MCP
(`get_transaction` → `memoryUnits.consumed`) — or just read the CLI's
`Pages Used`. SU ladder: ex02 `resize_to` on any owned account. O-levels:
compile `tn_example_04_hash_a.c` with the SDK's flag set, overriding the
trailing `-O` flag.
