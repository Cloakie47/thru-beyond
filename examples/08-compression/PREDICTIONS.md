# 08-compression — predictions, committed BEFORE measurement

Date: 2026-08-02. Spec read first: account-compression page (compressed
accounts leave validator storage; decompression needs the prior state +
proof; proofs variable-size via a 256-bit path_bitset, max depth 256),
state-proofs C reference (existing/updating/creation proof shapes), syscall
signatures (`tsys_account_compress(idx, proof, sz)`,
`tsys_account_decompress(idx, meta62, data, proof, sz)`).

Under the established model (512/syscall, 1 CU/byte processed, account
growth zero-filled at 1 CU/byte):

- **compress(S)** ≈ 4,608 floor + 512 + proof bytes + dispatch — plus an
  unknown hashing term: compressing must hash the S bytes of account data,
  so predict **≈ 5,3xx + proof + ~1 CU × S** (the hash term is the
  uncertain part; if compress does NOT scale with S, the hash is charged
  elsewhere).
- **decompress(S)** — the data rides as instruction data (charged 1 CU/B)
  and must be written back into the account (~1 CU/B) and hashed for
  verification (~1 CU/B?): predict **≈ 2–3 CU per byte revived** + proof +
  floor. Exact slope is the measurement.
- **Transaction limit**: 32 KiB per transaction. Decompression payload =
  6 args + 62 meta + 4 + S data + 4 + proof (~200–500 B) + transaction
  envelope (~300 B: signature, header, account list). Predict the maximum
  revivable S in one transaction ≈ **31,800–32,400 bytes**; **65,536 is
  impossible in a single transaction** — to verify empirically, and the
  actual failing size to be found by probing near the computed bound.
- **State-unit refund — the economic question**: delete refunds nothing
  (SU=0, measured in 02), so predict **compress also refunds nothing:
  SU = 0, never negative** → compression is a validator-storage mechanism,
  not an economic refund to the payer. If SU comes back negative, that
  refutes this and makes compression economically real for users.
- **Proof size vs tree depth**: path_bitset ⇒ proof size varies with
  position/occupancy; predict sizes in the 100–500 B range varying between
  accounts and between (creating / existing / updating) kinds, growing as
  more accounts populate the tree. Report every proof size measured.
- **Stale proof**: predict rejection with a syscall error surfaced through
  our 0x8100/0x8200 wrappers (not a VM fault).
- **modify** on a freshly decompressed account: an ordinary write — for
  S=5,000, page-0 copy ⇒ ≈ 9,6xx like example 02's `write_at(0)`.

Falsifiable core: SU refund yes/no; decompress slope in CU/byte; the
single-transaction revival ceiling; compress scaling (or not) with S.

Time-box note (per task): if the proof flow dead-ends, the failure point
gets reported precisely instead of worked around.
