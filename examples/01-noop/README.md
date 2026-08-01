# 01-noop — the floor cost of a Thru transaction

**What it teaches:** the fixed overhead of entry + exit + transaction
processing. The program body is a single `tsdk_return(TSDK_SUCCESS)` — the
explicit terminator is required, because a bare empty function that falls off
the end of the entrypoint is undefined behavior on the Thru VM.

## Measured cost

Alphanet (`https://rpc.alphanet.thru.org`), thru CLI 0.3.2+54058649,
node 0.0.0-local+599daf60, measured 2026-08-01.

| Run | Slot | Compute Units | State Units | Pages Used | Events |
|---|---|---|---|---|---|
| 1 | 537635 | 4,763 | 0 | 1 | 0 |
| 2 | 537639 | 4,763 | 0 | 1 | 0 |
| 3 | 537642 | 4,763 | 0 | 1 | 0 |

Identical across runs, as deterministic execution requires. (An earlier
deployment of the same binary under a different seed also measured 4,763 —
the figure is a property of the binary, not the deployment. The canonical
address for this example is the one below.)

## Where does 4,763 CU go? (RESOLVED by examples 03 and 05)

```
  4,096   the one stack page the SDK entry stub maps before start() runs
    512   the entry stub's set_anonymous_segment_sz syscall (NOT tsdk_return —
          tsys_exit measures as free)
    155   instruction bytes processed
  -----
  4,763   exact
```

An earlier version of this section attributed the 512 to the `tsdk_return`
syscall. Two lines of evidence (2026-08-02) settle the attribution:

**1. Disassembly.** The 138-byte binary contains exactly three `ecall`
sites:

```
 3000044:  4881      li   a7,0        ; syscall 0 = set_anonymous_segment_sz
 3000046:  00000073  ecall            ; _start: maps ONE 4KB stack page
 ...
 3000058:  48ad      li   a7,11       ; exit — revert path, NOT executed
 300005c:  00000073  ecall
 ...
0000000003000074 <tsys_exit>:
 3000074:  48ad      li   a7,11       ; exit — executed via tsdk_return
 3000076:  00000073  ecall
```

The success path executes *both* syscall 0 and syscall 11. Summing the
executed instructions' encodings gives ~114 CU (+5 CU of data loads), so at
most **one** of the two executed syscalls can carry a 512 charge: both
charged would require negative instruction cost (4,763 − 4,096 − 1,024 < 114),
neither charged would require 667 CU of instructions where only ~119 execute.

**2. Which one: the same syscall was measured directly.** Example 05's
program calls `tsys_set_anonymous_segment_sz` explicitly, and its cost was
isolated as 512 + 28 CU (the grow delta reconciles to the unit). The entry
stub issues the identical syscall by the identical mechanism, so it is
charged — leaving 0 for `tsys_exit`.

The planned third check — fault immediately before exit and compare CU —
is blocked by a CLI limitation worth knowing: **failed transactions report
no consumed CU anywhere** (not in the `txn execute` error output, not in
`--json`, and they do not appear in `thru account transactions`), so the
fault variant's CU is unobservable. Within what is measurable, decomposition
(a) — entry-stub syscall 512 + stack page 4,096 + 155 instructions — is the
one supported by evidence; no observation supports charging `tsys_exit`.
[03-storage](../03-storage/README.md) and
[02-counter](../02-counter/README.md) established the account-data half of
the cost model (copy cost on first write; per-byte, not flat).

- **Binary size:** 138 bytes (`build/thruvm/bin/tn_example_01_empty_c.bin`)
- **Program account:** `taIjGXEaz6jCa8ORd1YWClEQgbxCdw-hDSpzGtYkZAXk-_`
- **Seed:** `example_01_empty`

## Reproduce

From the repo root, inside WSL2 Ubuntu:

```bash
export PATH="$HOME/thru-cli:$PATH"
export RISCV_TOOLCHAIN_ROOT="$HOME/.thru/sdk/toolchain"   # repo on /mnt/c, see gotcha
make                                                       # -> build/thruvm/bin/tn_example_01_empty_c.bin
thru --url https://rpc.alphanet.thru.org program create example_01_empty \
    ./build/thruvm/bin/tn_example_01_empty_c.bin
thru --url https://rpc.alphanet.thru.org txn execute --fee 0 \
    taIjGXEaz6jCa8ORd1YWClEQgbxCdw-hDSpzGtYkZAXk-_ ""
```

Empty instruction data (`""`) is accepted; no writable accounts are needed.
Or just run `./run.sh` from the repo root.

## Note on requested vs consumed units

`thru txn execute` sets `req_compute_units` from the `--compute-units` flag —
a fixed default, not an estimate. The actual default is **300,000,000**
(clap's `[default: 300000000]`), even though the same help text's prose says
"defaults to 1000000000". State and memory units both default to 10,000.
The table above reports units *consumed*, printed by the CLI after execution.

## Gotchas hit

- The SDK make system finds the RISC-V toolchain by walking **up** the
  directory tree from the project looking for `.thru/sdk/toolchain`. A repo on
  `/mnt/c` never reaches `~/.thru`, so the build dies with "RISC-V toolchain
  not found - reached root directory". Fix:
  `export RISCV_TOOLCHAIN_ROOT="$HOME/.thru/sdk/toolchain"`.
- `thru program create` on an already-used seed uploads a full temp buffer
  *before* noticing the program exists (manager error 0x0504), orphaning the
  buffer accounts. Probe with `thru getaccountinfo <program>` first.
