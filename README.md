# Beyond — Thru and Beyond

Working, deployed, cost-measured example programs for the Thru blockchain.
See `CLAUDE.md` for the rules and workflow.

## The cost model

Built by examples [01](examples/01-noop/README.md),
[03](examples/03-storage/README.md), [04](examples/04-hash/README.md), and
decided by [05](examples/05-events/README.md):

```
CU ≈ 4,096 × (charged pages) + 512 × (charged syscalls)
     + 1 × (data bytes processed) + instruction bytes
```

**What counts as a charged page — measured, not assumed:**

| Page type | Charged? | Counted in `Pages Used`? | Evidence |
|---|---|---|---|
| Anonymous page (stack/heap) at segment **allocation** | **4,096/page** (= 1 CU per zero-filled byte) | yes | 05: growing 1→8 pages = 7×4,096 + 512 + 28, exact |
| Account data growth at **resize** | **1 CU per byte grown**; full pages ⇒ 4,096/page | yes | 02: 8→{4096,4104,8192,16384,65536} = +{4,088, 4,096, 8,184, 16,376, 65,528}, exact; shrink free (constant CU) |
| Account data page, **first write** (copy-on-write) | **1 CU per byte copied** — 4,096 only for a full page | yes | 03: full-page account, +4,096/page exactly; 02: 8-byte account, increment = 5,523 with **no** 4,096 term |
| Account data page, read | no | no | 03: `read_p1` +129 CU over baseline, Pages unchanged |
| Already-mapped anonymous page, write | no | no change | 05: `touch_stack` +15/+15/+11 CU per extra page, Pages flat |
| Event buffer page | **no** | **yes** | 05: emit cost = 570 + 1·N exactly, continuous through both page boundaries while `Pages Used` steps 9→10→11 (boundary at N+10 per event: 10-byte record overhead, measured per-event via a two-event probe) |

`Pages Used` is therefore a mixed meter — it counts charged pages *and*
uncharged event pages. Do not read it as CU/4,096.

**The instruction term is now measured, not assumed**
([06-instructions](examples/06-instructions/README.md)) — and it **confirms
the spec** (`/spec/runtime/resources.md`) at ratio 1.000: 1 CU per byte of
instruction or data processed is the spec's own headline rule, and the
4 CU / 2 CU encoding rates, the per-width load costs, *and the per-width
store costs* (lb/sb 1, lh/sh 2, lw/sw 4, ld/sd 8 — the spec even works the
`sd` example: 4 + 8 = 12 CU) are all documented there. **This repo wrongly
assumed stores were free** — that unread spec page is what opened the
"36 CU gap" in 01-noop's reconciliation. Verification is still worth
having: same four instructions assembled full-width cost exactly double
(bytes, not count, as the spec says), and every rate measured 1.000 against
disassembled ground truth. But these are CONFIRMS-SPEC results, not
discoveries.

For account data the per-byte law is verified at byte granularity — and it
**corrects the spec**, whose resources page says "each page fault costs
exactly 4,096 compute units". Measured: a CoW copy charges **1 CU per byte
present in the copied page** — min(4,096, bytes in page) — at 8, 92, 100,
**904 (partially filled trailing page)**, and 4,096 bytes, each predicted
before measurement. 4,096 is the full-page special case. For anonymous
segments the per-byte reading is **UNVERIFIED and unverifiable as stated**:
`set_anonymous_segment_sz` rejects non-page-multiple sizes, so "4,096 per
page" vs "1 CU per zero-filled byte" cannot be distinguished — treat
anonymous allocation as 4,096 per page (which *confirms* the spec's number;
the charge-at-mapping timing is undocumented). See [AUDIT.md](AUDIT.md) for
the falsifiability review; anything labeled UNVERIFIED there is not
established.

## Findings ledger — what confirms, corrects, or extends the spec

Strict rule applied: if the spec says it anywhere, it is CONFIRMS, not
UNDOCUMENTED.

**CONFIRMS SPEC (11):** 1 CU per instruction-encoding byte (4/2 rates);
per-width load costs; per-width store costs; cost tracks bytes not
instruction count (the core rule); 512 syscall base with per-call extras on
top; anonymous page allocation = 4,096 per page (magnitude); CoW
copy-on-first-write mechanism; per-byte charge on event/log/proof payload
(core rule); deterministic execution (150+ measurement cells identical
across runs); CPI instruction data delivered in registers a0/a1; account
indices transaction-global under CPI ("same transaction context").

