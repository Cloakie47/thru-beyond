# AUDIT — falsifiability review of every published finding

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

**Instruction cost = 1 CU per encoding byte (4 CU/32-bit, 2 CU/compressed).**
(1) 06: asm loop bodies with disassembled encodings, N up to 10,000.
(2) Any slope ≠ predicted bytes; in particular `spin_wide` (same 4
instructions, double bytes) costing the same as `spin` would have proven
count-based charging. (3) Yes — and the byte-based result was not
preordained: slopes measured 8.000/16.000/11.000/11.000 vs prediction, ratio
1.000. **Cost tracks bytes, not instruction count.** Every residual-based
figure in the repo rests on this rate; it now stands on direct measurement.

**Data loads cost 1 CU/byte on top of the instruction.** (1) 06 `spin_load`.
(2) Slope 10 (free loads) or 14 (4 CU/load). (3) Yes. Measured 11.

**Data stores cost 1 CU/byte.** (1) 06 `spin_store`. (2) Slope 10. (3) Yes.
Measured 11. Previously assumed free — this was wrong and closed most of the
noop residual gap (see Part 4 below).

**CoW on account write = 1 CU per byte copied, page granularity.**
(1) 02/03 originally; audit added increments on a 100 B and a 5,000 B
account with exact predictions written first. (2) 100 B ≠ 5,615 (per-byte)
or 5,000 B = 10,515 (whole-account copy) or 9,619+ (flat page). (3) Yes —
three distinguishable outcomes. Measured 5,615 and 9,611: per-byte, first
page only. This finding earned VERIFIED the honest way: its earlier flat-4,096
form was refuted by a reachable experiment.

**Resize growth = 1 CU per byte grown; shrink constant-CU.** (1) 02 ladder;
audit added non-page-multiple targets 100 (+92) and 5,000 (+4,992). (2) Any
step pattern or page rounding. (3) Yes. Exact at 7 transitions.

**Resize SU = ceil(bytes grown / 4096).** (1) The 02 ladder fit two formulas
(pages-grown vs target-size — refutation unreachable, flagged during audit);
the discriminator 4096→8192 was run: SU=1, not 2. (2) SU=2. (3) Now yes.
Shrink and no-op resizes: SU=0. Delete refunds nothing (SU=0, never negative).

**Syscall base = 512 per call, charged even for a no-op syscall.**
(1) Audit added `tsys_log(_, 0)`: +530 ≈ 512 + 18 dispatch over its intra-
binary baseline; `log8` adds exactly 8. Consistent direct measurements:
`set_anonymous_segment_sz` 512+28, `emit_event` 512+58+N. (2) A near-zero
cost for log0 would have shown 512 is an average of busy syscalls. (3) Yes.
**512 is a base, not an average; payload work is charged on top** (create's
~1,430 residual is payload, see UNVERIFIED). Scope: verified for log, emit,
set-segment, set-writable, resize, delete; other syscalls (transfer, invoke,
compress, decompress) untested.

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

**The 04 intercept decomposition** (6,129 = 512 + 4,096 + 602 + 919
instructions). The 919 was a residual; nobody counted 04's executed path.
The entry-stub 4,096 term is verified by disassembly of the *noop* binary;
04's binaries share the same SDK entry stub but were not themselves
disassembled. The intercept *value* is verified; its decomposition is not.

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
