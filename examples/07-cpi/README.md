# 07-cpi — what a cross-program invocation costs

Two programs: caller (848 B, `ta9TmfhHffn5hJ3P83hC8NtwERjworfg7pSGxU_GrEPEmy`,
seed `example_07_caller`) and a deliberately tiny callee (734 B,
`taAf417KM3aXeDTILaGkk2kUMpJbDadZSGnm-1uSAJIRCu`, seed `example_07_callee`).
Alphanet, thru CLI 0.3.2+54058649, 2026-08-02, `--fee 0`, three identical
runs per successful cell. Predictions committed at `81b3c04` before
measurement ([PREDICTIONS.md](PREDICTIONS.md)).

## Verdict on the central question: H-C — per *depth level*, not per hop

The stack segment is shared (spec: one stack segment per VM), but each
invocation **frame** at a new depth maps one fresh 4,096-CU page via the
callee's entry stub — and that page is **reused** by sequential invokes at
the same depth.

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

- **First hop: 5,567 CU** (10,492 − 4,925) — H-B-sized, because the callee's
  frame page is mapped on first entry.
- **Each additional same-depth hop: exactly 1,511 CU** — the `cpi_n` slope is
  1,511.00 at every pair, with `Pages Used` flat at 2: the frame page is not
  freed on return and costs nothing to reuse. (First-hop premium over the
  marginal hop: 4,056 ≈ 4,096 − small baseline-path differences.)
- **Each depth level: exactly 5,607 CU = 1,511 + 4,096**, with `Pages Used`
  = 2 + N. Depth costs precisely one page more than breadth per hop — the
  cleanest confirmation the model could ask for, since 4,096 and the
  hop cost separate perfectly.
- Marginal-hop decomposition: 1,511 = 512 (invoke base) + 512 (callee entry
  stub's segment syscall, a no-op re-set once the page exists) + ~487
  (callee path + call-site instructions + data). The ~487 is *consistent
  with* the invoke syscall's documented 256-byte register save being charged
  as data writes (callee path estimated ~230 from its disassembly, leaving
  ~256) — so **invoke does appear to cost more than the bare 512 base**, by
  about the size of the register save, but the split is an estimate:
  UNVERIFIED as a decomposition.

## Depth limit

`cpi_deep` N=14 (root + 14 nested = **call depth 15**) is the deepest that
executes. N=15 fails: the deepest callee's `tsys_invoke` returns **−24
(TN_VM_ERR_SYSCALL_CALL_DEPTH_TOO_DEEP)** — observed as our wrapper code
`0x7BE8` = 0x7C00 + (ulong)(−24). The SDK header's comment says "16 call
depths (1..16)"; observed reality is that depth 16 is unreachable —
**maximum achievable call depth is 15**. (CU for the failing transactions is
unobservable: failed transactions report no CU — the known CLI gap.)

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