**CORRECTS SPEC (7):** CoW cost is per byte present in the copied page, not
"exactly 4,096 per page fault"; reads of untouched account pages incur no
page-fault charge at all; `tsys_exit` costs 0 despite the blanket 512
syscall base; the CLI's `--compute-units` help text contradicts itself
(prose 1,000,000,000 vs actual default 300,000,000); maximum call depth is
15, not the SDK header's "16 call depths (1..16)"; a callee's `tsdk_revert`
aborts the whole transaction — the C reference's catch-via-`invoke_err`
pattern only applies to syscall-level invoke errors; **decompression of
compressed accounts is impossible on alphanet as deployed** (every
decompression-side RPC fails with "bintrie: key not found" on freshly
compressed leaves — compressed accounts are currently one-way).

**UNDOCUMENTED (17):** entry stub maps exactly one 4KB stack page and the
stack never grows on demand; every transaction's floor therefore includes
512 + 4,096 before user code; program image including .data/.bss is
read-only — no writable globals; .bss is stored in the image (binary
inflation); event pages are counted in `Pages Used` but never CU-charged;
10-byte per-event record overhead; zero-length emit records no event;
resize growth = 1 CU/byte grown with constant-cost shrink; resize SU =
ceil(bytes grown/4096); delete requires data size 0 and refunds nothing;
`set_anonymous_segment_sz` rejects non-page-multiple sizes; failed
transactions report no consumed CU anywhere; `Pages Used` is a mixed meter;
Zknh SHA-256 speedup is 1.30× (quantified); CPI pricing (1,511 CU per
same-depth hop, +4,096 per depth level, frame pages reused across
sequential hops); the SDK txn accessors read top-level data, so
quickstart-pattern programs cannot be CPI callees; CPI events attribute to
the emitting frame's program.

**UNVERIFIED (10):** the per-byte reading of anonymous allocation; the
~1,430 CU create-syscall residual attribution; 04's intercept instruction
term (never independently counted); the docs-figure reconciliation
attribution; the event-header layout interpretation; deployment cost as a
function of binary size (single sample); the `user_error` register-echo
interpretation; the invoke syscall's ~256 CU register-save surcharge
(consistent-with, not isolated); the ~32,100–32,300-byte single-transaction
decompression ceiling (analytic only — decompression is broken); the
creating-proof staleness boundary (90 s accepted; limit untested).

To the UNDOCUMENTED list add (→ 21): compression pricing ≈ 6,053 + 1 CU per
byte of account data hashed; compression refunds no state units at any size
(one-way cost — validator-side, not economic for the payer); compression is
fee-payer-signable on program-owned accounts (system-level operation); the
proof service currently serves absence (creating) proofs only.

Out-of-sample check against the docs' quickstart (104-byte proof): the model
predicts create = 7,695 vs the docs' 7,524 (+2.27%) and increment-with-event
= 6,065 vs 5,980 (+1.42%). Residuals are shape/SDK-version sized but
**unexplained** without the quickstart's binary.

Known open item: 01-noop's floor reconciles to within **4 CU** (was 36 —
the difference was store bytes, now measured); the final 4 CU is
unexplained.

**What counts as a charged syscall:** every syscall measured so far costs the
512 base (`tsys_emit_event`: 570 total incl. call setup;
`set_anonymous_segment_sz`: 512 + setup) — **except `tsys_exit`, which
measures as free**. Every program also implicitly pays 512 + 4,096 before
`start()` runs: the SDK entry stub maps one stack page via
`set_anonymous_segment_sz`. That is the floor of every Thru transaction:
01-noop = 512 + 4,096 + 155 instruction CU, exact.

