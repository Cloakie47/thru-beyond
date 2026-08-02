# AUDIT — falsifiability review of every published finding

*Mapping note (2026-08-02 consistency pass):* the README ledger counts 53
labelled items (12 CONFIRMS / 5 CORRECTS / 25 UNDOCUMENTED / 11
UNVERIFIED); this file contains 34 finding rows. The mapping is complete
but not one-to-one — several rows cover multiple ledger items measured by
one experiment (e.g. the syscall-base row covers the base, the per-byte
payload charges, and log/emit behavior), and small observational items
(zero-length emits, .bss stored in the image) live inside their parent
experiment's row. Every ledger item traces to a row; two rows record
retractions of this repo's own earlier claims.

Date: 2026-08-02, thru CLI 0.3.2+54058649, alphanet. Trigger: example 02
falsified a rule example 03 had "confirmed," because 03's 8,192-byte account
could only ever copy full pages — the refuting result was unreachable by
design. Every finding below is examined for the same failure mode. Three
questions each: (1) what experiment produced it, (2) what result would have
refuted it, (3) was that refuting result reachable?

New discriminating experiments run for this audit: direct instruction
measurement (example 06), CoW on 100 B and 5,000 B accounts, partial-page
segment growth, the resize-SU formula, a near-empty syscall (`tsys_log(_,0)`),
and a third create-proof size.

## VERIFIED

