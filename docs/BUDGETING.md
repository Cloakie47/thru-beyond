# Budgeting Thru transactions — worked examples

Every transaction declares `req_compute_units`, `req_state_units`,
`req_memory_units`. The CLI defaults request **300,000,000 / 10,000 /
10,000** — against real usage typically in the thousands, tens, and single
digits. These examples use `bench/estimate.py` (validated: 89/89 recorded
transactions predicted exactly once the instruction term is calibrated).
All figures alphanet, CLI 0.3.2, 2026-08.

Terms cheat-sheet: floor 4,608 (every transaction) + 512/syscall (exit
free) + 4,096/anonymous page + 1 CU per byte (CoW copied, resize grown,
payload, event data, instructions). MU = pages written or allocated
(+1 stack, +event pages). SU = ceil(bytes grown/4096) + creates; nothing
refunds.

## 1. Simple account update (increment an 8-byte counter)

| Term | CU |
|---|---|
| floor | 4,608 |
| set_account_data_writable | 512 |
| CoW (8 bytes present) | 8 |
| instructions (calibrated) | ~395 |
| **total (measured: 5,523)** | **~5,523** |

Recommend: **CU 7,500 · SU 2 · MU 4** (measured MU 2).
CLI default requests 54,000× the CU actually needed.

## 2. Event-emitting program (update + 8-byte event)

Add one emit: 512 base + 8 bytes + ~22 setup ≈ +542 (measured total
6,065; MU 3 — the event page is CU-free but MU-charged).
Recommend: **CU 8,500 · SU 2 · MU 5**.

## 3. CPI composition, three deep

Caller + callee + callee-of-callee (deepest depth 3): floor + 2 hops ×
1,024 syscall bases + 2 fresh depth pages × 4,096 + callee paths
(~490/hop calibrated) ≈ 4,608 + 2,048 + 8,192 + ~980 ≈ **~15,800 CU**
(measured `cpi_deep` N=1, same shape: 16,147). MU = 3 (one stack page per
depth). Recommend: **CU 21,000 · SU 2 · MU 5**.
Sequential same-depth calls are far cheaper (1,511/hop, MU flat) —
compose wide, not deep.

## 4. 500-account batch write (1 byte each)

Floor + 500 × 590 (writable syscall + 8-byte CoW + loop) ≈ **~300,000 CU**
(measured write_all N=512: 307,033). MU = 501 — **the tightest budget
relative to its default** (5% of the CLI's 10,000, where CU sits at 0.1%
of its default) — but note it still does not bind: even the heaviest
measured workload reached 1,001 MU against a 65,535 ceiling, and the
32 KiB transaction size runs out before any declared budget does (batch
writes cap near ~1,015 accounts for this reason, not because of MU).
SU 0. Recommend: **CU 400,000 · SU 2 · MU 510**.

## Does over-requesting cost anything today?

**No observed cost.** Transactions requesting 300M CU against 5k consumed
paid the same fees as tight requests in every measurement here (all at
`--fee 0`; the deploy pipeline's own transactions request up to 500M).
Budgets act only as ceilings: exceed one and the transaction fails.
**Open question, flagged:** if a fee market ever prices *requested* rather
than *consumed* units — as congestion pricing on other chains does —
over-requesting would become expensive and tight budgets would matter
economically. Nothing measured here settles what Thru will do; size
tightly anyway, because it is free to do so and the failure mode of a too-
tight budget (one clean error) is easier to debug than most.
