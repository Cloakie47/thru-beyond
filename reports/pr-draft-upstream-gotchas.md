# PR draft (NOT sent) — target: Unto-Labs/ai, thru-best-practices skill

## Title

thru-best-practices: add measured gotchas from on-chain cost measurements

## Description

This adds a set of measured, reproducible gotchas to thru-best-practices.
All of them come from deploying and executing instrumented C programs on
alphanet (thru CLI 0.3.2, 2026-08); every figure is from real CLI output,
three identical runs each, with methodology and raw numbers published at
github.com/Cloakie47/thru-beyond. The repo also packages these as a skill,
but the guidance belongs here, in the official skill, rather than in a
competing one — this PR contributes the content upstream.

Proposed additions, in priority order:

1. **Memory-model traps** (the two that break ported C code at runtime with
   no build-time diagnostic): the program image including .bss is mapped
   read-only, so writable globals/statics fault; and the entry stub maps
   exactly one 4KB stack page which never grows, so frames over ~4KB fault
   in the prologue. Workaround for the latter:
   `tsys_set_anonymous_segment_sz` (page-granular; 512 + 4,096 CU per page).
   (Filed as Unto-Labs/thru#37 and #38.)
2. **CPI correctness**: invoked programs receive instruction data in
   registers (a0/a1) — `tsdk_txn_get_instr_data()` reads the top-level
   transaction, so quickstart-pattern programs misparse every CPI. Declare
   `start(uchar const *data, ulong sz)`. Also: a callee `tsdk_revert`
   aborts the whole transaction (only syscall-level invoke errors return in
   `invoke_result`), and the practical depth limit measures 15.
3. **Cost intuitions that contradict common assumptions**: copy-on-write
   charges 1 CU per byte *present* in the touched page (an 8-byte account
   writes for +8, not +4,096); `Pages Used` counts uncharged event pages
   and must not be read as CU/4096; failed transactions report no consumed
   CU anywhere, so revert paths cannot be profiled; keep hot account fields
   within the first 2,047 bytes (12-bit immediate limit, measured +8 CU at
   offset 4096); denser codegen is literally cheaper (cost tracks encoding
   bytes).
4. **Workflow**: probe `getaccountinfo` before choosing `program create` vs
   `upgrade` — a failed create on an existing seed orphans its temp buffer;
   `txn make-state-proof` output should be captured to a file (SIGPIPE
   panic); `tsys_account_delete` requires data size 0 first.

Happy to reformat to match the skill's house style, split into separate
rules, or drop anything the maintainers consider out of scope. Everything
is independently reproducible from the linked repo's run.sh scripts.
