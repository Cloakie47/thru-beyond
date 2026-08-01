# 02-counter — what the account lifecycle actually costs

The most-wanted numbers in the repo: create, write, resize, delete. One
binary (952 B), five instructions, program
`taog1g-QXdnJHjWg3o_wrzHmKEMm01_mAWU142rFNeic4s` (seed `example_02_counter`).
Alphanet, thru CLI 0.3.2+54058649, 2026-08-02, `--fee 0`. Three identical
runs per figure (create: three separate accounts; delete: three separate
accounts).

## Headline figures

| Operation | CU | SU | Pages | Notes |
|---|---|---|---|---|
| `create` (proof 200 B) | 7,791 | 1 | 2 | accounts a, b — identical |
| `create` (proof 168 B) | 7,759 | 1 | 2 | account c |
| `increment` (no event) | 5,523 | 0 | 2 | |
| `increment_e` (emit 8 B) | 6,065 | 0 | 3 | event page counted, not charged |
| `resize_to` 8→4096 | 10,055 | 1 | 2 | |
| `resize_to` 8→4104 | 10,063 | 1 | 3 | |
| `resize_to` 8→8192 | 14,151 | 2 | 3 | |
| `resize_to` 8→16384 | 22,343 | 4 | 5 | |
| `resize_to` 8→65536 | 71,495 | 16 | 17 | |
| shrink to 8 (any source size) | 5,967 | 0 | 1–2 | constant CU |
| `delete` (data size 0) | 5,401 | 0 | 1 | **no refund: SU = 0, not negative** |

**Create cost is not constant across accounts**: the state proof is
instruction data, charged per byte. Accounts a/b needed a 200-byte proof
(7,791 CU); account c a 168-byte proof (7,759 CU). ΔCU = Δproof = 32 —
**exactly 1 CU per proof byte**, measured.

**Delete has an undocumented precondition**: `tsys_account_delete` fails
(errors, program reverts) while the account has data. The header comment
requires only balance = 0 and nonce = 0; observed behavior also requires
**data size = 0**. Resize to 0 first, then delete succeeds. Delete refunds
nothing — State Units consumed is 0, never negative.

## Resize: the third charging regime — and it unifies everything

Growth cost above the constant 5,967 shrink/no-op baseline:

| Transition | Bytes grown | CU above baseline |
|---|---|---|
| 8→4096 | 4,088 | 4,088 |
| 8→4104 | 4,096 | 4,096 |
| 8→8192 | 8,184 | 8,184 |
| 8→16384 | 16,376 | 16,376 |
| 8→65536 | 65,528 | 65,528 |

**Exactly 1 CU per byte grown, at all five sizes.** Account growth is
charged per byte at resize (zero-fill), not per page and not at first
write. Shrinking is free (constant 5,967 = entry 4,608 + writable 512 +
resize 512 + ~335 instructions, regardless of how much is released).
State Units step with pages grown: SU ≈ (new − old)/4096 rounded
(4096→1, 4104→1, 8192→2, 16384→4, 65536→16). Marginal cost of one extra
byte of account data: **1 CU at resize** (plus 1 CU per byte of the
containing page region copied on each later CoW write — see below).

So "does resize charge at allocation the way the stack did in example 05?"
— yes, and the two are the same rule: the stack grow's "4,096 per page" is
just 1 CU per zero-filled byte in units of whole pages.

## Increment: the 4,096 "page charge" was never a page charge

`increment` on this 8-byte account costs **5,523 CU** = 4,608 (entry floor)
+ 512 (`set_account_data_writable`) + 403 (instructions and 8-byte
copy/read/write). There is **no 4,096 CoW term** — yet example 03 measured
+4,096 per written page on an 8,192-byte account. Both are explained by one
rule: **first write to an account page costs 1 CU per byte the CoW copy
actually copies** — 4,096 for a full page, ~8 for this 8-byte account.
The "page fault = 4,096 CU" of the docs is the full-page special case.

`increment_e` = 5,523 + 542 (emit syscall 512 + 8 event bytes + ~22
instructions), and its event page appears in `Pages Used` (3) without a CU
charge, consistent with example 05.

## Does the published model predict these?

