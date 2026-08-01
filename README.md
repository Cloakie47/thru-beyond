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

| Page type | Charged 4,096? | Counted in `Pages Used`? | Evidence |
|---|---|---|---|
| Anonymous page (stack/heap) at segment **allocation** | **yes** | yes | 05: growing 1→8 pages = 7×4,096 + 512 + 28, exact |
| Account data page, **first write** (copy-on-write) | **yes** | yes | 03: +4,104 = 4,096 + 8 addressing, `Pages Used` +1 exactly |
| Account data page, read | no | no | 03: `read_p1` +129 CU over baseline, Pages unchanged |
| Already-mapped anonymous page, write | no | no change | 05: `touch_stack` +15/+15/+11 CU per extra page, Pages flat |
| Event buffer page | **no** | **yes** | 05: emit cost = 570 + 1·N exactly, continuous through both page boundaries while `Pages Used` steps 9→10→11 |

`Pages Used` is therefore a mixed meter — it counts charged pages *and*
uncharged event pages. Do not read it as CU/4,096.

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
| 02-counter | *(not yet measured)* | | | | | | |
| [03-storage](examples/03-storage/README.md) | Page-fault cost experiment (4 instructions) | 4,892–13,659 | 0 | 1–3 | 0 | 838 B | `tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1` |
| [04-hash](examples/04-hash/README.md) | SHA-256, portable C (arm A), 0–4096 B input | 18,959–841,119 | 0 | 2 | 1 | 3,496 B | `ta-rWexuBmL558uxLZXqOb23DM0HeThZGSxG2mOm3-6oxv` |
| [04-hash](examples/04-hash/README.md) | SHA-256, Zknh instructions (arm B) | 15,997–648,589 | 0 | 2 | 1 | 2,744 B | `taUgLhBWu3NCyYud3ioz-8XS-K8ly2BxzHk3-HRaQ0MMcb` |
| [05-events](examples/05-events/README.md) | Page-charge experiment (4 instr, 8-page stack) | 34,107–165,919 | 0 | 8–11 | 0–1 | 534 B | `tay1XampjPF__geQXy0YoyM24cCKzL6_AcTS-VTV2C-Add` |

Deploying an 838 B program cost **320,292 CU** across five transactions
(measured breakdown in the 03 README).

Example 04 tested the model against predictions committed before measurement:
structure survived (exact linearity — 12,846 CU per 64-byte block portable,
9,884 with Zknh, identical intercepts of 6,129), point predictions missed by
19–34% (full reckoning in the 04 README). **Zknh SHA-256 speedup: 1.30×.**
Example 05 settled the page question: event pages are counted in `Pages
Used` but never charged; anonymous pages charge 4,096 at allocation.

All figures: thru CLI 0.3.2+54058649, alphanet, 2026-08-01.
