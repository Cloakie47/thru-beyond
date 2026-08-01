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

## Where does 4,763 CU go? (HYPOTHESIS)

This decomposition is a hypothesis, not a measurement:

```
  4,096   one page fault (the single page in "Pages Used: 1")
    512   one syscall (program termination via tsdk_return)
    155   actual instruction bytes processed
  -----
  4,763
```

[03-storage](../03-storage/README.md) tested the page-fault term: writing to
one additional page adds ~4,100 CU and exactly +1 `Pages Used` (consistent
with 4,096 + addressing overhead), and the write-path delta over this baseline
decomposes as 512 + 4,096 + instructions. Both support the numbers above.
One refinement from 03: the 4,096 charge fires on *written* (copy-on-write)
pages only — reads of untouched pages cost no page fault and don't increment
`Pages Used` — so the page term here is presumably a written page (e.g. stack),
not code.

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
