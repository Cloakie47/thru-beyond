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