**Instruction cost = 1 CU per encoding byte (4 CU/32-bit, 2 CU/compressed).
CONFIRMS SPEC** — the resources spec states the core rule ("1 CU per byte of
instruction or data processed") and both rates; the "bytes, not count"
framing this repo briefly presented as a discovery is that rule restated.
(1) 06: asm loop bodies with disassembled encodings, N up to 10,000.
(2) Any slope ≠ predicted bytes; in particular `spin_wide` (same 4
instructions, double bytes) costing the same as `spin` would have proven
count-based charging — and *refuted the spec*. (3) Yes. Slopes measured
8.000/16.000/11.000/11.000 vs prediction, ratio 1.000. Every residual-based
figure in the repo rests on this rate; it now stands on measurement as well
as documentation.

**Data loads cost 1 CU/byte on top of the instruction. CONFIRMS SPEC**
(lb 1 / lh 2 / lw 4 / ld 8 are listed explicitly). (1) 06 `spin_load`.
(2) Slope 10 (free loads) or 14 (4 CU/load). (3) Yes. Measured 11.

**Data stores cost per width. CONFIRMS SPEC — and corrects this repo.**
The spec documents store costs alongside loads (sb 1 / sh 2 / sw 4 / sd 8,
with a worked `sd` = 4 + 8 = 12 CU example). This repo assumed stores were
free, which is what opened the "36 CU gap" — the error was an unread spec
page, not undocumented VM behavior. (1) 06 `spin_store`. (2) Slope 10.
(3) Yes. Measured 11 (1.000 vs spec).

**CoW on account write = 1 CU per byte *present in the copied page*
(min(4,096, bytes in page)). CORRECTS SPEC** — resources.md says "each page
fault costs exactly 4,096 compute units"; measurement says the charge is the
bytes actually copied. (1) 02/03 originally; audit added increments on a
100 B and a 5,000 B account; the trailing-page experiment (predictions
committed at `d74af38` before measurement) wrote single bytes at offsets 0,
4096, and 4999 of the 5,000-byte account. (2) Offset-4096 costing W0
(= flat page) vs W0 − 3,192 (= bytes present); both outcomes stated in
advance. (3) Yes. Measured: `write_at(0)` = 9,656, `write_at(4096)` =
`write_at(4999)` = 6,464 = W0 − 3,192 exactly — **bytes present (904)**, and
`write_two(4096)` = 10,567 = W0 + 904 + 7: the two pages' copies add, total
5,000 = the whole account. Verified at copy sizes 8, 92, 100, 904, 4,096.
This finding earned its status the honest way: its earlier flat-4,096 form
was refuted by a reachable experiment, twice refined since.

**Resize growth = 1 CU per byte grown; shrink constant-CU.** (1) 02 ladder;
audit added non-page-multiple targets 100 (+92) and 5,000 (+4,992). (2) Any
step pattern or page rounding. (3) Yes. Exact at 7 transitions.

**Resize SU = ceil(bytes grown / 4096).** (1) The 02 ladder fit two formulas
(pages-grown vs target-size — refutation unreachable, flagged during audit);
the discriminator 4096→8192 was run: SU=1, not 2. (2) SU=2. (3) Now yes.
Shrink and no-op resizes: SU=0. Delete refunds nothing (SU=0, never negative).

**Syscall base = 512 per call, charged even for a no-op syscall.
CONFIRMS SPEC** — the resources spec already states a 512 base with
additional per-syscall costs on top; the repo's "base, not average" phrasing
was a restatement, not a finding. (1) Audit added `tsys_log(_, 0)`: +530 ≈
512 + 18 dispatch over its intra-binary baseline; `log8` adds exactly 8.
Consistent direct measurements: `set_anonymous_segment_sz` 512+28,
`emit_event` 512+58+N. (2) A near-zero cost for log0 would have refuted the
documented base. (3) Yes. Create's ~1,430 residual is per-call payload work
(see UNVERIFIED). Scope: verified for log, emit, set-segment, set-writable,
resize, delete; other syscalls (transfer, invoke, compress, decompress)
untested. The one **spec correction** here: `tsys_exit` measures 0 despite
the blanket 512 base.

**`tsys_exit` costs 0.** (1) Disassembly of noop (3 ecall sites; success path
executes syscalls 0 and 11) + executed-byte count proving at most one 512 fits
+ the direct 512+28 measurement of the same syscall 0 from program code.
(2) An instruction count leaving room for both syscalls, or set_seg measuring
below 512. (3) Yes. Caveat: the behavioral fault-before-exit test is
impossible — the CLI reports no CU for failed transactions.

**Anonymous segment allocation costs 4,096 per page at map time.** (1) 05
grow 1→8 pages (7×4,096+512+28 exact); 06 grow by exactly one page (+4,096).
(2) Cost at first touch instead (05 `touch_stack` showed ~15 CU/page touched)
or free mapping. (3) Yes.

**Event pages counted in `Pages Used`, never CU-charged; 10-byte per-event
record overhead.** (1) 05 emit series (CU = 570+N exact through two page
boundaries while Pages steps), boundary bisect (4086/4087), two-event probe
(4076/4078). (2) A ~4,096 CU step at a boundary; a one-event-only overhead
would have left the two-event boundary at 4086. (3) Yes. Scope caveat: this
verifies the runtime's event buffer as observed; whether the runtime
pre-provisions that buffer and charges no one is not observable from the CLI.

**Zknh SHA-256 speedup 1.30×; slopes 12,846.0 / 9,884.0 CU per block;
common intercept 6,129.** (1) 04 with predictions committed pre-measurement;
slopes re-derived pair-wise; arm B @ 256 B re-run hit the corrected fit
exactly. (2) Non-linear scaling, differing intercepts, or the re-run missing
55,549. (3) Yes.

**Create cost varies with proof size at 1 CU/proof byte.** (1) 02: proofs of
168, 200 (twice), and — added by this audit — 264 bytes: 7,759 / 7,791 /
7,855. ΔCU = Δproof exactly, three distinct sizes. (2) Constant cost or
non-unit slope. (3) Yes.

## Example 07 (CPI) — added 2026-08-02, predictions committed at `81b3c04`

**Per-hop and per-depth CPI cost (1,511 same-depth; 5,607 = 1,511 + 4,096
per depth level). UNDOCUMENTED.** (1) `cpi_n` N=1,2,4,8 and `cpi_deep`
N=1..14, three runs each, exact slopes at every pair, Pages flat (breadth)
vs 2+N (depth). (2) Any nonlinearity, a 4,096 step per sequential hop
(H-B), or no step per depth (H-A) — three pre-stated hypotheses, mutually
exclusive. (3) Yes; H-C won.

**Frame page reused across sequential same-depth invokes. UNDOCUMENTED.**
(1) `cpi_n` slope 1,511 with Pages flat vs first-hop 5,567. (2) Slope ≈
5,600 with Pages growing would have meant freed-on-return. (3) Yes.

**Call-depth limit: 16 call depths (1..16). CONFIRMS the SDK header** —
after correcting this audit's own earlier row, which claimed "max 15,
CORRECTS" based on a recursion miscount (`cpi_deep(N)` spawns N+1 callee
frames; the depth-0 countdown still executes as a frame). (1) `cpi_deep`
N=14 → deepest call depth 16, succeeds; N=15 (would need depth 17) fails
with our 0x7C00 + (−24) wrapper. `Pages Used` = deepest depth (N=14 → 16)
independently confirms the frame count. (2) N=14 failing, or Pages ≠
depth. (3) Yes. Lesson logged: the refutation of the wrong claim was
reachable all along — in the Pages column of the same table.

