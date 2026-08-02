---
name: thru-programs
description: Measured constraints, costs, and workflow for writing C programs on the Thru blockchain. Use when writing, porting, debugging, or cost-optimizing a Thru program, or when using the thru CLI to deploy and execute. Everything here was measured on-chain (alphanet, thru CLI 0.3.2, 2026-08) by github.com/Cloakie47/thru-beyond; figures are exact, not estimates.
license: MIT
metadata:
  author: cloakie
  version: "1.0.0"
---

# Thru C Programs — measured constraints and costs

All figures: alphanet, thru CLI 0.3.2, 2026-08. Execution is deterministic —
identical inputs give identical CU, so one measurement is exact.

## 1. Memory model traps (these break you before anything else)

- **No writable globals or statics.** The program image — .data and .bss
  included — is mapped read-only. Any write to a static faults at runtime
  (`VM_FAILED`); there is NO build-time diagnostic. Also .bss is stored
  literally in the image (a 16KB static array grows a 500 B binary to 33KB).
  All mutable state lives in account data, the stack, or grown segments.
- **The stack never grows.** The SDK entry stub maps exactly ONE 4KB stack
  page. Any function whose frame pushes total stack past 4,096 bytes faults
  on every path — even paths that never touch the big buffer, because frames
  allocate in the prologue. For more, call
  `tsys_set_anonymous_segment_sz((void*)(0x050001000000UL - pages*4096))`
  first (costs 512 + 4,096/new page).
- `tsys_set_anonymous_segment_sz` is **page-granular**: sizes that are not
  4,096-multiples are rejected with an error.
- **A single memory access must not span a 4KB page boundary** — it faults.
  Use byte accesses near boundaries; never memcpy across offset 4096.
- **No floating point.** Integer / fixed-point only (RV64IMC + zba zbb zbc
  zbs zknh).
- **CPI callees must read instruction data from entrypoint registers**:
  declare `TSDK_ENTRYPOINT_FN void start(uchar const *data, ulong sz)`.
  The quickstart's `tsdk_txn_get_instr_data()` reads the TOP-LEVEL
  transaction only — a program using it misparses every cross-program
  invocation. The register form works top-level too; use it always.
- On a `VM_FAILED` fault, `user_error` echoes a fault-related register
  value (address or loop bound) — it is not your error code.

## 2. The cost model (measured domain: alphanet, CLI 0.3.2, single program
   or CPI to depth 15, accounts 0–65,536 B)

```
CU = 512 × syscalls          (tsys_exit is free — measured 0)
   + 4,096 × anonymous pages mapped (charged at allocation, not first touch)
   + 1 CU × bytes processed:
       instruction encoding bytes (4 per 32-bit, 2 per compressed)
       load/store bytes (per access width; sd = 8)
       bytes grown on account resize (zero-fill; shrinking is free)
       bytes PRESENT in each account page copied on first write per txn
         (CoW = min(4096, bytes in page) — an 8-byte account's write
          copies 8 bytes, not 4,096)
       event / log / state-proof payload bytes
```

Fixed floor: every transaction pays 512 + 4,096 (entry stub's segment-map
syscall + its stack page) before user code. Minimal program = 4,763 CU.

Reference points: account create ≈ 7,760–7,860 (varies 1 CU per state-proof
byte; proofs differ per account), 8-byte counter increment 5,523, +event
6,065, delete 5,401 (requires data size 0 first; refunds nothing), deploy of
an 838 B program 320,292 (single sample). CPI: 1,511 CU per same-depth hop,
+4,096 per depth level (frame pages are reused across sequential calls at
the same depth); 16 call depths total (root = depth 1), exceeding returns
−24. Compression: 5,853 + 1 CU/account byte + 1 CU/proof byte, no SU refund
(validator-side, not economic); after compressing, ALL decompression
surfaces fail with a fatal-looking "bintrie: key not found" for ~1–5 min of
indexing lag — wait it out, do not conclude the account is lost.

Practical rules:
- **Keep hot fields within the first 2,047 bytes of account data** — larger
  offsets exceed the 12-bit immediate and cost extra addressing instructions
  (measured +8 CU at offset 4096).
- **Denser codegen is literally cheaper**: cost tracks encoding BYTES, not
  instruction count — the same instructions full-width cost exactly 2×.
- **Small accounts write nearly free** (CoW charges bytes present); the
  "4,096 page fault" only bites full pages and anonymous allocations.
- Everything is linear: calibrate one size on-chain, predict any other to
  ~0.01%. Do not estimate instruction counts by hand (errors of 20–35%).
- State units: resize consumes ceil(bytes grown / 4096) SU; most other ops 0.
- Prefer breadth over depth for CPI (~3.7× cheaper per hop), and never
  design around catching a callee's revert: `tsdk_revert` in ANY frame
  aborts the whole transaction with the callee's code. Only syscall-level
  invoke errors (e.g. depth −24) return in `invoke_result`.

UNVERIFIED (do not treat as established): anonymous allocation being
per-byte rather than per-page (untestable — the API is page-granular);
the ~1,430 CU inside account_create beyond base+proof. (Resolved: invoke
costs exactly its 512 base — an exact disassembly count left an 8 CU
residual, killing the register-save-surcharge reading.)

## 3. Build / deploy / measure loop

Project layout (SDK make system):

