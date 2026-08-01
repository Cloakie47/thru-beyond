# 07-cpi — predictions, committed BEFORE measurement

Date: 2026-08-02. Spec read first: `/spec/vm/syscalls/invoke.md` (base 512,
justified as 32 registers × 8 bytes × 2 saved/restored; target by
transaction account index; "same transaction context"; depth limit
`TN_VM_CALL_DEPTH_MAX`, error −24), the C CPI reference, and the
memory-layout/shadow-stack material ("Max Segments: 1 per VM instance" for
the stack; `TSDK_SHADOW_STACK_FRAME_MAX` = 17 = 16 call depths + frame −1).

One extra piece of evidence shapes these predictions: the SDK entry stub
(disassembled in example 05's session) computes the parent's stack bytes
from the shadow stack and calls `set_anonymous_segment_sz(parent + 4096)` —
so the callee's `_start` runs per invoke and requests one more page in the
*shared* stack segment.

Estimated fixed callee-side path (entry stub ~145 CU incl. loads/stores +
minimal dispatch ~40): **~185 CU**, to be replaced by a disassembly count.

## The central question and three hypotheses (per hop)

- **H-A — shared stack, no growth** (callee runs inside the existing
  mapping; its set-segment call is a no-op re-set): hop ≈ 512 (invoke)
  + 512 (callee entry syscall) + ~185 (callee path) ≈ **~1,210**, Pages flat.
- **H-B — fresh 4,096 per hop** (the entry stub's request maps a new page
  every time): hop ≈ 1,024 + 4,096 + ~185 ≈ **~5,300**, Pages +1 per hop.
- **H-C — mixed/other.** Most likely form, given the entry stub: **+4,096
  per DEPTH level but not per sequential hop** — the first invoke at a given
  depth maps the frame's page; whether it is freed on return decides
  `cpi_n`'s slope. If freed: `cpi_n` slope ≈ H-B's hop. If persistent:
  `cpi_n` slope ≈ H-A's hop while `cpi_1` pays H-B once. Another H-C
  signature: invoke costing **more than 512 base** — the documented 256-byte
  register save is a memory write, and stores are charged 1 CU/byte, so
  invoke may measure 512 + 256 (+ restore 256) above its call instructions.

## Quantitative predictions

- `no_cpi` baseline ≈ 4,900–5,000 (entry floor + dispatch; measured, not
  predicted, and all deltas are against it).
- `cpi_1` − `no_cpi`: H-A ~1,210 · H-B ~5,300 · H-C(invoke-save charged)
  adds +256 or +512 to either.
- `cpi_n` N = 1, 2, 4, 8: linear; slope = per-hop cost (± ~15 CU loop
  overhead); discriminates the freed-on-return question.
- `cpi_deep_n` at depth n: expect ≈ n × (hop + ~60 CU recursion instructions),
  with **+4,096 × n if frames map fresh pages** — and `Pages Used` = 1 + n
  in that case (the cleanest signal; if Pages stays 1, H-A).
- Depth limit: root = depth 1, so `cpi_deep_n` adds n frames → total depth
  1 + 1 + (n−1)... stated plainly: with 16 usable call depths documented,
  some n between 14 and 16 must fail with **−24
  (TN_VM_ERR_SYSCALL_CALL_DEPTH_TOO_DEEP)**; n = 17 certainly fails. The
  exact boundary is a measurement, not a prediction.
- `cpi_revert`: the caller absorbs the failure (checks both codes, emits
  them, returns success) — prediction: the transaction SUCCEEDS, the callee's
  side effects roll back, and the caller still pays the callee's execution
  up to the revert (hop-sized CU). If instead the whole transaction reverts
  despite the caller absorbing the error, that is a finding.
- Account passing: predicted **same indices as the top-level transaction**
  (spec: "same transaction context") — tested by invoking ex02's
  `increment_e` through CPI and reading the emitted counter value.

Falsifiable core: H-A vs H-B differ by 4,096 per hop — unmissable; Pages
Used per depth is a second independent discriminator.
