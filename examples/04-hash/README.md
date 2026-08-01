# 04-hash — the model meets a case it wasn't fit to

SHA-256 over the transaction's instruction data, two arms of the same program:

- **Arm A** — portable C, no intrinsics. Binary: **3,496 B**.
  Program: `ta-rWexuBmL558uxLZXqOb23DM0HeThZGSxG2mOm3-6oxv` (seed `example_04_hash_a`)
- **Arm B** — identical code except the four sigma functions use the RISC-V
  Zknh instructions (`sha256sig0/sig1/sum0/sum1`, via GCC
  `__builtin_riscv_sha256*`, which compiled first try). Binary: **2,744 B**.
  Program: `taUgLhBWu3NCyYud3ioz-8XS-K8ly2BxzHk3-HRaQ0MMcb` (seed `example_04_hash_b`)

The SDK **does** enable Zknh: `config/machine/thruvm.mk:13` passes
`-march=rv64imc_zba_zbb_zbc_zbs_zknh`. Note `zbb` in there — its
single-instruction rotates matter below. Both arms' digests were verified
against `sha256sum` via the emitted events.

Predictions were written and committed (`4df3756`) before any transaction ran —
see [PREDICTIONS.md](PREDICTIONS.md).

## Measurements

Alphanet, thru CLI 0.3.2+54058649, 2026-08-01, `--fee 0`, three runs per cell,
all 30 runs deterministic (slots 539701–539856). Input = N zero bytes as
instruction data; digest emitted as a 32-byte event.

| Arm | Input | Blocks | Predicted CU | Measured CU | Error | Pages (pred → meas) |
|---|---|---|---|---|---|---|
| A | 0 | 1 | 15,400 | 18,959 | −18.8% | 1 → **2** |
| A | 32 | 1 | 15,500 | 19,097 | −18.8% | 1 → **2** |
| A | 256 | 5 | 55,700 | 70,359 | −20.8% | 1 → **2** |
| A | 1024 | 17 | 176,400 | 224,511 | −21.4% | 1 → **2** |
| A | 4096 | 65 | 659,500 | 841,119 | −21.6% | 1 → **2** |
| B | 0 | 1 | 11,800 | 15,997 | −26.2% | 1 → **2** |
| B | 32 | 1 | 11,900 | 16,135 | −26.2% | 1 → **2** |
| B | 256 | 5 | 37,800 | 55,549 | −32.0% | 1 → **2** |
| B | 1024 | 17 | 115,500 | 174,157 | −33.7% | 1 → **2** |
| B | 4096 | 65 | 426,600 | 648,589 | −34.2% | 1 → **2** |

SU = 0 and Events = 1 (32 B) everywhere.

## What the measurements say

**Linearity is exact.** Per-block slope from every pair drawn from
{256, 1024, 4096}: arm A **12,846.0 CU/block**, arm B **9,884.0 CU/block** —
no variance. Intercepts implied by each of those points: **6,129, exactly,
in both arms** — identical, not merely close. The 0- and 32-byte points
deviate from that line (−16 and +122 CU respectively; they share a block
count yet differ by exactly +138 CU in *both* arms — a per-input-byte
tail-copy effect, independent of the sigma implementation).
*Correction (2026-08-02):* this section originally reported slope B = 9,885
and intercepts 6,113/6,112, derived from pairs anchored at the 0-byte point,
which carries the small-input deviation. The figures above are re-derived
from the clean points only. Practical consequence: **two measurements of a
linear program predict every other size to ~0.01%** (from A's 256-byte point,
the 4,096-byte prediction is 841,134 vs 841,119 measured — 15 CU off; the
re-run of arm B at 256 B hit its predicted 55,549 exactly, three times).

**Slopes (CU per byte hashed):** arm A **200.7**, arm B **154.4**
(at 4,096 bytes, including overhead: 205.4 and 158.3).

**Zknh speedup: 1.30× (B/A slope ratio 0.77).** Predicted 0.55–0.70 — missed.
The acceleration is real but modest, and the reason is visible in the march
string: `zbb` gives portable C single-instruction rotates (`rori`), so the
portable sigma is 5 instructions, not 8–10. Zknh collapses it to 1, saving
2,962 CU/block — worth having, but nothing like the "hardware SHA" framing.
The loudest performance claim survives contact with reality at 23% per block.

**Pages Used stayed constant as input grew** — 2 pages at 0 bytes and at
4,096 bytes. The model needs **no term for the instruction-data segment**;
reading input is charged per byte, not per page (read-only pages are free,
consistent with example 03).

**But Pages Used = 2, not the predicted 1.** This program writes no account
data, yet uses one more page than 01-noop. *Resolved by
[05-events](../05-events/README.md):* the extra page is the event buffer
page, which is **counted in `Pages Used` but never charged** — event emission
costs 570 + 1 CU/byte with no page charge at all. That is also why the 6,129
intercept sits below the 4,096×2 + 512×2 floor a charged event page would
require: only one page (the entry stub's stack page) is charged, `tsys_exit`
is free, and the intercept decomposes as 512 (entry segment-map syscall)
+ 4,096 (stack page) + 602 (emit of 32 B: 570 + 32) + 919 (instructions).

## Where the predictions failed, exactly

1. **Page count** (−4,096 CU on every point): predicted 1 page, measured 2.
   The model's *form* (4,096 × pages) is untouched — the input to it was
   wrong. With pages corrected, arm A's floor prediction lands within 3%.
2. **Instruction-byte slope** (the remaining −20% / −34%): estimated
   10,000 (A) and 6,450 (B) CU/block; measured 12,846 and 9,885. Implied
   instruction count was near the top of the estimate range *and* the hot
   loop compiles almost entirely to full-width 4-CU encodings (assumed
   avg 3.5). Arm B's miss is larger because the estimate credited Zknh
   with too much.

## Verdict

The model survives structurally and fails numerically — in one correctable
way and one honest way. Everything it claims about *structure* held on a case
it wasn't fit to: cost is exactly linear in blocks, intercepts are shared
across arms, pages don't scale with read-only input, determinism is perfect.
Its two numeric inputs are the weak points: page count must include a page
for event emission (newly learned, now documented), and instruction bytes
cannot be estimated from first principles to better than ~20–35% — measure
one size and calibrate the slope instead.

Corrected model (superseded again by 05 — see the root README for the final
form):

```
CU ≈ 4,096 × charged pages + 512 × charged syscalls + measured slope × blocks
```

## Reproduce

```bash
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
# input = N zero bytes: hex=$(head -c N /dev/zero | xxd -p | tr -d '\n')
thru $U txn execute --fee 0 ta-rWexuBmL558uxLZXqOb23DM0HeThZGSxG2mOm3-6oxv "$hex"   # arm A
thru $U txn execute --fee 0 taUgLhBWu3NCyYud3ioz-8XS-K8ly2BxzHk3-HRaQ0MMcb "$hex"   # arm B
```

Or `./run.sh` in this directory.

## Gotchas hit

- The first 8 bytes of a `tsys_emit_event` payload are consumed as the event's
  `event_type` tag (little-endian u64); `--json` shows only the remaining
  bytes as `data`. The digest checks out once you reassemble
  `event_type || data`. Plan payload layouts around this.
- `Events Size: 32` counts the full payload including those 8 type bytes.