| Op | Model prediction | Measured | Verdict |
|---|---|---|---|
| increment | ~9,600 (with flat 4,096 CoW) | 5,523 | **miss by ~4,096** — CoW is per-byte-copied, not flat; model corrected |
| increment (corrected: 4,608 + 512 + instr + 8) | ~5,500 | 5,523 | hit |
| increment_e | increment + 570 + 8 | 6,101 | 6,065 — 36 CU of codegen noise |
| resize growth | (not covered by model) | 1 CU/byte | new term added |
| create | 4,608 + 3×512 + proof + ~16 data | 6,360 + proof | 7,791 ⇒ **residual ~1,430** in the create syscall (seed/PDA hashing, proof verification compute) — syscalls with payloads exceed the 512 base, as the model's limits already warned |
| delete | 4,608 + 512 + instr | ~5,400 | 5,401 — hit |

## The docs' quickstart figures (7,524 create / 5,980 increment)

Reconciled, not contradicted:

- The quickstart's increment **emits an event**; comparing like with like,
  our `increment_e` = 6,065 vs docs 5,980 — Δ85 (~1.4%), the size of a
  codegen/program-shape difference between SDK versions.
- Create at 7,524 vs our 7,759–7,791: proof size alone moves this number
  1 CU/byte (our two proofs differed by 32 bytes), and the quickstart's
  proof size is unstated. Δ235–267 ≈ a modestly smaller proof plus shape
  differences. **No evidence of a real cost-model change.**

## Addendum (2026-08-02 audit): predictions that held

- **CoW discriminator**: increment on a 100-byte account measured **5,615**
  (predicted 5,615 = 5,523 + 92 more bytes copied) and on a 5,000-byte
  account **9,611** (predicted 9,611 = first page only, 4,096-byte copy; a
  whole-account copy would have been 10,515). Per-byte, page-granular CoW —
  confirmed at byte granularity.
- **Third proof size**: create with a 264-byte proof = **7,855** = 7,791 + 64.
  1 CU per proof byte at three distinct sizes (168/200/264).
- **SU formula discriminated**: resize 4096→8192 consumes **SU = 1** — so
  SU = ceil(bytes grown / 4096), not floor(target/4096). Non-page-multiple
  growth also exact in CU: →100 = +92, →5000 = +4,992.

## Trailing-page CoW experiment — PREDICTIONS, committed before measurement

The 5,000-byte account (`taRWnS6k1yAYX9XN9efjjwFNCvY2iFMTLSeeHuR2XSM97a`) has a
full page 0 (4,096 bytes) and a trailing page 1 holding only 904 bytes. Two
new intra-binary instructions: `write_at` (one byte at a given offset) and
`write_two` (one byte at offset 0 AND one at the given offset). Let W0 =
measured `write_at(0)` (copies full page 0 = 4,096).

**Outcome A — copy charges bytes *present* in the page
(min(page size, bytes present)):**
`write_at(4096)` = W0 − 4,096 + 904 = **W0 − 3,192**, and `write_at(4999)`
identical. `write_two(4096)` ≈ W0 + 904 + ε (both pages copied = the whole
5,000-byte account; ε = a few CU for the extra store's instruction and data
bytes).

**Outcome B — copy charges the full mapped page regardless of data present:**
`write_at(4096)` = **W0 exactly** (same 4,096 charge), `write_at(4999)` the
same, `write_two(4096)` ≈ W0 + 4,096 + ε — and then the 8-byte (+8) and
100-byte (+92) increment results need a different explanation than
bytes-copied.

Additivity check either way: `write_two(4096)` − `write_at(4096)` should
equal `write_at(0)` − (fixed path) + ε, i.e. the two pages' charges add.
The offset is read from instruction data, so the executed code path is
byte-identical across offsets.

## Reproduce

```bash
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
P="taog1g-QXdnJHjWg3o_wrzHmKEMm01_mAWU142rFNeic4s"
# create: derive PDA, make-state-proof creating, then
#   instr = 00000000 + 0200 + <seed 32B hex> + <proof size LE u32> + <proof hex>
# increment:   01000000 0200        increment_e: 02000000 0200
# resize_to S: 03000000 0200 <S LE u32>          delete: 04000000 0200
thru $U txn execute --fee 0 --readwrite-accounts <PDA> "$P" <instr-hex>
```

Accounts used (now deleted): `taAO2n7Xp7Y9GOqYSswrmo8RqNbk1M8TEIFJ1uUUeufWFA`,
`taYZ_VtsFzo10iPq7WZoBfULbs58wxzQUznrsK5xzxu5Zr`,
`ta1y3q_1C7JZLpNScOOOS-XSaPPWYGrIEyuZ52LxuVdAAW`.
