# PUBLISH — what this repo established, and what it did not

Everything below was measured on alphanet with thru CLI 0.3.2+54058649
(node 0.0.0-local+599daf60), 2026-08-01/02, `--fee 0`, three identical runs
per figure. Nothing here is estimated.

## The model

```
CU = 512 × (charged syscalls)            [tsys_exit measures 0]
   + 4,096 × (anonymous pages mapped)    [charged at allocation, page-granular API]
   + 1 CU × (bytes processed):
       instruction encoding bytes (4 per 32-bit, 2 per compressed)
       load/store data bytes (per access width)
       bytes grown on account resize (zero-fill; shrink is free)
       bytes present in each account page copied on first write
         (CoW = min(4,096, bytes in page))
       event / log / proof payload bytes
```

Every transaction also pays a fixed floor before user code runs: 512 + 4,096
(the SDK entry stub's segment-map syscall and its one stack page) — noop
reconciles to 4,763 with 4 CU unexplained. Two consequences worth more than
the formula: execution is exactly linear (calibrate one size, predict any
other to ~0.01%), and `Pages Used` is not a cost meter (it counts uncharged
event pages too).

**Domain of validity:** alphanet under CLI 0.3.2; syscall base verified for
7 of the 16 syscalls (log, emit, set-segment, set-writable, resize, delete,
invoke — plus exit = 0); account sizes tested 0–65,536 bytes; instruction
term verified on ALU/load/store loop bodies; cross-program invocation
priced across all 16 call depths (example 07: 1,511 CU per same-depth hop,
+4,096 per depth level, frame pages reused for breadth; callee reverts
abort the whole transaction). Account compression round-tripped (example
08): compress = 5,853 + 1 CU/account byte + 1 CU/proof byte with no SU
refund; decompress ≈ base + ~1 CU/byte revived; the proof service indexes
compressed leaves with a ~1–5 minute lag that masquerades as "bintrie: key
not found". No heap-segment measurements. Outside that envelope this model
is extrapolation.

## Where the findings stand against the spec (strict labels)

- **CONFIRMS SPEC: 12** — the per-byte core rule, instruction encoding
  rates, load costs, store costs, 512 syscall base, 4,096 per anonymous
  page, CoW mechanism, per-byte payload charges, determinism, CPI register
  data delivery, transaction-global account indices under CPI, and the
  16-call-depth limit (after retracting our own miscounted "corrects").
- **CORRECTS SPEC: 5** — CoW charges bytes present, not a flat 4,096; reads
  never page-fault-charge; `tsys_exit` is free; the CLI's `--compute-units`
  help contradicts itself; a callee's revert aborts the whole transaction
  despite the C reference's catch-via-`invoke_err` implication.
- **UNDOCUMENTED: 25** — including the 1-page never-growing stack, the
  read-only program image (no writable globals), event pages counted but
  never charged, resize/delete semantics, failed transactions reporting no
  CU, CPI pricing and the SDK accessor trap, Pages = deepest call depth,
  the full compression economics, and the ~1–5 minute compressed-leaf
  indexing lag.
- **UNVERIFIED: 11** — flagged in AUDIT.md; the largest are the anonymous
  per-byte unification (unreachable refutation: the API is page-granular),
  the create-syscall residual (~1,430 CU), and the invoke register-save
  surcharge (in tension with the spec's own derivation of the 512 base).

Two retractions are part of the record, both caught by the repo's own
audit discipline: "max call depth 15" (a recursion miscount; the header's
16 is confirmed) and "decompression impossible" (a ~1–5 minute indexing
lag misread as permanence because retries stopped at 60 s).

A cautionary note this repo earned twice: three "findings" (store costs,
bytes-not-count, 512 base) were briefly presented as discoveries and are in
fact documented in `/spec/runtime/resources.md` — and the store assumption
that page would have prevented is exactly what opened the 36 CU gap. Read
the spec before measuring; measure anyway.

## The three things a new Thru developer most needs to know

1. **Your C runs in a hostile memory model: no writable globals (the program
   image, .bss included, is read-only) and exactly one 4KB stack page that
   never grows.** Ordinary C code compiles cleanly and faults at runtime.
   All mutable state goes in account data or in segments you grow yourself
   with `tsys_set_anonymous_segment_sz` (512 + 4,096/page). Filed as
   Unto-Labs/thru issues #37 and #38.
2. **Cost is per byte, almost everywhere.** Small accounts write nearly
   free (an 8-byte counter increment is 5,523 CU total, not 9,6xx); the
   feared "4,096 page fault" is just what a full page's bytes cost; keeping
   accounts small, code compressed, and hot fields within the first 2,047
   bytes are all the same lever. Deployment is the one big fixed cost
   (320,292 CU for an 838 B program — single sample).
3. **Trust only what you can measure, and know what you can't.** Execution
   is deterministic and exactly linear, so one calibration measurement
   predicts everything — but consumed memory units are reported nowhere,
   failed transactions report no CU at all (issues #36, #39), and `Pages
   Used` mixes charged and uncharged pages.

## Known limitations, in one list

- Single machine, single environment (WSL2 Ubuntu on one laptop).
- One CLI/toolchain/SDK version (0.3.2) against one node version on
  alphanet only; no mainnet, no version comparison.
- Deployment cost is a single sample at one binary size.
- Instruction-path decompositions of the fixed floors are hand-counted from
  disassembly (±4 CU unexplained in noop; 04's intercept term never counted).
- 6 of 16 syscalls measured; no cross-program invocation; no heap-segment
  measurements; no MU measurements (unobservable from the CLI).
- The docs-quickstart comparison carries unexplained residuals of +1.4–2.3%
  (their binary is unavailable).
- Event-buffer provisioning is unobservable; "counted but never charged"
  describes what the CLI shows, not the runtime's internals.