**`Pages Used` under CPI = deepest call depth reached. UNDOCUMENTED.**
(1) The unified table across no_cpi / cpi_1 / cpi_n / cpi_deep: 1 / 2 /
2-flat / N+2, all consistent with one stack page per depth, reused at the
same depth. (2) Any row breaking the identity — `cpi_n` Pages growing, or
deep Pages ≠ N+2. (3) Yes.

**The 40 CU difference between the first hop (5,567) and the per-depth
slope (5,607): explained by mechanism, not closed to the CU.** (1)
Disassembly of both binaries: the deep marginal frame runs the callee's
recursion path (extra u32 parse ≈ 28 CU, args-struct stores, two
return-code checks) vs the cpi_1 path (caller helper + shorter op-0
terminal). (2) Identical code paths would have left 40 CU genuinely
unexplained. (3) Partially — the mechanism is established, the exact sum
was not performed (hand-counts carry ±4 CU). Flagged in the README.

**A callee's `tsdk_revert` aborts the whole transaction; the caller cannot
catch it. CORRECTS the C reference's implication** that `invoke_err` lets
callers branch on callee failure. (1) `cpi_revert` absorbs both codes and
emits them — the transaction still failed with the callee's 0x7BAD and no
event. (2) A successful transaction with the emitted codes. (3) Yes.
Nuance: *syscall-level* invoke errors (e.g. −24) DO return in
`invoke_result` and are catchable.

**Account indices are transaction-global under CPI. CONFIRMS SPEC**
("same transaction context"). (1) CPI callee read the u64 at index 2 and
emitted 3 — account d's known counter. (2) A wrong value, NULL pointer, or
fault. (3) Yes.

**CPI instruction data via registers (a0/a1). CONFIRMS SPEC (invoke page
documents it) — but the SDK accessor trap is UNDOCUMENTED:**
`tsdk_txn_get_instr_data()` reads the top-level transaction, so
quickstart-pattern programs (ex02 included) reject or misparse CPI data.
(1) First-draft callee failed every CPI on its own size check with the
caller's 12-byte payload; register-arg rewrite fixed it. (2) The txn
accessors returning per-frame data. (3) Yes.

**Invoke costs exactly its 512 base — no register-save surcharge. CONFIRMS
SPEC (resolved 2026-08-02 by the discriminating experiment).** (1) Exact
disassembly count of the full marginal `cpi_n` path: caller loop 36 +
helper 80 + tsys_invoke wrapper 168 + callee stub 109 + callee dispatch/
return/exit 86 + 1,024 syscall bases = 1,503 vs measured 1,511 — residual
8 CU. (2) A residual near 256 would have confirmed the surcharge; 8 kills
it. (3) Yes — both outcomes were stated in advance with thresholds (~230
vs ~490 for the non-syscall residual; counted 479, i.e. the "kill" branch).
The earlier surcharge reading was an artifact of estimating the callee path
alone (~230) while the caller-side helper + wrapper actually cost 284/hop.

## Example 08 (compression) — added 2026-08-02, predictions at `a21831a`

