# 07-cpi — what a cross-program invocation costs

Two programs: caller (848 B, `ta9TmfhHffn5hJ3P83hC8NtwERjworfg7pSGxU_GrEPEmy`,
seed `example_07_caller`) and a deliberately tiny callee (734 B,
`taAf417KM3aXeDTILaGkk2kUMpJbDadZSGnm-1uSAJIRCu`, seed `example_07_callee`).
Alphanet, thru CLI 0.3.2+54058649, 2026-08-02, `--fee 0`, three identical
runs per successful cell. Predictions committed at `81b3c04` before
measurement ([PREDICTIONS.md](PREDICTIONS.md)).

## Verdict on the central question: H-C — per *depth level*, not per hop

The stack segment is shared (spec: one stack segment per VM), but the first
visit to each call depth maps one fresh 4,096-CU page via the callee's
entry stub — and that page is **reused** by sequential invokes at the same
depth.

| Measurement | CU | Pages |
|---|---|---|
| `no_cpi` (baseline) | 4,925 | 1 |
| `cpi_1` | 10,492 | 2 |
| `cpi_n` N=1 | 10,526 | 2 |
| `cpi_n` N=2 | 12,037 | 2 |
| `cpi_n` N=4 | 15,059 | 2 |
| `cpi_n` N=8 | 21,103 | 2 |
| `cpi_deep` N=1 | 16,147 | 3 |
| `cpi_deep` N=2 | 21,754 | 4 |
| `cpi_deep` N=4 | 32,968 | 6 |
| `cpi_deep` N=8 | 55,396 | 10 |
| `cpi_deep` N=13 | 83,431 | 15 |
| `cpi_deep` N=14 | 89,038 | 16 |
| `cpi_deep` N≥15 | transaction fails | — |

**Frame-counting convention (corrected 2026-08-02):** the top-level program
runs at call depth 1. `cpi_deep_n(N)` hands the callee a countdown of N, and
the `depth = 0` invocation still executes as a frame — so it spawns **N + 1
callee frames**, reaching call depth **N + 2**. (An earlier revision of this
README counted N frames; the CU figures were unaffected but the depth-limit
conclusion was wrong — see below.)

**One table, one convention — `Pages Used` = deepest call depth reached:**

| Measurement | Deepest depth | Pages Used |
|---|---|---|
| `no_cpi` | 1 | 1 |
| `cpi_1` | 2 | 2 |
| `cpi_n` N=1 / 2 / 4 / 8 | 2 (depth-2 frame reused) | 2 / 2 / 2 / 2 |
| `cpi_deep` N=1 | 3 | 3 |
| `cpi_deep` N=2 | 4 | 4 |
| `cpi_deep` N=4 | 6 | 6 |
| `cpi_deep` N=8 | 10 | 10 |
| `cpi_deep` N=13 | 15 | 15 |
| `cpi_deep` N=14 | 16 | 16 |

Both series agree exactly under this convention: one stack page per call
depth, mapped at the first visit to that depth, reused by every later frame
at the same depth (in this no-account, no-event program there are no other
page sources). There is no disagreement between `cpi_1` (Pages 2) and
`cpi_deep_1` (Pages 3): deep N=1 is caller + **two** callee frames, not one.

- **First hop: 5,567 CU** (10,492 − 4,925) — the depth-2 frame page is
  mapped on first entry.
- **Each additional same-depth hop: exactly 1,511 CU** — the `cpi_n` slope
  is 1,511.00 at every pair, Pages flat: the frame page is not freed on
  return and costs nothing to reuse.
- **Each additional depth level: exactly 5,607 CU = 1,511 + 4,096**, Pages
  +1 per level.
