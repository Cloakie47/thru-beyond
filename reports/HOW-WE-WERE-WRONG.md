# How we were wrong — four failure modes from a measurement project

This repo spent several days measuring the true costs of a blockchain VM.
It published four kinds of wrong result along the way, each caught and
corrected by its own process. The failure modes are not blockchain
failures; they are measurement failures, and they generalize to any
engineering work that turns observations into claims.

## 1. The experiment that couldn't say no

**The case.** To test the documented rule "a copy-on-write page fault
costs 4,096 compute units," we wrote one byte into an account and measured
exactly +4,096 per page touched. Confirmed — or so we published. The
account we used was 8,192 bytes: two *full* pages. Weeks of arithmetic
later, a sequel experiment wrote to an 8-byte account and measured +8, not
+4,096. The real rule was "1 unit per byte actually copied," and 4,096 was
the special case where a full page gets copied. Our original experiment
could not have produced any number *other* than 4,096 — every page it
could touch was full.

**The root cause.** The experiment's design space did not contain the
refuting outcome. It wasn't that we ignored contrary evidence; contrary
evidence was unreachable.

**The rule adopted.** For every finding, write down what result would have
refuted it — and then check whether that result was *reachable* by the
experiment as designed. If it wasn't, the finding is unverified no matter
how clean the data looks. (This audit question later killed a second
claim of ours — a call-depth limit "correction" that was actually an
off-by-one in our own recursion accounting, reachable in the very Pages
column of our own results table.)

## 2. Insufficient patience, filed as a defect

**The case.** After compressing accounts, every attempt to decompress them
failed with a fatal-looking error ("bintrie: key not found"). We retried
for about 60 seconds, concluded that compressed accounts were
unrecoverable — "one-way black holes" — sacrificed test accounts to the
claim, and published it. A later session ran timed retries: still failing
at one minute, fully working at five. The service indexes new entries with
a one-to-five-minute lag, and the error message for "not yet indexed" is
identical to "does not exist."

**The root cause.** We chose a retry window sized to our patience, not to
the system's actual propagation time, and the error text encouraged the
misreading. Nothing distinguished "wait longer" from "give up" — so we
gave up, and called it a finding.

**The rule adopted.** Before declaring an operation impossible, retries
must exceed every plausible propagation delay in the system — and the
retry schedule belongs in the published evidence. Distrust any conclusion
whose only support is "we tried for a while." (For system builders, the
mirror rule: transient states deserve distinguishable errors. "Not yet"
and "never" should not share a message.)

## 3. The filtered view, mistaken for the world

**The case.** We claimed — and filed upstream issues stating — that the
CLI reports consumed memory units nowhere, and that failed transactions
report no cost figures at all. Both claims were false. The CLI's
transaction-inspection command had printed a `Memory Units Consumed` line
all along, for successful and failed transactions alike. Our tooling
never showed it to us: every observation ran through grep filters that
matched compute, state, and page fields — and silently dropped everything
else. We had mistaken our filters' output for the command's output.

**The root cause.** Automation that narrows what you see becomes
invisible itself. After enough sessions, "what my script prints" and
"what the tool prints" felt like the same thing. They weren't.

**The rule adopted.** Before claiming that data is unavailable, enumerate
the surfaces — every subcommand, every output format, every independent
API — and inspect at least one raw, unfiltered output from each. Scope
the written claim to the surfaces actually checked. And when a claim is
about *absence*, treat your own tooling as the first suspect.

## 4. The constant absorbed into the slope — three times

**The case.** Three separate published cost laws quoted a per-byte rate
that silently swallowed a large fixed term. The compression law's first
fit folded proof bytes and part of the intercept into a "1.0005 CU/byte"
slope; the decompression law repeated the identical mistake days after
the first was corrected; and the program-upgrade figure was published as
"≈170–195 CU per binary byte" when the data is actually a line with a
137,149-CU intercept — no single data point reproduces from the quoted
rate (345,340 CU for 1,216 bytes is 284 "CU/byte"). Each law looked
plausible because the sampled sizes were large enough that the constant
hid inside the rate's scatter.

**The root cause.** Fitting a fresh law from scratch instead of first
subtracting the terms already established, and publishing a rate without
its intercept. Correcting one instance didn't fix the habit: the same
error shipped twice more because each fit was done locally, and nobody
grepped the repo for sibling figures fitted the same way.

**The rule adopted.** Apply established laws first and fit only the
residual; never publish a per-X rate without stating the intercept next
to it; and when a fitting error is corrected once, sweep every other
published fit for the same shape — the second and third occurrences were
found exactly that way, and the verifier now recomputes each fit from
the raw points.

## The common thread

All four errors produced clean, reproducible, internally consistent
numbers. Reproducibility is not correctness: each wrong claim replicated
perfectly, because the flaw was upstream of the measurement — in the
design, the schedule, or the lens. The fixes that worked were all
structural: predictions committed before measuring, an audit row per
finding stating its reachable refutation, and a verifier that recomputes
every published number from raw records. None of these prevent being
wrong. They make being wrong loud, cheap, and short-lived.
