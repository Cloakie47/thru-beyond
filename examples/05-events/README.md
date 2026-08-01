# 05-events — which pages actually cost 4,096 CU?

**Question:** example 04's programs showed `Pages Used = 2` (one more than
noop) while emitting events. H1: the event page is counted but not charged.
H2: it is charged and the 04 fit was wrong somewhere.

**Verdict: H1 — decisively — plus a rewrite of the page rule itself.**
Event pages are counted in `Pages Used` and never charged. And the 4,096 CU
anonymous-page charge fires at *allocation* (segment mapping), not at first
write.

One binary (534 B), four instructions, program
`tay1XampjPF__geQXy0YoyM24cCKzL6_AcTS-VTV2C-Add` (seed `example_05_events`).
Alphanet, thru CLI 0.3.2+54058649, 2026-08-02, `--fee 0`, three identical runs
per cell (no exceptions).

Every instruction first grows the stack segment to a constant 8 pages — the
same syscall on every path, so it cancels in all deltas. The fill buffer is
the same maximum size regardless of emitted length.

## Raw measurements

`return_only`: **34,107 CU**, Pages 8. SU = 0 everywhere in this example.

| N | `emit_n` CU | Pages | EvSize | `fill_only` CU | Pages | emit − fill |
|---|---|---|---|---|---|---|
| 0 | 34,713 | 8 | 0 | 34,143 | 8 | 570 |
| 8 | 34,847 | 9 | 8 | 34,269 | 8 | 578 |
| 64 | 35,743 | 9 | 64 | 35,109 | 8 | 634 |
| 512 | 42,911 | 9 | 512 | 41,829 | 8 | 1,082 |
| 2048 | 67,487 | 9 | 2048 | 64,869 | 8 | 2,618 |
| 4000 | 98,719 | 9 | 4000 | 94,149 | 8 | 4,570 |
| 4088 | 100,127 | **10** | 4088 | 95,469 | 8 | 4,658 |
| 4096 | 100,255 | 10 | 4096 | 95,589 | 8 | 4,666 |
| 4104 | 100,383 | 10 | 4104 | 95,709 | 8 | 4,674 |
| 4200 | 101,919 | 10 | 4200 | 97,149 | 8 | 4,770 |
| 8100 | 164,319 | 10 | 8100 | 155,649 | 8 | 8,670 |
| 8192 | 165,791 | **11** | 8192 | 157,029 | 8 | 8,762 |
| 8200 | 165,919 | 11 | 8200 | 157,149 | 8 | 8,770 |

`touch_stack` (one byte written per already-mapped stack page):
P=1: 34,157 · P=2: 34,172 · P=3: 34,187 · P=4: 34,198 — Pages 8 at every P.

(A zero-length emit executes the syscall but records no event:
EvCount = 0 at N = 0.)

## The deciding numbers

**`emit_n(8)` − `return_only` = 740 CU.** Nowhere near 4,608. H1.

**Isolated emit cost = 570 + N exactly, at all 13 points, residual zero.**
570 = 512 syscall + 58 call-setup instructions; then precisely 1 CU per byte
emitted. **No discontinuity at N = 4096 or N = 8192** — the deltas step by
exactly the 8-byte N increments straight through both page boundaries.

**`Pages Used` moves and CU does not.** Pages steps 9 → 10 between N = 4000
and 4088, and 10 → 11 between N = 8100 and 8192, while CU stays on the
570 + N line to the unit. That is the H1 signature, measured. (The boundary
sits ~8–92 bytes before a multiple of 4096 — consistent with a small event
record header sharing the page; the event's first 8 payload bytes are its
`event_type` tag.)

## The control (Part 4): anonymous pages charge at ALLOCATION, not at touch

Writing one byte to each additional pre-mapped stack page costs
**+15 / +15 / +11 CU** (the loop iteration), with Pages flat at 8. Touching
an anonymous page is effectively free.

The charge happens when the segment is mapped. Growing the stack from the
entry stub's 1 page to 8 pages cost, measured against the pre-grow baseline:
**7 × 4,096 + 512 + 28 = 29,212 CU — exact.** And the full return_only
reconciles to the unit:

```
34,107 = 512 (entry stub's segment-map syscall)
       + 4,096 (initial stack page)
       + 287 (entry + dispatch instructions)
       + 540 (our grow call: 512 syscall + 28 setup)
       + 28,672 (7 × 4,096 newly mapped pages)
```

This also resolves 01-noop exactly — and corrects its decomposition:

```
4,763 = 512 (entry stub's set_anonymous_segment_sz syscall — NOT tsdk_return)
      + 4,096 (the one stack page the entry stub maps)
      + 155 (instructions)
```

**`tsys_exit` costs 0 CU.** No other assignment fits: charging exit's 512
overshoots every reconciliation by exactly one syscall.

## Things that broke on the way (all reproducible)

1. **The VM does not grow the stack on demand.** The SDK entry stub maps
   exactly one 4KB stack page (`entrypoint.S`:
   `set_anonymous_segment_sz(sp − 4096)`). An 8,256-byte stack frame faulted
   with `VM_FAILED` on every instruction — including ones that never used the
   buffer, because the frame is allocated in the prologue. Grow the segment
   first via `tsys_set_anonymous_segment_sz`.
2. **Thru C programs have no writable globals.** A second draft used static
   (page-aligned .bss) buffers: every write faulted. The program image —
   .data and .bss included — is mapped in the segment type named
   `TSDK_SEG_TYPE_READONLY_DATA`. All mutable state must live on the (grown)
   stack, the heap segment, or account data. Bonus: the zero-filled arrays
   inflated the binary from ~500 B to 32,840 B — .bss is stored in the image.
3. The `user_error` value on a `VM_FAILED` fault echoes a fault-related
   register value (we observed the faulting buffer address in one layout and
   the loop bound in another), not a real user error code.

Incidental measurement: the volatile byte-fill loop costs almost exactly
15 CU per iteration (15.0007 at N = 8192).

## Reproduce

```bash
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
P="tay1XampjPF__geQXy0YoyM24cCKzL6_AcTS-VTV2C-Add"
# instruction data = <type:u32 LE><n:u32 LE>
thru $U txn execute --fee 0 "$P" 0000000000000000   # return_only
thru $U txn execute --fee 0 "$P" 0100000008000000   # emit_n, N=8
thru $U txn execute --fee 0 "$P" 0200000008000000   # fill_only, N=8
thru $U txn execute --fee 0 "$P" 0300000002000000   # touch_stack, P=2
```

Or `./run.sh` in this directory.
