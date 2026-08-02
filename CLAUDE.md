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
- Official skill pack: `npx skills add https://thru.org/docs` (`thru-best-practices`; the `Unto-Labs/ai` repo form fails — private/404)
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
| 2026-08-02 | Memory **stores** are charged per width, same as loads (06 `spin_store`: 10 instr bytes + 1 = 11 CU/iter, exact) | **The repo assumed stores were free — but `/spec/runtime/resources.md` documents sb/sh/sw/sd costs explicitly, with a worked `sd` = 12 CU example.** Unread spec page, not undocumented behavior | CONFIRMS SPEC at ratio 1.000. This assumption opened noop's "36 CU gap" (32 of the 36; 4 CU remain unexplained). Read resources.md before decomposing anything |
| 2026-08-02 | Instruction cost tracks encoding **bytes**, not instruction count (`spin_wide`: same 4 instructions, 2× bytes, exactly 2× CU) | This is the spec's own core rule ("1 CU per byte of instruction or data processed") — a verification, not a discovery | CONFIRMS SPEC (ratio 1.000); codegen density is a first-order cost lever |
| 2026-08-02 | A syscall that does nothing still costs 512 (`tsys_log` len 0 = +530 incl. dispatch; `log8` adds exactly 8) | Spec already states base 512 + per-syscall extras — verification, not a discovery | CONFIRMS SPEC. Create's ~1,430 residual is payload work, not a different base |
| 2026-08-02 | `tsys_set_anonymous_segment_sz` **rejects non-page-multiple sizes** (grow by 8/100/2048 bytes all error; 4096 works) | Anonymous segment API is page-granular | Also means per-byte vs per-page anonymous charging is untestable below a page — flagged UNVERIFIED in AUDIT.md |
| 2026-08-02 | Resize SU = **ceil(bytes grown / 4096)** — 4096→8192 consumes SU=1, refuting the floor(target/4096) reading | Discriminator run for the audit | Shrinks and no-ops: SU=0 |
| 2026-08-02 | Audit demoted several findings to UNVERIFIED (anonymous per-byte unification, create-residual attribution, 04 intercept decomposition, docs reconciliation, event-header layout) | Their refuting results were unreachable in the original experiments — same failure mode as 03's full-page-only account | See AUDIT.md; README flags them |
| 2026-08-02 | Trailing-page CoW: writing into a page holding only 904 of its 4,096 bytes charges **904** (`write_at(4096)` = W0 − 3,192 exactly; offset 4999 identical; two-page write adds to the whole account, 5,000) | CoW = 1 CU per byte *present* in the copied page — min(page size, bytes in page) | Corrects the spec's flat "page fault = 4,096 CU"; predictions committed before measurement (`d74af38`) |
| 2026-08-02 | Three repo "discoveries" (store costs, bytes-not-count, 512 base) turned out to be documented in `/spec/runtime/resources.md` — incl. a worked `sd` = 12 CU example | The repo never read that spec page; the store assumption it would have prevented opened the 36 CU gap | All three reframed CONFIRMS SPEC everywhere; rule: read the spec section before claiming novelty |
| 2026-08-02 | Six issues filed upstream after re-scrutiny (all survived): Unto-Labs/thru #34 (Windows toolchain), #35 (--compute-units help), #36 (consumed MU unreported), #37 (1-page stack), #38 (read-only image), #39 (failed txns report no CU) | — | reports/ drafts annotated with URLs |
| 2026-08-02 | **Quickstart-pattern programs cannot be CPI callees**: every CPI failed on the callee's own size check | CPI instruction data arrives in registers a0/a1 (documented in the invoke spec); `tsdk_txn_get_instr_data()` always reads the TOP-LEVEL transaction | Declare `void start(uchar const *data, ulong sz)` — works top-level too. ex02 as-is can never be invoked |
| 2026-08-02 | A callee's `tsdk_revert` **aborts the entire transaction** with the callee's error code; the caller's catch-and-absorb code never runs | `tsys_exit(code, revert=1)` is transaction-fatal from any frame; `invoke_err` is only for syscall-level failures (e.g. depth −24, which DOES return in `invoke_result`) | Don't design CPI error handling around catching callee reverts |
| 2026-08-02 | CPI pricing: first hop 5,567 CU, each additional same-depth hop exactly **1,511**, each depth level exactly **5,607 = 1,511 + 4,096**; Pages = 2+depth, flat for breadth | Callee frame page (4,096) maps once per depth level and is reused across sequential same-depth invokes | Breadth is ~3.7× cheaper than depth per hop; batch sequential calls at constant depth |
| 2026-08-02 | Max call depth is **16** (header's "16 call depths (1..16)" CONFIRMED); the repo's earlier "max 15, corrects header" was a recursion miscount — `cpi_deep(N)` spawns N+1 callee frames because the depth-0 countdown still runs as a frame | Off-by-one in the experiment's own frame accounting, not in the chain | Count frames from the countdown's termination condition; `Pages Used` = deepest call depth is the cross-check that catches this |
| 2026-08-02 | Marginal hop residual ~487 CU: split between callee path and a possible invoke register-save surcharge is UNVERIFIED — and the surcharge reading **double-counts the spec's own derivation of the 512 base** (32 regs × 8 B × 2) | Two readings fit the same 1,511; the callee-path term was estimated, never exactly counted | Open question written into the 07 README with the discriminating experiment; don't quote "invoke > 512" |
| 2026-08-02 | Event JSON shape varies: an 8-byte event emitted by a CPI callee showed `data_hex` with the full payload (no `event_type` split), plus `call_idx`/`program_idx` attributing the emitting frame | CLI JSON is not schema-stable across event shapes/contexts | Inspect raw JSON before parsing events programmatically |
| 2026-08-02 | Skill verification (fresh install in a scratch dir, trivial program end to end guided by the skill only) exposed two gaps: no headers/skeleton, and the create-vs-upgrade probe read as if it applied to first deploys | A skill is only as good as what a cold reader can execute; the verification program measured 5,315 CU / Pages 2, exactly as the skill's own model predicts | Both fixed in SKILL.md; verify skills by using them cold, not by rereading them |
| 2026-08-02 | ~~Compressed accounts are one-way black holes~~ **RETRACTED**: the "bintrie: key not found" from all decompression surfaces is a **~1–5 minute indexing lag**, not permanence — timed retries at T+1 (fail) and T+5 (all succeed) bracketed it; every "lost" account then decompressed fine | The original session's retries stopped at ~60 s — inside the lag window; the error text reads as fatal | **Retry windows must exceed plausible indexing lag before declaring anything impossible.** The refuting result was one longer retry away |
| 2026-08-02 | Full round trip measured post-lag: decompress 6,211/7,079/11,111 CU at 100/1,000/5,000 B (≈ base + 1 CU/byte revived, SU = pages restored); 65,536 B revived via the CLI's **chunked buffer flow** (32 KiB single-txn ceiling routed around); modify 5,565 ×3; recompress 6,153 = same formula as compress | Decompression is ~1 CU/byte, far below the predicted 2–3 | Large-account revival is multi-transaction, not impossible |
| 2026-08-02 | Explorer MCP (`scan.thru.org/api/mcp`, plain JSON-RPC works) shows compression is executed by the **system program** `taAAAA…EB` (instr type 0x05, proof embedded), and reports **consumed memory units** (CLI never does) | Independent index; richer txn detail than the CLI | Use it for consumed MU and instruction-level forensics |
| 2026-08-02 | The docs' skills command `npx skills add https://thru.org/docs` **works** (well-known-endpoint discovery); this repo's own CLAUDE.md had the broken `Unto-Labs/ai` form | Scrutiny before filing killed a wrong issue draft | Repo reference fixed; planned issue dropped |
| 2026-08-02 | The published decompress law ("base + 1 CU/byte revived") repeated the exact mistake the compress law had just been corrected for: remainders after data alone are 6,111/6,079/6,111 — non-constant, absorbing proof-size variation | The compress fix was not applied repo-wide | **When a law is corrected, grep the repo for every sibling figure fitted the same way.** Refit in progress (blocked mid-measurement by an RPC outage) |
| 2026-08-02 | Recompressing the account revived via the **chunked** flow fails: "bintrie: key already exists" (2 attempts); accounts revived via single-txn decompress recompress fine | The chunked revival path appears to leave the old leaf in the bintrie (or the CLI wrongly requests a creating proof for a key that legitimately still exists) | Don't chunked-revive an account you intend to recompress until this is resolved |
| 2026-08-02 | Six more exact compress-formula points: recompress s1000 = 7,021 (proof 168), s5000 = 11,053 (200), probes 36,085/37,053/37,821/38,517 at 30,000/31,000/31,800/32,400 B, fresh 64K = 71,653 (264) | Every one lands on 5,853 + S + proof to the CU | The formula now has ~12 exact points |
| 2026-08-02 | Invoke surcharge KILLED by exact count: marginal hop = 1,024 syscall bases + 284 caller-side + 195 callee-side = 1,503 vs measured 1,511 (residual 8) | The old "callee ~230 + save ~256" split ignored the caller helper + tsys_invoke wrapper (284 CU/hop) | `tsys_invoke` costs its plain 512 base — CONFIRMS the spec's derivation |
| 2026-08-02 | Full alphanet RPC outage mid-session ("upstream connect error … connection termination" on `getversion`), right after 8 back-to-back compressions | Infra outage (correlation with our load unproven) | Measurement scripts need outage-tolerant retries distinct from indexing-lag retries; 7 accounts left compressed pending the refit |
| 2026-08-02 | **Post-outage proof-root desync — a third proof-surface failure mode**: freshly fetched proofs of BOTH kinds are rejected on-chain with `Invalid state proof (-23)` (creating proofs fail account creation and program deploys; existing proofs are served by `prepare-decompression` but the decompress txn reverts with user_error = (u64)−23); simultaneously the two newest compressed leaves return "bintrie: key not found" 40+ min after compression | After the RPC outage the proof service's trie root no longer matches the chain's | Distinguish three modes: indexing lag (~1–5 min, transient), full RPC outage (connection errors), and root desync (−23 on valid-looking proofs). Only the first is safe to wait out blindly |
| 2026-08-02 | Under the degraded state, one decompression buffer-upload txn reported **5,413,212 CU** (p31800, 31,800 B) — ~170× the per-byte expectation | Anomalous; measured once on a degraded chain | Do NOT treat as a figure; re-measure the chunked flow's txns on a healthy chain |
| 2026-08-02 | **Consumed MU ≡ `Pages Used`** — identical in 28/28 transactions incl. the deciding case: event pages are CU-free but MU-charged (emit8 → MU 9) | The CLI prints consumed memory units under another name; the "mixed meter" is the MU meter | Size `req_memory_units` straight from Pages Used; issue #36's premise corrected on the issue |
| 2026-08-02 | SU = ceil(bytes grown/4096) survives the sharp 4097 test (target spans 2 pages, SU=1); shrinks/delete/compress always SU 0 | Grown-bytes formula, not target-pages; nothing ever refunds SU | Storage costs are strictly one-way at the SU level too |
| 2026-08-02 | The SDK's default **-O3 built the LARGEST binary** of six levels for SHA-256 (3,496 B vs 1,216 at -O1, 2.9×) | -O3 unrolls aggressively; deploy cost tracks size ~1 CU/byte | Hot-path CU comparison pending (deploys blocked); don't switch defaults on size alone |
| 2026-08-02 | The −23 desync **only breaks proof-consuming operations** — all 24 fresh executions on deployed programs reproduced their historical CU exactly during it | Execution, accounts, events, CPI all healthy; creates/deploys/compress-decompress blocked | A degraded chain can still be measured — for everything except state proofs |
| 2026-08-02 | **MU charges PEAK, not end-state**: grow-to-8-pages-then-shrink-to-1 reports MU 8 (3×, Explorer-confirmed); CU shows +538 for the extra syscall and no refund | CONFIRMS the spec's "peak usage" rule; "shrinking releases" means re-growth headroom, not the bill | The last MU unknown closed; `Pages Used` tracks the same peak |
| 2026-08-02 | **Program upgrades WORK during the proof-root desync** (ex05 upgraded to 672 B while creates still fail −23) | The upgrade path avoided fresh bintrie-proof-consuming creates this time (finalized-buffer reuse observed on an earlier retry; not fully characterized) | You can ship code to existing programs during a desync; you cannot ship new programs or accounts |
| 2026-08-02 | Refinement of "failed txns report no CU": an included-but-reverted txn IS retrievable via `thru txn get` if you captured its signature (the −23 deploy txn returned its slot and result) — the CLI's `execute` error path just never prints the sig | Two different gaps: sig not surfaced on failure vs txn not queryable | Capture sigs from intermediate "Transaction completed" lines; whether consumed CU appears for reverted txns via `txn get` still unchecked |
| 2026-08-02 | The spec's syscall page (earlier 403) is reachable and lists **15 syscalls (0x00–0x0E)** — matching the SDK header exactly except **0x0F account_create_eoa is SDK-only** | Doc-site availability is flaky; retry transient 403s before concluding a page doesn't exist | Coverage map updated with spec-vs-SDK provenance |
| 2026-08-02 | Traceability walk found the repo stores no raw measurement logs — every figure lives only in prose tables; two formula errors slipped through this way | Raw CLI outputs stayed in session transcripts | Keep a raw-results file per example going forward; four discrepancies found and fixed this pass (SU-ladder protocol wording, 02 baseline version note, stale PUBLISH ledger counts, syscall-map provenance) |
| 2026-08-02 | Compression: **5,853 + 1 CU/account byte + 1 CU/proof byte**, exact at five sizes incl. an out-of-sample 100 B repro (6,153 = 5,853+100+200), SU = 0 at every size | Account and proof are both charged per byte; **no state-unit refund — compression is validator-side, not economic for the payer** | The scaling-story economics question, answered. Corrected from "6,053 + 1 CU/B slope 1.0005" |
| 2026-08-02 | The first published compression fit ("6,053 + 1.0005/B") was wrong: it absorbed proof bytes into the slope and anchored the intercept on the two accounts that happened to have 200-byte proofs | Repo didn't apply its own already-established law (proof bytes = 1 CU each, ex02) before fitting a new one | **Apply established laws first, then fit the residual.** Subtracting known terms exposed a constant 5,853 at all five points |
| 2026-08-02 | The CLI compressed a **program-owned** account with only the fee payer signing | Compression is a system-level operation not involving the owner program | Owner programs cannot prevent their accounts being compressed by the fee payer |
| 2026-08-02 | `tsys_account_compress` from a program reverts with syscall error −43 for both obtainable proof kinds (creating; existing fails at the RPC) | Expected proof shape undocumented; the CLI builds its own internally | Program-side compression: unresolved, time-boxed out after 2 attempts |
| 2026-08-02 | A **90-second-old creating proof was accepted** (create succeeded at the normal CU) | Proofs are not tightly slot-bound — refuted this repo's own committed staleness prediction | Tolerance boundary untested; don't assume proofs expire quickly, don't assume they never do |
| 2026-08-02 | `make-state-proof existing/updating` fails with "bintrie: key not found" for LIVE accounts too | Live accounts are not in the compressed bintrie; the proof service currently serves absence (creating) proofs only | Only `creating` proofs are usable today |