**Compression = 5,853 + 1 CU per account byte + 1 CU per proof byte; SU = 0
at every size (no refund). UNDOCUMENTED.** (1) CLI `account compress` on
8 / 1,000 / 5,000 / 65,536-byte accounts plus a 100-byte out-of-sample
repro; subtracting the two already-established per-byte terms leaves
exactly 5,853 at all five points. (2) Any nonzero residual variance, or
negative SU (a real refund — explicitly reachable, would have made
compression economic for payers). (3) Yes. **Compression is a
validator-storage mechanism, not a payer refund.** Caveats: the proof-byte
term uses the same-key creating-proof size (identical trie path ⇒ identical
size; the CLI does not print the proof it embeds) — flagged UNVERIFIED as
an inference, though five exact points leave little room. *Correction
note:* the first published form ("≈ 6,053 + 1 CU/B, slope 1.0005")
absorbed proof bytes into the fit — the repo failed to apply its own ex02
law before fitting. Corrected everywhere.

**Compression is a system-level operation: fee-payer-signed, on a
program-owned account, without the owner program. UNDOCUMENTED.** (1) The
CLI compress transaction succeeded with only the `default` key signing.
(2) A rejection requiring owner-program involvement. (3) Yes.

**RETRACTED, then corrected (discrimination session): "decompression
impossible" was wrong — the defect is a ~1–5 minute indexing lag.
UNDOCUMENTED.** The original row claimed CORRECTS based on retries that
never exceeded ~60 s; timed retries on a fresh account bracketed the truth:
T+1 min all surfaces fail with "bintrie: key not found", T+5 min all
succeed, and every previously "lost" account then decompressed. (1) Timed
probe at T+0/T+1/T+5 (T+15 moot once T+5 succeeded), plus Explorer MCP
cross-check (get_account NOT_FOUND during the lag while get_transaction
fully indexes the compress — executed by the system program `taAAAA…EB`).
(2) The refuting result for the original claim was a retry past the lag —
reachable all along, and this audit's own standard should have caught that
the retry window was never varied. (3) Yes, eventually. Meta-lesson logged
as a gotcha: retry windows must exceed plausible indexing lag before
declaring impossibility.

**The completed round trip (decompress ≈ base + ~1 CU/byte revived, SU =
pages restored; 65,536 B revived via the CLI's chunked buffer flow; modify
5,565 ×3; recompress = the compress formula exactly). UNDOCUMENTED.**
(1) Four decompressions (6,211 / 7,079 / 11,111 / 72,037-final-txn at
100 / 1,000 / 5,000 / 65,536 B), three identical modifies, one recompress
(6,153 = 5,853 + 100 + 200 — a sixth exact point for the compression
formula). (2) Non-linear revival cost, or recompress diverging from the
compress formula. (3) Yes. One-shot ops can't repeat 3×; cross-checked via
multiple sizes and the formula identity instead. The single-transaction
revival ceiling (~32,100–32,300 B) remains UNVERIFIED (unprobed — the CLI
auto-switches to the chunked flow).

**A 90-second-old creating proof is accepted. Refutes this repo's own
committed prediction** (rejection). (1) Fetch proof, sleep 90 s, create —
succeeded at the standard CU. (2) The predicted rejection. (3) Yes. The
tolerance boundary is untested (UNVERIFIED beyond 90 s).

**`tsys_account_compress` from a program: UNRESOLVED.** Both obtainable
proof kinds revert with syscall error −43; the expected proof shape is
undocumented and the CLI constructs its own. Two attempts, then time-boxed
out. Not a finding — a precisely-located blocker.

## Example 11 (budgets) — added 2026-08-02, predictions at `96efd57`

**Consumed MU ≡ `Pages Used`, in every transaction. UNDOCUMENTED.**
(1) 28 transactions swept via the Explorer's `memoryUnits.consumed`
(fresh probes across 01/03/04/05/06/07 + the resize ladder + historical 08
signatures): identical in 28/28, including the pre-stated deciding case —
event pages (CU-free per ex05) ARE MU-charged (emit8 → MU 9,
emit4088 → 10). (2) Any single divergence, most plausibly the event pages.
(3) Yes. Within-transaction peak-vs-end-state remains UNVERIFIED (no
deployed instruction grows then shrinks; deploys blocked by the desync).

**SU = ceil(bytes grown / 4096), sharpened at the 4097 discriminator;
never refunded. UNDOCUMENTED (confirming and extending the earlier row).**
(1) Ladder from 8 to {1, 8, 4095, 4096, 4097, 8192, 65536}: SU =
{0,1,1,1,**1**,2,16} — target 4097 spans two pages yet SU=1, killing the
target-pages reading a second, sharper time. CU stayed exactly 1/byte
grown throughout. (2) SU=2 at 4097. (3) Yes.

