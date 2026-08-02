# Incident: alphanet state-proof validation broken since the 2026-08-02 RPC outage (−23 on all fresh proofs)

## Environment

- thru CLI 0.3.2+54058649; alphanet (`https://rpc.alphanet.thru.org`);
  node 0.0.0-local+599daf60 throughout.

## Onset

- **Last captured known-good proof-consuming transaction:**
  `tsub2BBaYbgVOUNG1yBMsnA3bTEg9GRzkNGDycXV7Ea3Y5lk4qD0-_NH_x_2BKTAGwka4za_aKyOd93fnSFlu9ChvV`
  (account compression, slot **604530**). Further compressions kept
  succeeding until ~19:10:53 UTC (signatures not captured; ~slot 648,0xx).
- **RPC outage:** ~19:16–19:20:40 UTC — `getversion` itself returned
  "upstream connect error … connection termination"; recovery observed at
  19:20:40.
- **First known-bad:** immediately after recovery. Earliest captured
  failing signature:
  `tsLCrkgQuHfVooZ1zPsuJUlN1d3DhllebKxlTjcgqj_aGT1QrDk5evdLTG4edZBnlF7cUjsTieFyX0LNPVSPgoCSG-`
  (slot **648269**) — a program-deploy state-proof transaction, included
  and reverted with:

```
Warning: Transaction completed with execution result: -4 (hex 0xFFFFFFFFFFFFFFFC)
vm_error: -765 (TN_RUNTIME_TXN_ERR_VM_REVERT)
- Manager program error: Syscall error: Invalid state proof (-23)
```

Still failing at slot ~653,395 (≈5,100 slots later) at the time of writing.

## Scope — what fails

Every operation that consumes a freshly fetched state proof, regardless of
proof kind:

- `thru account create` / any program-side `tsys_account_create` with a
  just-fetched `creating` proof → revert, syscall −23.
- `thru program create` (deploy) → manager error −23 at the
  "Creating state proofs for permanent program" step.
- `thru account decompress` → the decompress transaction reverts with
  `User error: 18446744073709551593` = (u64)(−23), even though
  `prepare-decompression` happily serves the proof.
- During the same window, the two most recently compressed leaves also
  returned `bintrie: key not found` from all proof surfaces 40+ minutes
  after compression (normal indexing lag is ~1–5 min).

## Scope — what does NOT fail (the key discriminator)

**Execution and consensus are healthy.** 24 fresh executions across seven
already-deployed programs during the incident reproduced their historical
compute-unit figures *exactly* (e.g. the minimal program's 4,763 CU, an
8-page stack grow's 34,127, CPI to depth 14 at 89,038), with correct
events, account reads/writes, resizes, and per-transaction determinism
throughout. **Program upgrades also succeeded during the incident.** Only
the proof path is affected — consistent with the proof service signing
proofs against a trie root the chain no longer accepts.

## Recovery probes

- Probe window 1: creation attempt every 2 min for 30 min
  (19:26–19:56 UTC) — all failed.
- Probe window 2: every 5 min for 45 min (19:39–~20:19 UTC) — all failed.
- Repeated manual checks over the following hours — all failed.

## Minimal repro

```bash
U="--url https://rpc.alphanet.thru.org"
PROG=tahE2pWV9nqlASlyX7PTaTXCfx7iLC8l7E29FVPSMxcxfY   # any deployed program
PDA=$(thru $U program derive-address "$PROG" any_new_seed | grep -oP 'Derived Address: \K\S+')
thru $U txn make-state-proof creating "$PDA"           # succeeds — proof served
# submit any create using that proof (or simply: thru $U program create <seed> <bin>)
# -> revert, "Syscall error: Invalid state proof (-23)"
```

The proof service serves; the chain rejects. Nothing the caller changes
(fresh proofs, different accounts, different proof kinds, hours of
retries) alters the outcome.
