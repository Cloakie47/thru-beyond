# Compressed leaves are unqueryable for ~1–5 minutes, with a fatal-looking error — and the compress syscall's proof shape is undocumented

(NOT FILED — being taken to the team directly.)

## Environment

- thru CLI 0.3.2+54058649, alphanet (`https://rpc.alphanet.thru.org`),
  node 0.0.0-local+599daf60, 2026-08-02

## Timeline of the repro (all identifiers real)

Account `tarvoZu7fh587rXrOTtW5bM_zCUY1UdULUJa50B0jAi0it` (100 B data,
owned by program `tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY`).

1. `thru account compress <pda>` → success. Signature
   `tsub2BBaYbgVOUNG1yBMsnA3bTEg9GRzkNGDycXV7Ea3Y5lk4qD0-_NH_x_2BKTAGwka4za_aKyOd93fnSFlu9ChvV`,
   slot 604530, 6,153 CU. `getaccountinfo` → "Account not found" (expected).
2. **T+0 and T+~1 min** — all of the following fail identically:
   ```
   thru … txn make-state-proof existing <pda>
   thru … txn make-state-proof updating <pda>
   thru … account prepare-decompression <pda>
   thru … account decompress <pda>
   → Error: … RPC error: code: 'Unknown error', message: "bintrie: key not found"
   ```
3. **T+~5 min** — all of them succeed. `account decompress` then completes
   (6,211 CU) and the account returns at its full 100 bytes.

So the failure is a **query-service indexing lag of between ~1 and ~5
minutes**, during which the error is indistinguishable from "this account
is gone forever". In an earlier session we retried only up to ~60 s,
concluded decompression was impossible, and sacrificed three accounts to
that conclusion — the error text invites exactly that misreading.

## What was ruled out

- **Caller error**: the entire flow used official CLI commands; nothing
  else for the caller to do differently except wait.
- **Path-specific defect (CLI- vs program-initiated compression)**: the
  Explorer (scan.thru.org MCP, an independent index) shows compression is
  executed by the system program `taAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEB`
  (instruction type 0x05, proof embedded) — the canonical path. Its leaf
  becomes queryable after the same lag. Nothing suggests an alternate
  compression path would index differently.
- **Permanent service defect**: refuted at T+~5 min.

Supported hypothesis: transient indexing lag in the query service's
compressed-account bintrie (H1-transient). During the lag the Explorer's
`get_account` also returns NOT_FOUND while `get_transaction` fully indexes
the compress — there is no surface anywhere that says "compressed,
indexing in progress".

## Requests

1. Document the indexing lag, or better, distinguish the transient state:
   "leaf not yet indexed (retry)" vs "key not found".
2. `account decompress` could retry/poll internally instead of surfacing
   the raw bintrie error.
3. Document `tsys_account_compress`'s expected proof shape. From a program
   it returns syscall error −43 for every proof kind the CLI can fetch
   (`creating`; `existing` is unobtainable for a live account since live
   accounts are not in the bintrie). There is no spec page for the syscall;
   the CLI builds its proof internally. Program-initiated compression is
   currently trial-and-error with no documented target.

## Supporting observation

The Explorer's `get_transaction` reports **consumed memory units** (this
compress: 1 MU) — data the CLI omits entirely (see issue #36); worth
unifying.