Validation of the floor (alphanet, thru CLI 0.3.2+54058649, 2026-08-01/02;
"syscalls" = charged syscalls including the entry stub's, exit excluded):

| Instruction | Charged pages | Charged syscalls | Floor | Measured | Residual |
|---|---|---|---|---|---|
| 01-noop | 1 | 1 | 4,608 | 4,763 | 155 |
| 03 `read_p1` | 1 | 1 | 4,608 | 4,892 | 284 |
| 03 `write_p0` | 2 | 2 | 9,216 | 9,582 | 366 |
| 03 `write_p0_x2` | 2 | 2 | 9,216 | 9,555 | 339 |
| 03 `write_p0_p1` | 3 | 2 | 13,312 | 13,659 | 347 |
| 05 `return_only` (8-page stack) | 8 | 2 | 33,792 | 34,107 | 315 |
| 05 `emit_n(8)` | 8 | 3 | 34,304 | 34,847 | 543* |

\* includes the 8 emitted bytes and event overhead (570 + 8 − 512 = 66) plus
instructions.

**Domain of validity, stated plainly:**

- Derived from examples 01–05 on alphanet, CLI 0.3.2, node
  0.0.0-local+599daf60. Single program per transaction, no cross-program
  invocation, no heap segment measurements yet.
- Syscalls with large data payloads carry per-byte cost beyond the 512 base —
  03's `init` (account create with a 232-byte state proof, plus resize)
  measured 15,913 CU, far above its floor.
- The instruction-bytes term cannot be estimated from first principles to
  better than ~20–35% (04's committed predictions). Measure one size and
  calibrate; linearity then predicts other sizes to ~0.01% (04, and the
  emit series in 05: 570 + N exact at 13 points).
- Residuals vary by ±30 CU from codegen alone: a handler doing strictly
  *more* work measured 27 CU *less* (03's `write_p0_x2` vs `write_p0`).
- Event emission: 570 + 1 CU per byte, no page charge, but the event pages
  inflate `Pages Used` (~8–92 bytes of record header shares the page).

**Two rules that fall out of the measurements:**

1. **Keep hot fields within the first 2,047 bytes of account data.** Offsets
   beyond that exceed RISC-V's 12-bit signed immediate and cost extra
   addressing instructions — measured at +8 CU for offset 4096
   (03: `write_p0_p1` − `write_p0_x2` = 4,104, i.e. 4,096 page + 8 addressing).
2. **Denser codegen is literally cheaper.** CU bills per instruction byte:
   a compressed 16-bit encoding costs 2 CU where a full-width 32-bit
   instruction costs 4. Compiler flags and code shape directly change the bill.

## Cost table

All figures measured on alphanet with `thru txn execute --fee 0`, three
identical runs each. Consumed units from real CLI output — never estimated.

| # | Example | CU | SU | Pages | Events | Binary | Program account |
|---|---|---|---|---|---|---|---|
| [01-noop](examples/01-noop/README.md) | Empty entrypoint, returns success | 4,763 | 0 | 1 | 0 | 138 B | `taIjGXEaz6jCa8ORd1YWClEQgbxCdw-hDSpzGtYkZAXk-_` |
| [02-counter](examples/02-counter/README.md) | Account lifecycle: create 7,791 / increment 5,523 / +event 6,065 / resize 1 CU per byte grown / delete 5,401, no refund | 5,401–71,495 | 0–16 | 1–17 | 0–1 | 952 B | `taog1g-QXdnJHjWg3o_wrzHmKEMm01_mAWU142rFNeic4s` |
| [03-storage](examples/03-storage/README.md) | Page-fault cost experiment (4 instructions) | 4,892–13,659 | 0 | 1–3 | 0 | 838 B | `tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1` |
| [04-hash](examples/04-hash/README.md) | SHA-256, portable C (arm A), 0–4096 B input | 18,959–841,119 | 0 | 2 | 1 | 3,496 B | `ta-rWexuBmL558uxLZXqOb23DM0HeThZGSxG2mOm3-6oxv` |
| [04-hash](examples/04-hash/README.md) | SHA-256, Zknh instructions (arm B) | 15,997–648,589 | 0 | 2 | 1 | 2,744 B | `taUgLhBWu3NCyYud3ioz-8XS-K8ly2BxzHk3-HRaQ0MMcb` |
| [05-events](examples/05-events/README.md) | Page-charge experiment (4 instr, 8-page stack) | 34,107–165,919 | 0 | 8–11 | 0–1 | 534 B | `tay1XampjPF__geQXy0YoyM24cCKzL6_AcTS-VTV2C-Add` |
| [06-instructions](examples/06-instructions/README.md) | Instruction term measured directly (spin loops, log/grow probes) | 4,924–164,928 | 0 | 1–2 | 0 | 420 B | `ta10jkQhjY5E8XIahXpzTlhDYhv8bLkbtYHWuZAlJC1LVu` |
| [07-cpi](examples/07-cpi/README.md) | CPI: 1,511/hop, +4,096/depth, max depth 15 | 4,925–89,038 | 0 | 1–16 | 0–1 | 848 B + 734 B | `ta9TmfhHffn5hJ3P83hC8NtwERjworfg7pSGxU_GrEPEmy` |
| [08-compression](examples/08-compression/README.md) | Compress ≈ 6,053 + 1 CU/B, SU refund = 0; **decompression broken on alphanet** | 6,061–71,621 | 0 | 1 | 0 | 996 B | `tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY` |

Deploying an 838 B program cost **320,292 CU** across five transactions
(measured breakdown in the 03 README).

Example 04 tested the model against predictions committed before measurement:
structure survived (exact linearity — 12,846 CU per 64-byte block portable,
9,884 with Zknh, identical intercepts of 6,129), point predictions missed by
19–34% (full reckoning in the 04 README). **Zknh SHA-256 speedup: 1.30×.**
Example 05 settled the page question: event pages are counted in `Pages
Used` but never charged; anonymous pages charge 4,096 at allocation.

All figures: thru CLI 0.3.2+54058649, alphanet, 2026-08-01.
