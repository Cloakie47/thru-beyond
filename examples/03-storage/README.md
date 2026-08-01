# 03-storage — what does touching a second page actually cost?

**Hypothesis under test** (from [01-noop](../01-noop/README.md)): the documented
page-fault cost is 4,096 CU, so touching one additional 4KB page of account
data should add ~4,096 CU and increment `Pages Used` by exactly 1.

**Verdict: confirmed for writes — killed for reads.** Writing to a second page
adds ~4,100 CU and exactly +1 page. *Reading* a never-touched page adds no
4,096 CU charge and does not increment `Pages Used` at all. The page charge is
a copy-on-write cost, not a general access cost.

## Setup

One program, one 8,192-byte account (two full 4KB pages), five instructions.
The three write handlers are structurally identical — same dispatch, one
`tsys_set_account_data_writable` call each (so its 512 CU cancels out of every
delta), volatile single-byte stores — differing only in which offsets they touch.
Single-byte accesses only: a single access spanning a 4KB page boundary faults.

| # | Instruction | Touches |
|---|---|---|
| 0 | `init` | create account with state proof, resize to 8192 |
| 1 | `write_p0` | write 1 byte at offset 0 |
| 2 | `write_p0_x2` | write 1 byte at offset 0 and 1 byte at offset 100 (both page 0) |
| 3 | `write_p0_p1` | write 1 byte at offset 0 and 1 byte at offset 4096 (one per page) |
| 4 | `read_p1` | read 1 byte at offset 4096, discard; no write, no writable call |

## Measurements

Alphanet (`https://rpc.alphanet.thru.org`), thru CLI 0.3.2+54058649,
node 0.0.0-local+599daf60, 2026-08-01. Three runs per instruction, `--fee 0`,
account passed as `--readwrite-accounts` for all instructions so the
transaction shape is constant. All three runs of every instruction were
identical (12/12 deterministic).

| Instruction | CU | SU | Pages Used | Events | Slots |
|---|---|---|---|---|---|
| `init` (one-time) | 15,913 | 2 | 3 | 0 | 538222 |
| `write_p0` | 9,582 | 0 | 2 | 0 | 538269, 538277, 538284 |
| `write_p0_x2` | 9,555 | 0 | 2 | 0 | 538292, 538300, 538306 |
| `write_p0_p1` | 13,659 | 0 | 3 | 0 | 538313, 538317, 538326 |
| `read_p1` | 4,892 | 0 | 1 | 0 | 538329, 538335, 538346 |

Reference: [01-noop](../01-noop/README.md) baseline = 4,763 CU, 1 page.
The CLI does not print Memory Units consumed (only requested — see gotchas).

## The four deltas

**A. `write_p0_x2` − `write_p0` = −27 CU, +0 pages.**
A second write to the same page has no page cost. The *negative* sign is a
finding in itself: the handler that does strictly more work costs 27 CU less.
CU charges per byte of instruction processed, so compiler code layout
(compressed vs. full-width instructions, branch ordering) can outweigh an extra
1-byte store. Small CU differences between similar programs are noise, not signal.

**B. `write_p0_p1` − `write_p0` = +4,077 CU, +1 page (exactly).**
Hypothesis said ~4,096: confirmed. The even cleaner comparison is
`write_p0_p1` − `write_p0_x2` — identical two-store shape, only the second
offset differs — which gives **+4,104 = 4,096 + 8**, the 8 being the extra
addressing instructions for offset 4096 (doesn't fit a 12-bit immediate).
`Pages Used` went 2 → 3, exactly +1.

**C. `read_p1` − `write_p0` = −4,690 CU, −1 page.**
The read variant drops the writable syscall (−512), the store, and — the key
observation — the entire page charge. 4,892 CU is barely above the 01-noop
baseline (+129), despite loading a byte from a page nothing ever wrote.
Reads do not fault-charge and do not count toward `Pages Used`.

**D. `write_p0` − 01-noop = 9,582 − 4,763 = +4,819 CU, +1 page.**
Decomposes cleanly as 512 (one `tsys_set_account_data_writable` syscall)
+ 4,096 (copy-on-write fault on first write to the account page)
+ 211 (dispatch, get-data-pointer, store). This supports the 01-noop
decomposition hypothesis.

**Interpretation:** `Pages Used` counts *written* (copy-on-write) pages, and
the 4,096 CU charge fires on the first write to a page — never on reads.
The docs' "page fault = 4,096 CU" needs the qualifier "on write". For cost
optimization this inverts the usual advice: spreading *reads* across pages is
free; spreading *writes* across pages costs 4,096 CU each.

## Reproduce

From the repo root, inside WSL2 Ubuntu (see `run.sh` in this directory):

```bash
export PATH="$HOME/thru-cli:$PATH"
export RISCV_TOOLCHAIN_ROOT="$HOME/.thru/sdk/toolchain"
U="--url https://rpc.alphanet.thru.org"
make
thru $U program create example_03_storage ./build/thruvm/bin/tn_example_03_storage_c.bin
# -> program tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1
thru $U program derive-address tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1 ex03_storage_acc
# -> account taCr3SBDQu395xbvc0eG6odoWC6uhwJEp5O1kgMOwqlJoi
thru $U txn make-state-proof creating taCr3SBDQu395xbvc0eG6odoWC6uhwJEp5O1kgMOwqlJoi
# init instruction = 00000000 + 0200 + <seed hex 32B> + <proof size, LE uint32> + <proof hex>
thru $U txn execute --fee 0 --readwrite-accounts taCr3SBDQu395xbvc0eG6odoWC6uhwJEp5O1kgMOwqlJoi \
    tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1 <init hex>
# then the four ops, hex 010000000200 / 020000000200 / 030000000200 / 040000000200
```

- **Binary size:** 838 bytes
- **Program account:** `tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1` (seed `example_03_storage`)
- **Storage account:** `taCr3SBDQu395xbvc0eG6odoWC6uhwJEp5O1kgMOwqlJoi` (seed `ex03_storage_acc`)

## What deployment itself cost

Captured from the five transactions `thru program create` ran for this 838-byte
binary (via `thru txn get`; same CLI/network/date as above). Nobody publishes this:

| Step | Slot | CU | SU | Pages | Requested CU |
|---|---|---|---|---|---|
| Create meta + buffer accounts | 538149 | 12,489 | 1 | 3 | 51,676 |
| Write chunk (1 × 838 B) | 538151 | 38,574 | 0 | 2 | 500,000,000 |
| Finalize upload | 538153 | 188,080 | 0 | 2 | 217,600 |
| Create managed program | 538159 | 45,367 | 1 | 3 | 500,000,000 |
| Cleanup temp buffer | 538162 | 35,782 | 0 | 1 | 50,000 |
| **Total** | | **320,292** | **2** | | |

Finalize dominates (~59%) — plausibly hashing/verifying the uploaded image.
Note the CLI never prints the chunk-write signature; it was recovered via
`thru account transactions` on the temporary buffer account.

## Gotchas hit

- `thru txn make-state-proof` panics ("failed printing to stdout: Broken pipe")
  if its stdout closes early — pipe to `head` and you get a Rust panic instead
  of output. Capture to a file first.
- `Requested Memory Units` appears in `thru txn get`, but *consumed* memory
  units are not reported anywhere in the CLI output.
