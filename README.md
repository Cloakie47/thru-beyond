# Beyond — Thru and Beyond

Working, deployed, cost-measured example programs for the Thru blockchain.
See `CLAUDE.md` for the rules and workflow.

## The cost model

Examples [01](examples/01-noop/README.md) and [03](examples/03-storage/README.md)
produced a predictive model for Thru execution cost:

```
CU ≈ 4,096 × (pages written) + 512 × (syscalls) + instruction bytes
```

Validation against every measurement taken so far (alphanet, thru CLI
0.3.2+54058649, 2026-08-01):

| Instruction | Pages | Syscalls | Predicted floor | Measured | Residual |
|---|---|---|---|---|---|
| 01-noop | 1 | 1 | 4,608 | 4,763 | 155 |
| `read_p1` | 1 | 1 | 4,608 | 4,892 | 284 |
| `write_p0` | 2 | 2 | 9,216 | 9,582 | 366 |
| `write_p0_x2` | 2 | 2 | 9,216 | 9,555 | 339 |
| `write_p0_p1` | 3 | 2 | 13,312 | 13,659 | 347 |

Every residual falls in the instruction band (155–366 CU — dispatch,
validation, the actual loads and stores).

**Limits of the model, stated plainly:**

- It is derived from five data points.
- Syscalls with large data payloads carry per-byte cost beyond the 512 base —
  example 03's `init` (account create with a 232-byte state proof, plus
  resize) measured 15,913 CU, far above its floor.
- Residuals vary by ±30 CU from codegen alone: a handler doing strictly
  *more* work measured 27 CU *less* (03's `write_p0_x2` vs `write_p0`),
  because CU bills per instruction byte and compiler layout dominates at
  that scale.
- The page term is **copy-on-write**: written pages only. Reads of untouched
  pages cost no page charge and don't increment `Pages Used`.

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

Deploying an 838 B program cost **320,292 CU** across five transactions
(measured breakdown in the 03 README).

Example 04 tested the model against predictions committed before measurement:
structure survived (exact linearity — 12,846 CU per 64-byte block portable,
9,885 with Zknh, common intercept to 1 CU), point predictions missed by
19–34% (full reckoning in the 04 README). **Zknh SHA-256 speedup: 1.30×.**
Event-emitting programs use one more written page than noop — budget for it.

All figures: thru CLI 0.3.2+54058649, alphanet, 2026-08-01.