```
project/
├── GNUmakefile        # 3 lines:
│     BASEDIR:=$(CURDIR)/build
│     THRU_C_SDK_DIR:=$(HOME)/.thru/sdk/c/thru-sdk
│     include $(THRU_C_SDK_DIR)/thru_c_program.mk
└── examples/
    ├── Local.mk       # $(call make-bin,<name>_c,<name>,,-ltn_sdk)
    └── <name>.c
```

Complete minimal program (this exact skeleton built, deployed, and measured
5,315 CU / Pages 2, 3× identical, as the model above predicts):

```c
#include <stdint.h>
#include <thru-sdk/c/tn_sdk.h>          /* types, TSDK_ENTRYPOINT_FN */
#include <thru-sdk/c/tn_sdk_syscall.h>  /* tsys_* syscalls */

TSDK_ENTRYPOINT_FN void start(uchar const *instruction_data,
                              ulong instruction_data_sz) {
    (void)instruction_data;
    ulong v = instruction_data_sz;
    tsys_emit_event(&v, sizeof(v));
    tsdk_return(TSDK_SUCCESS);   /* or tsdk_revert(code); never fall off */
}
```

The SDK compiles with `-Werror -Wall -Wextra -Wpedantic -Wconversion` —
cast explicitly and mark unused params.

```bash
# toolchain lives in $HOME; if the project dir is elsewhere (e.g. /mnt/c),
# the SDK's upward search fails — set this or the build dies:
export RISCV_TOOLCHAIN_ROOT="$HOME/.thru/sdk/toolchain"
make                                    # -> build/thruvm/bin/<name>_c.bin

U="--url https://rpc.alphanet.thru.org" # never edit ~/.thru/cli/config.yaml
                                        # (it holds the plaintext private key)

# First deploy of a new seed: just create — it prints the program address.
thru $U program create <seed> ./build/thruvm/bin/<name>_c.bin
# Save that address. On EVERY later deploy, probe before choosing create
# vs upgrade: a failed `program create` on an existing seed uploads a full
# temp buffer BEFORE noticing, then errors (manager 0x0504), orphaning the
# buffer accounts on-chain. Never use create-failure as the probe:
if thru --quiet $U getaccountinfo "$SAVED_PROG_ADDR" >/dev/null 2>&1; then
  thru $U program upgrade <seed> ./build/thruvm/bin/<name>_c.bin
else
  thru $U program create  <seed> ./build/thruvm/bin/<name>_c.bin
fi

thru $U txn execute --fee 0 [--readwrite-accounts <addr>] <program> <hex>
# read: Compute Units Consumed / State Units Consumed / Pages Used
```

Account creation (PDA): `derive-address <prog> <seed>` →
`txn make-state-proof creating <pda>` (capture to a FILE — it panics with
"Broken pipe" if piped to head) → instruction data =
`<type u32><idx u16><seed 32B (seed-to-hex)><proof_size u32 LE><proof>`
→ execute with `--readwrite-accounts <pda>`. Account lists must be sorted
ascending. Wire structs: `__attribute__((packed))`, little-endian.

## 4. Gotchas that change what you do

- **Failed transactions report NO consumed CU anywhere** (not in errors,
  not via `txn get`, not listed in `account transactions`). You cannot
  profile revert paths. Design measurements to succeed.
- **`Pages Used` IS the consumed memory units** (proven identical in 28/28
  transactions via the Explorer) — the third budget, printed unlabeled.
  It is NOT a CU meter: it counts CU-charged pages AND CU-free event pages
  (which do consume MU). Size `req_memory_units` from it. State units:
  ceil(bytes grown/4096) per resize, 1 per create, never refunded by any
  operation.
- The SDK's default `-O3` produced the LARGEST binary of six optimization
  levels for real code (2.9× `-O1`) — deploy cost tracks size directly.
  Whether -O3 still wins on hot-path CU is unmeasured; don't assume either
  way.
- Events: the first 8 payload bytes become the event's `event_type` tag;
  reassemble `event_type || data` to recover the payload. ~10 bytes of
  per-event record overhead land in the event buffer page accounting.
  Zero-length emits execute (570 CU) but record no event. Event JSON shape
  varies by size/context — inspect before parsing.
- `tsys_account_delete` requires data size 0 (undocumented) — resize to 0
  first.
- Doc URLs need a trailing slash (`thru.org/docs/<page>/`) — without it
  they 301 to an unreachable port.
- Toolchain install only works on Linux (`thru dev toolchain install`
  shells out to `uname`); use WSL2 on Windows, ~1.1GB download.
- The unflagged `txn execute` requests 300,000,000 CU (the help text's
  "defaults to 1000000000" prose is wrong).

## 5. The discipline (this is why the numbers can be trusted)

- **Read the spec section before claiming novelty.** Three of this repo's
  "discoveries" (store costs, bytes-not-count, the 512 base) were already
  in `/spec/runtime/resources.md` — and the store assumption that page
  would have prevented caused a 36 CU accounting hole.
- **Commit predictions before measuring.** State the CU each hypothesis
  implies; a killed hypothesis is a better finding than a confirmed one.
- **For every finding, ask what result would have refuted it and whether
  that result was reachable.** Example 03 "confirmed" a flat 4,096 CoW
  charge using an account that could only copy full pages; a 100-byte
  account refuted it in one transaction.
- 3 runs per figure. Determinism means any variance is itself a finding.
- Never report a figure that did not come from real CLI output.
