# 04-hash — predictions, written and committed BEFORE measurement

Date: 2026-08-01. Model under test (from examples 01 + 03):

```
CU ≈ 4,096 × (pages written) + 512 × (syscalls) + instruction bytes
```

with 1 CU per byte of data processed. Binaries already built (arm A portable:
3,496 B; arm B Zknh builtins: 2,744 B) but **nothing has been executed yet**.

## Reasoning

**Pages.** Nothing is written to any account. Instruction data is read-only,
and example 03 showed reads of untouched pages incur no page charge and no
`Pages Used` increment. Baseline (01-noop) shows 1 page even with zero account
activity — presumably the stack. SHA-256 state, `w[64]` schedule (256 B), and
the 128 B tail buffer all live in that same stack page.
**Prediction: Pages Used = 1 for every measurement, at every input size.**
This is the model's stress point: if the runtime charges the instruction-data
segment as written pages, the 4,096-byte input should show +1–2 pages
(+4,096–8,192 CU) and the model needs a new term.

**Syscalls.** Constant 2 per transaction (`tsys_emit_event`, `tsdk_return`)
= 1,024 CU, plus 32 event bytes ≈ 1,056. Identical across arms and sizes —
cancels in every delta.

**Fixed floor.** 4,096 (page) + 1,024 (syscalls) + ~230 (entry, dispatch,
event bytes, digest serialization) ≈ **5,350 CU** shared by both arms.

**Blocks.** SHA-256 processes N = ceil((L+9)/64) 64-byte blocks:
L=0→1, 32→1, 256→5, 1024→17, 4096→65. CU should be **linear in N**, not in L
directly (0 and 32 bytes should cost the same to within the tail-copy noise).

**Per-block instruction estimate, arm A.** Message load 16 words byte-wise
≈ 160 instr + 64 CU data; schedule 48 iters × ~19 instr ≈ 912 (the SDK enables
zbb, so each rotate in the sigma idiom is a single `rori` — 5 instr per sigma,
not 8); rounds 64 × ~25–33 instr ≈ 1,600–2,100. Total ≈ 2,700–3,200 instr,
mostly 32-bit encodings ≈ 3.5 CU avg → **≈ 10,000 CU/block** (±25%),
data included → slope ≈ **157 CU/byte**.

**Per-block, arm B.** Each of the 4 sigma functions collapses from 5
instructions to 1: saves 4 instr × (2×48 schedule + 2×64 rounds) = 896 instr
× 4 CU ≈ 3,580 CU → **≈ 6,450 CU/block** (±25%), slope ≈ **101 CU/byte**.
**Predicted speedup ratio B/A ≈ 0.64** (bounds 0.55–0.70).

## Point predictions (CU = 5,350 + per-block × N)

| Arm | Input bytes | Blocks | Predicted CU | Predicted Pages |
|---|---|---|---|---|
| A | 0 | 1 | 15,400 | 1 |
| A | 32 | 1 | 15,500 | 1 |
| A | 256 | 5 | 55,700 | 1 |
| A | 1024 | 17 | 176,400 | 1 |
| A | 4096 | 65 | 659,500 | 1 |
| B | 0 | 1 | 11,800 | 1 |
| B | 32 | 1 | 11,900 | 1 |
| B | 256 | 5 | 37,800 | 1 |
| B | 1024 | 17 | 115,500 | 1 |
| B | 4096 | 65 | 426,600 | 1 |

Confidence: floor ±200 CU; block term ±25% (it is an instruction-count
estimate, the least constrained part of the model). The sharp, falsifiable
claims are: Pages Used = 1 everywhere; all runs deterministic; CU linear in
block count with a common intercept across arms; B/A slope ratio in 0.55–0.70.
