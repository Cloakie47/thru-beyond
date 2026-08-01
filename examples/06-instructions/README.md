# 06-instructions — the instruction term, measured directly

Until this example, instruction cost was always the *residual* after
subtracting pages, syscalls, and data — and the 4-CU-per-32-bit /
2-CU-per-compressed rate came from the docs, not from measurement. Here the
loop bodies are inline asm, so the executed encodings are exact ground truth,
and only the iteration count varies.

One binary (420 B), program `ta10jkQhjY5E8XIahXpzTlhDYhv8bLkbtYHWuZAlJC1LVu`
(seed `example_06_instructions`). Alphanet, thru CLI 0.3.2+54058649,
2026-08-02, `--fee 0`, three identical runs per cell (no exceptions).

## Ground truth from the disassembly

| Body | Encodings (objdump) | Instr | Bytes | Predicted CU/iter |
|---|---|---|---|---|
| `spin` | `c.addi` `c.add` `c.addi` `c.bnez` | 4 | 8 | 8 |
| `spin_wide` (`.option norvc`) | `addi` `add` `addi` `bnez` | 4 | 16 | 16 |
| `spin_load` | `lbu` `c.add` `c.addi` `c.bnez` | 4 | 10 | 10 + 1 load byte |
| `spin_store` | `sb` `c.add` `c.addi` `c.bnez` | 4 | 10 | 10 + 1 store byte |

## Measured (N = 0, 1, 10, 100, 1000, 10000)

| Variant | N=0 | N=10000 | Slope (CU/iter) | Predicted | Ratio |
|---|---|---|---|---|---|
| `spin` | 4,924 | 84,924 | **8.000** (exact at every pair) | 8 | **1.000** |
| `spin_wide` | 4,928 | 164,928 | **16.000** | 16 | **1.000** |
| `spin_load` | 4,926 | 114,926 | **11.000** | 11 | **1.000** |
| `spin_store` | 4,930 | 114,930 | **11.000** | 11 | **1.000** |

**Verdicts:**

- **The docs' rate is exactly right**: 4 CU per 32-bit encoding, 2 per
  compressed — measured ratio 1.000 at every size.
- **Cost tracks encoding BYTES, not instruction count**: `spin_wide` executes
  the *same four instructions* per iteration as `spin` and costs exactly
  double — 16 vs 8 — because its encodings are twice the bytes. The per-byte
  law extends to instructions: **1 CU per instruction byte.**
- **Loads cost 1 CU per byte accessed on top of the instruction** (`spin_load`
  = 10 instruction bytes + 1): the docs' claim, confirmed.
- **Stores are charged the same way** (`spin_store` = 10 + 1) — not previously
  documented anywhere. This is what closes most of 01-noop's residual gap:
  the entry/exit path executes four 8-byte `sd` stores = 32 CU that earlier
  accounting missed.

## Syscall probes

- `log0` (`tsys_log` with length 0 — a syscall that does nearly nothing):
  5,454 = its dispatch baseline + **530 ≈ 512 + 18**. The 512 base is charged
  even for a no-op syscall: it is a **per-call base**, not an average.
- `log8`: 5,462 = log0 + 8 — **1 CU per logged byte.**
- `grow` (`set_anonymous_segment_sz`): re-setting the same size costs
  540 = 512 + 28; growing by one page costs 4,636 = 512 + 28 + 4,096 (CU
  9,560 vs baseline 4,924, Pages 2). **Sizes that are not page multiples are
  rejected** (n = 8, 100, 2048 all fail with the syscall returning an error)
  — anonymous-segment charging is page-granular *by API*, so per-byte vs
  per-page charging for anonymous memory cannot be distinguished below page
  granularity.

## Reproduce

```bash
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
P="ta10jkQhjY5E8XIahXpzTlhDYhv8bLkbtYHWuZAlJC1LVu"
# instruction data = <type:u32 LE><n:u32 LE>; types: 0 spin, 1 spin_wide,
# 2 spin_load, 3 spin_store, 4 log0, 5 log8, 6 grow(n bytes past page 1)
thru $U txn execute --fee 0 "$P" 0000000010270000   # spin, N=10000
```
