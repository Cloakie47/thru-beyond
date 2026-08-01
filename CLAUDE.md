# Beyond — Thru and Beyond

Working, deployed, cost-measured example programs for the Thru blockchain.

**The point of this repo is the numbers.** Every example must actually deploy to alphanet and report its real compute-unit cost. An example that compiles but was never executed on-chain is worthless here. Never invent, estimate, or infer a CU figure — only report what `thru txn execute` printed.

---

## Hard rules

1. **Never read, print, echo, or commit `~/.thru/cli/config.yaml`.** It stores the private key as plaintext hex. If a task seems to need it, stop and ask.
2. Never commit anything under `.thru/`, `build/`, or any file containing a key or seed phrase.
3. Never claim a measurement that did not come from real CLI output.
4. Use `--fee 0` for all test transactions.
5. If the docs and observed behavior disagree, **the observed behavior is the finding** — record it in the gotchas log below rather than silently working around it.

---

## Reference material

Load these before writing code, not after:

- Doc index: `https://docs.thru.org/llms.txt` — fetch this first, then only the pages the task needs
- Thru's own agent guidance: `https://docs.thru.org/CLAUDE.md`
- Official skill pack: `npx skills add Unto-Labs/ai` (`thru-best-practices`)
- Explorer MCP: `https://scan.thru.org/api/mcp` — use for inspecting deployed accounts, transactions, and on-chain ABIs instead of guessing

---

## The loop

Every example follows the same cycle. Do not skip steps.

```bash
make                                              # -> build/thruvm/bin/<name>.bin
thru program create <seed> <path-to-binary>       # first deploy
thru program upgrade <seed> <path-to-binary>      # subsequent deploys
thru txn execute --fee 0 [--readwrite-accounts <addr>] <program-addr> <hex-instruction-data>
```

Record from the output: `Compute Units Consumed`, `State Units Consumed`, `Pages Used`, and event count/size where relevant. Run three times. Execution is deterministic and integer-only, so identical inputs must yield identical CU — **any variance is itself a reportable finding.**

Useful helpers: `thru program derive-address <program> <seed>`, `thru program seed-to-hex <seed>`, `thru txn make-state-proof creating <addr>`, `thru --json <cmd>` for parseable output.

---

## Environment

Single environment only. If the `make` build requires WSL2, then the CLI, toolchain, and agent all run inside WSL2 — never split across PowerShell and WSL2, or `~/.thru` paths diverge.

- SDK: `~/.thru/sdk/c/thru-sdk`
- Toolchain: `~/.thru/sdk/toolchain/`
- Network: alphanet (`https://rpc.alphanet.thru.org`)

---

## Known constraints

Seeded from the spec. Verify each against real behavior and correct as you go.

**VM**
- RISC-V RV64I + M, C, B (bit manipulation, CLMUL), Zknh (SHA-256/512 acceleration)
- **No floating point.** All math is integer or hand-rolled fixed point.
- Single-threaded, fully deterministic
- Unaligned data access faults. Instructions align to 16 bits.

**Cost model**
- 1 CU per byte of instruction or data processed
- 32-bit instruction = 4 CU; compressed 16-bit = 2 CU
- Syscall base cost = 512 CU (only 16 syscalls exist)
- **Page fault = 4,096 CU** — pre-allocate hot paths
- Memory units charge *peak* usage, not cumulative; shrinking releases them
- Budgets are declared upfront: `req_compute_units` (uint32), `req_state_units` (uint16), `req_memory_units` (uint16)

**Accounts**
- Index 0 = fee payer, index 1 = program, then writable, then read-only
- Writable and read-only lists must each be sorted ascending; no duplicates
- Max 16MB data per account; max 1024 accounts and 32KiB per transaction
- **A single memory access must not span a 4KB page boundary** — this faults
- First write to a page triggers copy-on-write and costs 1 MU
- Only the owning program may write an account's data
- Call `tsys_set_account_data_writable()` before mutating

**Program shape**
- Entry point: `TSDK_ENTRYPOINT_FN void start(void)`
- Terminate with `tsdk_return(TSDK_SUCCESS)` or `tsdk_revert(code)` — programs do not return values
- Headers: `<thru-sdk/c/tn_sdk.h>`, `<thru-sdk/c/tn_sdk_syscall.h>`
- Validate instruction data size before casting to any struct
- Use `__attribute__((packed))` on all wire structs; little-endian encoding