- **The 40 CU difference between 5,567 and 5,607** (both "a fresh page plus
  a hop"): the two numbers ride different code paths. Disassembly of both
  binaries shows the deep marginal frame executes the callee's recursion
  path — parsing a second u32 field (4 × `lbu` + shifts/ors, ~28 CU by
  itself), building the next 10-byte args struct with stores, and checking
  two return codes — while the `cpi_1` path is the caller's invoke helper
  plus the callee's shorter op-0 terminal. Mechanism confirmed by
  disassembly; the exact 40 was not closed by instruction-count summation
  (the repo's hand-counts carry ±4-CU residuals at best).
- Marginal-hop decomposition: 1,511 = 512 (invoke base) + 512 (callee entry
  stub's segment syscall, a no-op re-set once the page exists) + ~487
  (callee path + call-site instructions + data). See the open question
  below before reading anything more into the ~487.

## Resolved (2026-08-02): no register-save surcharge — invoke costs exactly
## its 512 base

The open question — does invoke charge ~256 CU for its register save on top
of the 512 base the spec derives *from* that save (32 regs × 8 B × 2)? —
was closed by the exact-count experiment this section used to describe.
Counting every executed instruction and load/store byte on the marginal
`cpi_n` path from the disassemblies:

| Component | Instruction CU | Load/store CU | Total |
|---|---|---|---|
| Caller loop body (spill/reload, jal) | 20 | 16 | 36 |
| `ex07_invoke_callee` helper | 38 | 42 | 80 |
| `tsys_invoke` wrapper (auth=NULL path) | 64 | 104 | 168 |
| Callee: entry stub | 88 | 21 | 109 |
| Callee: `start` op-0 + return + exit | 66 | 20 | 86 |
| Syscall bases: invoke 512 + callee segment-set 512 (exit = 0) | | | 1,024 |
| **Counted** | | | **1,503** |
| **Measured marginal hop** | | | **1,511** |

Residual: **8 CU** — hand-count noise (the repo's counts carry ±4–8
everywhere), nowhere near the ~256 a surcharge would leave. **The surcharge
reading is dead; `tsys_invoke` costs its plain 512 base plus visible
instruction and data bytes, and the spec's derivation of the base stands
(CONFIRMS).** The earlier "callee ~230 + save ~256" split was an artifact
of estimating only the callee path and ignoring that the caller-side helper
and syscall wrapper themselves cost 284 CU per hop.

## Depth limit — CONFIRMS the SDK header (correcting this README's own
earlier claim)

The header constant, quoted exactly
(`include/thru-sdk/c/tn_sdk_txn.h:66`):

```c
#define TSDK_SHADOW_STACK_FRAME_MAX (17U) /* 16 call depths (1..16) + 1 for frame -1 */
```

It counts *call depths*, root = depth 1. Measured: `cpi_deep` N=14 spawns
15 callee frames on top of the root — **deepest call depth 16 — and
executes.** N=15 (which would require depth 17) fails with the deepest
callee's `tsys_invoke` returning **−24
(TN_VM_ERR_SYSCALL_CALL_DEPTH_TOO_DEEP)**, observed as our wrapper code
`0x7BE8` = 0x7C00 + (ulong)(−24). The header counts exactly what was
measured and matches: **CONFIRMS**, 16 call depths (1..16). An earlier
revision of this README claimed "maximum depth 15, corrects the header" —
that came from miscounting the recursion (forgetting the depth-0 frame),
not from the chain. (CU at the failing boundary is unobservable: failed
transactions report no CU — the known CLI gap.)

## Revert propagation — a caller CANNOT catch a callee's revert

`cpi_revert` was designed to absorb the callee's failure: check both
`invoke_result` and `invoke_err`, emit them, return success. It never got
the chance. A callee that calls `tsdk_revert` (i.e. `tsys_exit` with
revert=1) **aborts the entire transaction immediately** with the *callee's*
error code (`user_error=0x7BAD`, the callee's constant); no event was
emitted, so the caller's post-invoke code did not run. Contrast: *syscall-
level* invoke failures (like depth −24 above) DO return in `invoke_result`
and are catchable. The C reference's pattern of branching on `invoke_err`
only applies to that second class. CU consumed by the aborted attempt is
unreported.

## Account indices are transaction-global — confirmed empirically

The callee, invoked via CPI, read the u64 at offset 0 of the account at
transaction index 2 and emitted it: **3** — exactly the counter value of
ex02's account d, which the transaction placed at index 2. The callee sees
the same indices as the top-level transaction (spec's "same transaction
context", confirmed). Event metadata also attributes the event to the
callee (`program_idx: 3`, `call_idx: 1`).

## The finding that reshaped the experiment

**CPI instruction data arrives in registers (a0 = pointer, a1 = length —
documented in the invoke spec), and the SDK's `tsdk_txn_get_instr_data()`
always reads the TOP-LEVEL transaction.** A first draft of both programs
used the quickstart's txn-accessor pattern and every CPI failed on the
callee's own size check — the callee was parsing the *caller's* instruction
data. Programs written the quickstart way (ex02 included) cannot be CPI
callees; the entrypoint must be declared
`void start(uchar const *instr_data, ulong instr_data_sz)`. This works for
top-level execution too. Nothing in the C guide mentions it.

## Does the published model predict this?

Structurally, yes, with nothing new: the depth premium is the existing
4,096-per-anonymous-page term (the callee entry stub maps it); the hop cost
is two documented 512 syscall bases plus instruction/data bytes; linearity
is exact in both directions. What the model cannot do — as always — is
predict the ~487 instruction/data term from first principles; and the
probable +256 register-save charge inside it is new, unisolated, and
flagged UNVERIFIED.

## Reproduce

```bash
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
CALLER="ta9TmfhHffn5hJ3P83hC8NtwERjworfg7pSGxU_GrEPEmy"
CALLEE="taAf417KM3aXeDTILaGkk2kUMpJbDadZSGnm-1uSAJIRCu"
# args: <type:u32><n:u32><callee_idx:u16><aux_idx:u16>, all LE; callee at
# readonly index 2. Types: 0 no_cpi, 1 cpi_1, 2 cpi_n, 3 cpi_deep_n,
# 4 cpi_revert, 5 read-account probe
thru $U txn execute --fee 0 --readonly-accounts "$CALLEE" "$CALLER" 010000000000000002000000
```

Or `./run.sh` in this directory.