**The SDK's default -O3 emits the largest binary of six levels for the
SHA-256 example (3,496 B vs 1,216 at -O1). UNDOCUMENTED, sizes only.**
(1) Direct compiles with the SDK's exact flag set, trailing -O override.
(2) -O3 being smallest/typical. (3) Yes for sizes; the per-transaction CU
comparison — the half that decides the recommendation — is gated on the
proof-root desync (deploys fail with −23) and explicitly NOT claimed.

## UNVERIFIED (flagged in README until a discriminating experiment exists)

**"Every page charge is 1 CU per byte zero-filled" as applied to anonymous
segments.** The unification is clean for account ops (measured at byte
granularity), but `set_anonymous_segment_sz` **rejects sizes that are not
page multiples** (n=8, 100, 2048 all error — discovered by this audit's
attempt to test exactly this). The refuting result is unreachable below page
granularity: for anonymous memory, "4,096 per page" and "1 CU per zero-filled
byte" are observationally identical. Demoted to interpretation.

**The ~1,430 CU create-syscall residual attribution** (seed/PDA hashing,
proof verification). Plausible, never isolated: no experiment varies seed
size or proof content independently of proof length. Would need e.g. two
proofs of identical length with different verification depth.

**The 04 intercept decomposition** (6,129 = 4,096 + 512 + 512 + 1,009,
where 1,009 = instruction bytes + load/store bytes + 32 event payload
bytes). The 1,009 is a residual; nobody counted 04's executed path. (An
earlier revision split it as "602 + 919" under a different bundling of the
emit call — same total; the repo now uses one convention.) The entry-stub
4,096 term is verified by disassembly of the *noop* binary; 04's binaries
share the same SDK entry stub but were not themselves disassembled. The
intercept *value* is verified; its decomposition is not.

**Docs-figure reconciliation (quickstart 7,524 create / 5,980 increment).**
See out-of-sample section below: residuals of +171 and +85 CU remain after
adjusting for proof size and event emission. Attributing them to "SDK
version / program shape" is plausible but unverifiable without the
quickstart's exact binary. Flagged.

**Interpretive details, flagged as speculation:** the 10-byte event overhead
being "8-byte type + u16 length"; deployment finalize (188,080 CU)
"plausibly hashing the image"; the `user_error` register-echo pattern on
faults.

**Deployment cost 320,292 CU** is a single sample for one 838-byte binary —
valid as a data point, unverified as a function of binary size (only one size
measured).

## Out-of-sample validation (Part 3)

The quickstart shows a 104-byte state proof with create = 7,524 CU and
increment = 5,980 CU. Model prediction from our measurements: create =
7,791 − (200−104) × 1 CU = **7,695** vs docs **7,524** — error **+171 CU
(+2.27%)**. Increment (their shape emits an event, so compare `increment_e`):
**6,065** vs **5,980** — error **+85 CU (+1.42%)**. Both residuals are the
size of program-shape/codegen differences repeatedly observed between
binaries (±30–150 CU), and the docs predate CLI 0.3.2 — but with their
binary unavailable, the residual is **unexplained**, not explained. The
model gets within ~2% out-of-sample; it does not nail it.

## The 36 CU gap (Part 4)

01-noop residual: 155 CU. Executed-path count from disassembly: 114 CU of
instruction bytes + 5 CU of loads (lbu 1 + lhu 2 + lhu 2). The missing 36:
**32 CU are the four 8-byte `sd` stores** on the entry/exit path — store
bytes are charged (measured directly in 06, previously assumed free) —
leaving **4 CU unexplained** (~one instruction; within hand-count error, but
not accounted for). A cross-check hand-count of 06's longer dispatch path
leaves a similar single-digit residual. The entry stub requests exactly
4,096 bytes (`lui t4,0x1`; `sub a0,t3,t4` — disassembly), so the page term
is exact. Verdict: **closed from 36 to 4; the final 4 CU is unexplained**
and flagged in the README.
