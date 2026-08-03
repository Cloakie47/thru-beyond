# 09-limits — what 1,024 accounts and 32 KiB actually buy you

Alphanet, thru CLI 0.3.2+54058649, 2026-08-02. Predictions committed at
`9ab3321` and `937bd35` before measurement. **Program identity note:** the
09 binary runs on example 06's borrowed slot
(`ta10jkQhjY5E8XIahXpzTlhDYhv8bLkbtYHWuZAlJC1LVu`) via the upgrade path —
permanent program creation was down (proof desync) throughout. 06 is fully
measured and will be restored by re-upgrade. The account fleet is **1,022
EPHEMERAL accounts** (no proofs needed — the only creation path that
worked), created at 8 bytes each.

## The fleet itself was a finding

`fleet_create_n` created **950 ephemeral accounts in ONE transaction**:
2,480,667 CU (≈2,611 per account: create 512 + writable 512 + resize 512+8
+ ~1,070 internal), Pages/MU 951 — and **SU 2** (not 950): ephemeral
resizes are essentially state-unit-free, unlike permanent accounts'
1-SU-per-page growth. Second batch: 72 accounts, 192,599 CU, SU 1.

**≈2,611 is not the same quantity as example 12's 6,303** — see the
reconciliation in [12-ephemeral](../12-ephemeral/README.md): 6,303 is a
whole one-create transaction (4,608 floor included, no resize), 2,611 is
the marginal per-account cost inside one transaction and includes a
writable + resize (+1,024 CU) the single create doesn't do. Quote 6,303
for "one ephemeral account, one transaction" and ~2,611 for "each
additional account in a batch".

## Part A — the per-account ladders (each cell ≥3 identical runs on key points)

| N | touch_none CU | read_all CU | write_all CU | write Pages/MU |
|---|---|---|---|---|
| 1 | 4,947 | 4,999 | 5,543 | 2 |
| 2 | 4,947 | 5,043 | 6,133 | 3 |
| 4 | 4,947 | 5,131 | 7,313 | 5 |
| 8 | 4,947 | 5,307 | 9,673 | 9 |
| 16 | 4,947 | 5,659 | 14,393 | 17 |
| 64 | 4,947 | 7,771 | 42,713 | 65 |
| 128 | 4,947 | 10,587 | 80,473 | 129 |
| 256 | 4,947 | 16,219 | 155,993 | 257 |
| 512 | 4,947 | 27,483 | 307,033 | 513 |
| 1,000 | 4,947 | 48,955 | 594,953 | 1,001 |

- **A declared-but-untouched account costs exactly 0 CU.** touch_none is
  4,947 (the program's own floor) from N=0 to N=1,018 — account-list
  validation and sorting are entirely unmetered. The predicted-possible
  ~32 CU/account never appeared. (Bonus: declared accounts need not even
  exist — nonexistent derived addresses are accepted read-only.)
- **Read: exactly 44 CU per account** (get-pointer + 8-byte load + loop),
  linear at every pair. Pages/MU flat at 1 through 1,000 accounts — reads
  never count, at fleet scale.
- **Write: exactly 590 CU per account** = 512 (per-account
  `set_account_data_writable`) + 8 (CoW of the 8-byte account) + 70
  instructions. Pages/MU = N+1 exactly. SU = 0 (ephemeral).
- **Linearity is perfect. No O(n log n) appears anywhere** — the sort is
  validated, not performed, on-chain (the submitter must pre-sort). The
  practical ceiling is not computational.

## The boundary — three failure modes, exactly mapped

| N declared | Outcome |
|---|---|
| 1,018 | **works** (4,947 CU) |
| 1,019–1,021 | RPC rejects: transaction size exceeds 32 KiB |
| 1,022 | RPC: "transaction size 32888 exceeds maximum" (32,704 addresses + 184 envelope) |
| 1,023–1,024 | CLI signing refuses: "Too many accounts: N+2 exceeds maximum 1024" |
| 1,025 | CLI validation: "Too many accounts: 1025 (maximum 1024 allowed)" |

**The size limit binds before the account limit.** With a 184-byte
envelope (measured: 32,888 at 1,022 declared) the ceiling is
32N + 184 ≤ 32,768 → **N = 1,018 declared accounts (1,020 total)** —
the advertised 1,024 is unreachable by 6 for even the emptiest possible
transaction. 1,024 × 32 = 32,768 is exactly the whole size budget: the
two headline limits collide by construction.

## Part C — the 1,000-recipient payout: PROJECTION ONLY (blocked)

A real token payout needs recipients that can hold funds; ephemeral
accounts cannot, and permanent creation is down. **Projection from
measured figures, labeled as such:** floor 4,608 + 1,000 × (512 transfer
base + ~20 instructions) ≈ **~537,000 CU** — about **0.18% of the CLI's
default 300,000,000-CU request** (the basis of this percentage; against
the field's u32 maximum of 4,294,967,295 it would be 0.0125% — the two
denominators differ 14×, so always name the one you mean); CU is not the
constraint. The binding constraint is
transaction size: ~1,015 writable recipients + treasury fit. On these
numbers, Thru's account limit is a real capability — a 1,000-recipient
payout fits in one transaction with CU to spare — but the *measured*
version awaits working permanent creates.

## Reproduce

`results.json` carries every cell. Fleet: `fleet_create_n` (type 7) with
u16 seed-index-per-slot; ladders: types 0/1/2 with N in instruction data,
accounts passed as repeated `--readonly-accounts`/`--readwrite-accounts`
flags (comma lists are not accepted), sorted via `thru txn sort`.