**ABI**
- Hand-written YAML — it does **not** auto-sync with the program. Always roundtrip with `thru abi analyze` / `thru abi reflect` before publishing.

---

## Per-example checklist

- [ ] Builds clean
- [ ] Deployed to alphanet, program address recorded
- [ ] Executed successfully, CU/SU/Pages recorded from real output
- [ ] Measured three times, figures identical
- [ ] `run.sh` reproduces the whole thing from scratch
- [ ] Example README states what it teaches and the gotcha that cost time
- [ ] Root README cost table updated
- [ ] Any surprise added to the gotchas log below

---

## Gotchas log

Every entry here becomes a line in the agent skill later. Write them down as they happen, even the embarrassing ones.

| Date | What broke | Root cause | Fix |
|---|---|---|---|
| 2026-08-01 | `thru dev toolchain install` fails on Windows with "Failed to detect OS: program not found" | Installer shells out to `uname`, which doesn't exist on Windows | Run CLI + toolchain + build entirely inside WSL2 |
| 2026-08-01 | `npm i -g thru` inside WSL installed the *Windows* package to `C:\Users\...\.npm-global` | WSL PATH inherits Windows Node/npm (`/mnt/c/Program Files/nodejs`); Ubuntu had node but no Linux npm | Skip npm: extract `thru-cli-Linux-x86_64-<ver>.tar.gz` from GitHub releases to `~/thru-cli` (no root needed) |
| 2026-08-01 | `make` fails: "RISC-V toolchain not found - reached root directory" | SDK's `with-gcc.mk` finds the toolchain by walking **up** from `$(CURDIR)`; a repo on `/mnt/c` never reaches `~/.thru` | `export RISCV_TOOLCHAIN_ROOT="$HOME/.thru/sdk/toolchain"` |
| 2026-08-01 | Doc URLs like `thru.org/docs/<page>` (no trailing slash) 301-redirect to `thru.org:8080`, which refuses connections | Doc-site redirect misconfiguration | Always request doc pages with a trailing slash |
| 2026-08-01 | (Not broken, but worth knowing) config.yaml never needs editing | — | `--url https://rpc.alphanet.thru.org` global flag targets alphanet per-invocation; `thru network` manages profiles; empty instruction data `""` is accepted by `txn execute` |
| 2026-08-01 | `thru program create` on an already-used seed uploads a full temp buffer *then* fails (manager error 0x0504), orphaning the buffer accounts | Existence check happens in Step 2, after the Step 1 upload | Probe with `thru getaccountinfo <program-addr>` first and pick create vs upgrade |
| 2026-08-01 | Quickstart/DevKit docs reference CLI v0.2.27; actual latest release is v0.3.2 | Docs lag releases | Always pull the latest tag from GitHub releases, not the version pinned in docs |
| 2026-08-01 | Toolchain download is ~1.1GB (`thru-toolchain-Linux-x86_64-v0.3.2.tar.gz`), nothing in the docs warns about this | Undocumented | Budget time/disk; run `thru dev toolchain install` in the background |
| 2026-08-01 | `thru txn execute --help` contradicts itself on the compute-units default: prose says "defaults to 1000000000", clap annotation says `[default: 300000000]` | Stale doc string in the CLI | The real default is 300,000,000 (`req_compute_units` is a fixed flag default, never estimated) |
| 2026-08-01 | Docs say `getversion` needs `rpc_base_url` set in config.yaml first | Docs assume config editing | Not needed — global `--url` flag works on every command |
| 2026-08-01 | Reading a never-touched account page cost no 4,096 CU and didn't bump `Pages Used` (03-storage `read_p1`: 4,892 CU, 1 page) | The "page fault = 4,096 CU" charge is copy-on-write — it fires on the *first write* to a page, never on reads; `Pages Used` counts written pages | Docs need the "on write" qualifier; spread reads freely, batch writes per page |
| 2026-08-01 | A handler doing strictly *more* work (extra store) measured 27 CU *less* (`write_p0_x2` 9,555 vs `write_p0` 9,582) | CU charges per instruction byte processed; compiler layout (compressed 16-bit vs 32-bit encodings, branch order) outweighs one extra store | Treat small CU differences between similar programs as codegen noise, not signal |
| 2026-08-01 | `thru program create` prints signatures for 4 of its 5 transactions — the chunk-write signature is never shown | CLI omits it from batched-submit output | Recover it via `thru account transactions <temp-buffer-account>`, then `thru txn get` |
| 2026-08-01 | `thru txn make-state-proof` panics "failed printing to stdout: Broken pipe" when piped to `head` | Rust CLI doesn't handle SIGPIPE/closed stdout | Capture full output to a file, then slice |
| 2026-08-01 | `Requested Memory Units` exists in `thru txn get` output but *consumed* MU is reported nowhere | CLI/RPC omission | MU consumption currently unmeasurable from the CLI |
| 2026-08-01 | Verified: unflagged `txn execute` requests exactly 300,000,000 CU (via `thru txn get`), confirming the clap annotation over the "1000000000" prose | Stale doc string | — |
| 2026-08-01 | 04-hash used `Pages Used: 2` at every input size despite writing no account data (noop = 1) | The +1 page correlates with event emission / deeper stack — not attributable further from CLI output | Budget +1 written page (4,096 CU) for event-emitting programs; a noop+emit microbenchmark would isolate it |
| 2026-08-01 | First 8 bytes of a `tsys_emit_event` payload become the event's `event_type` (LE u64); `--json` shows only the remaining bytes as `data` | Runtime event format has an 8-byte type prefix; SDK docs don't mention it | Reassemble `event_type \|\| data` to recover the payload; design payloads with the first 8 bytes as a deliberate tag |
| 2026-08-01 | Zknh SHA-256 speedup is only 1.30×, not the expected ~1.6× | SDK march also enables `zbb`, whose single-instruction rotates already make portable sigma functions cheap (5 instr vs 1) | Real but modest: 12,846 → 9,885 CU per 64-byte block. `__builtin_riscv_sha256*` works out of the box in the shipped GCC 15.2 |
| 2026-08-01 | First-principles instruction-count estimates missed measured slopes by 20–35% (04 predictions) | Hot crypto loops compile almost entirely to full-width 4-CU encodings; hand estimates of instr/round run low | Don't predict the instruction term — measure one size and calibrate; linearity then predicts all other sizes to ~0.01% |
| 2026-08-02 | Repo's committed 04 fits were wrong: slope B 9,885 → **9,884**, intercepts 6,113/6,112 → **6,129/6,129 (identical)** | Original derivation anchored pairs at the 0-byte point, which deviates −16 CU from the clean line (0 vs 32 B differ by +138 CU at equal block count — per-input-byte tail effect) | Fit slopes only from points with distinct block counts and rem=0; corrected in the 04 README |
| 2026-08-02 | An 8,256-byte stack frame faults (`VM_FAILED`) on every instruction, even ones not using the buffer | SDK entry stub maps exactly ONE 4KB stack page (`entrypoint.S`: `set_anonymous_segment_sz(sp−4096)`); frames allocate in the prologue; the VM never grows the stack on demand | Call `tsys_set_anonymous_segment_sz(stack_top − pages×4096)` before using big frames; stack top = `0x05_0001_000000` |
| 2026-08-02 | Writes to static (.bss/.data) arrays fault | Program image maps into `TSDK_SEG_TYPE_READONLY_DATA` — Thru C programs have **no writable globals** | All mutable state in accounts, (grown) stack, or heap segment |
| 2026-08-02 | Static zeroed arrays ballooned the binary 488 B → 32,840 B | .bss is stored literally in the program image | Don't declare large static buffers even if they were writable |
| 2026-08-02 | Anonymous pages charge 4,096 CU at segment **allocation**, not at first write | Growing stack 1→8 pages cost 7×4,096 + 512 + 28 exactly; touching pre-mapped pages costs ~15 CU | The "page fault = 4,096" rule is really "page *provisioning* = 4,096": CoW for account pages, map-time for anonymous pages |
| 2026-08-02 | Event pages are **counted in `Pages Used` but never charged** | Emit cost = 570 + 1 CU/byte, exactly, continuous across the 4096/8192 boundaries while Pages steps 9→10→11 | `Pages Used` is a mixed meter — don't read it as CU/4,096. Explains 04's Pages=2 |
| 2026-08-02 | `tsys_exit` measures as **0 CU**; the 512 in noop's 4,763 belongs to the entry stub's `set_anonymous_segment_sz` | Every reconciliation (4,763; 4,895; 34,107) lands exactly only with exit=0 | 01's decomposition corrected in its README |
| 2026-08-02 | Zero-length `tsys_emit_event` executes (costs 570 CU) but records no event | Runtime drops empty events: EvCount stays 0 | — |
| 2026-08-02 | `user_error` on `VM_FAILED` echoes a fault-related register (observed: faulting address in one layout, loop bound in another) | Diagnostic register echo, layout-dependent | Don't parse it as an error code from your program |
| 2026-08-02 | Failed transactions report **no consumed CU anywhere** — not in `txn execute` error output, not in `--json`, and they don't appear in `thru account transactions` | CLI/RPC omission | CU of failing paths is unmeasurable; blocked the behavioral exit-cost test (closed via disassembly instead) |
| 2026-08-02 | Per-event record overhead is exactly **10 bytes** (8-byte `event_type` + 2), charged per event | Bisected: single-event page boundary at N=4086/4087; two-event boundary at 4076/4078 = one overhead earlier | Budget N+10 event-buffer bytes per event when watching `Pages Used` |
| 2026-08-02 | `increment` on an 8-byte account = 5,523 CU — **no flat 4,096 CoW charge** | Account CoW costs 1 CU per byte actually copied; 03's +4,096/page was the full-page special case | "Page fault = 4,096" is really "copy cost = bytes copied"; small accounts write nearly free |
| 2026-08-02 | Account resize charges **exactly 1 CU per byte grown** (zero-fill); shrink is constant-cost (5,967 in ex02's shape); SU ≈ pages grown | Measured at 5 sizes, exact to the byte (8→65536: +65,528) | Growth is charged at resize, like the stack — allocation = zero-fill at 1 CU/byte everywhere |
| 2026-08-02 | `tsys_account_delete` fails while the account has data | Undocumented precondition beyond balance=0/nonce=0: **data size must be 0** | Resize to 0, then delete (5,401 CU); delete refunds nothing (SU=0, never negative) |
| 2026-08-02 | `create` cost varies across accounts: 7,791 (200 B proof) vs 7,759 (168 B proof) | State proof is instruction data at 1 CU/byte; proof size depends on the account/state | Always record proof size next to a create measurement |
| 2026-08-02 | Docs' quickstart counter figures (7,524 create / 5,980 increment) reconcile with today's chain | Their increment emits an event (≈ our 6,065, Δ85 codegen); create differs by proof size at 1 CU/byte | Not a cost-model change; compare like shapes and state proof bytes |
| 2026-08-02 | Memory **stores** are charged 1 CU/byte, same as loads (06 `spin_store`: 10 instr bytes + 1 = 11 CU/iter, exact) | Previously assumed free; nowhere documented | Count `sd`/`sw`/`sb` bytes in any hand-decomposition — this was 32 of noop's "36 CU gap" (4 CU remain unexplained) |
| 2026-08-02 | Instruction cost tracks encoding **bytes**, not instruction count (`spin_wide`: same 4 instructions, 2× bytes, exactly 2× CU) | CU charges per byte of instruction fetched | Docs' 4/2 CU rates verified exactly (ratio 1.000); codegen density is a first-order cost lever |
| 2026-08-02 | A syscall that does nothing still costs 512 (`tsys_log` len 0 = +530 incl. dispatch; `log8` adds exactly 8) | 512 is a per-call base, not an average; payload charged on top | Create's ~1,430 residual is payload work, not a different base |
| 2026-08-02 | `tsys_set_anonymous_segment_sz` **rejects non-page-multiple sizes** (grow by 8/100/2048 bytes all error; 4096 works) | Anonymous segment API is page-granular | Also means per-byte vs per-page anonymous charging is untestable below a page — flagged UNVERIFIED in AUDIT.md |
| 2026-08-02 | Resize SU = **ceil(bytes grown / 4096)** — 4096→8192 consumes SU=1, refuting the floor(target/4096) reading | Discriminator run for the audit | Shrinks and no-ops: SU=0 |
| 2026-08-02 | Audit demoted several findings to UNVERIFIED (anonymous per-byte unification, create-residual attribution, 04 intercept decomposition, docs reconciliation, event-header layout) | Their refuting results were unreachable in the original experiments — same failure mode as 03's full-page-only account | See AUDIT.md; README flags them |
