# 09-limits — predictions, committed BEFORE measurement

Date: 2026-08-02. Spec read first (`/spec/core/transactions.md`): max
transaction size 32 KiB; at most 1,024 accounts referenced; addresses are
exactly 32 bytes; writable and read-only lists each sorted ascending, no
duplicates; the signature covers header + account addresses + instruction
data + optional proofs.

**The headline arithmetic the spec invites but does not state: 1,024
accounts × 32 bytes = 32,768 bytes = the entire transaction budget.** The
two loudest limits collide. Predictions follow from that.

## Part A — per-account cost (touch_none / read_all / write_all)

- **Declared-but-untouched account: ~32 CU each** (point prediction). The
  address is 32 bytes of transaction data; under the 1-CU-per-byte core
  rule the runtime's handling of each entry should bill about its size.
  Alternative outcome A0: **0 CU** — account-list validation happens
  outside the metered VM (like consensus-side signature checks) and
  declaring accounts is free. These differ decisively at N=1,000
  (~32,000 CU vs 0); no third value predicted.
- **Linear, not superlinear.** Sorting is validator-side validation of an
  already-sorted list (verifying sortedness is O(n)); no O(n log n) CU
  term should appear. Refutation: slope growing with N.
- `read_all` − `touch_none`: **~30–40 CU per account** (get-ptr ~6
  instructions + 8-byte load + loop overhead).
- `write_all` − `touch_none`: **~545–565 CU per account** (512 per-account
  `set_account_data_writable` + 8-byte CoW copy of each 8-byte account +
  ~25 instructions). Pages Used = 1 + N (one written page per account);
  MU = N (CoW pages) — watch the 10,000 default MU budget near N=1,000.
- **The failure boundary is the SIZE limit, not the account limit.**
  Envelope ≈ 3×32 addresses + 64 signature + ~100–140 header + 8
  instruction bytes ≈ 270–300 B, so declared accounts should stop fitting
  at **N ≈ 1,014–1,016** (N×32 + envelope > 32,768) — BELOW the count
  limit of 1,022 (1,024 minus fee payer and program). N = 1,023/1,024/1,025
  all fail. Whether the error is CLI-side or chain-side, and whether it
  names size or count, is measured not predicted.

## Part B — instruction data (echo_n / carry_n)

- `carry_n` (payload never read): slope **1 CU per byte** — the core rule
  applied to instruction data at transaction level. Refutation: slope 0
  (instruction data free until read).
- `echo_n` (loop-read): slope ≈ carry + 11 (the measured 06 loop cost) ≈
  **~12 CU/byte**.
- Rejection at instruction data ≈ **32,768 − envelope ≈ ~32,4xx bytes**;
  32 KiB exactly should already fail.

## Part C — the 1,000-recipient payout

- Uses `tsys_account_transfer` (syscall 0x03, previously unexercised).
  Authorization risk flagged: whether a program may debit a program-owned
  treasury PDA is untested; if transfer requires different authority the
  payout design fails and that is the finding.
- Per recipient ≈ **512 (transfer base) + ~20 instructions**; 1,000
  recipients ≈ 4,608 floor + ~535,000 ≈ **~540k CU** — well inside the
  300M budget, so CU is NOT the binding constraint.
- The binding constraint is transaction size: recipients ≈
  (32,768 − envelope − 32 (treasury) − instr) / 32 → **max ~1,010
  recipients**, i.e. a full 1,000-recipient payout FITS in one
  transaction with ~10 addresses of headroom. If the envelope is bigger
  than estimated, the max drops a few; report the measured maximum.
