# Programs written the documented quickstart way cannot be CPI callees

## Environment

- thru CLI 0.3.2+54058649, C SDK v0.3.2, toolchain v0.3.2
- Alphanet (`https://rpc.alphanet.thru.org`), node 0.0.0-local+599daf60

## Summary

The invoke spec says CPI instruction data is delivered in registers ("Sets
a0 to instruction data address, a1 to data length"), and the SDK entry stub
preserves a0/a1 into `start`. But the C quickstart's documented pattern
reads instruction data via `tsdk_txn_get_instr_data(tsdk_get_txn())`, which
always returns the TOP-LEVEL transaction's instruction data. Under CPI the
two disagree: a quickstart-pattern callee parses the *caller's* payload and
(if it validates sizes, as the quickstart teaches) rejects every
invocation. Such programs are silently uninvokable — the failure surfaces
as the callee's own validation error, which looks like a caller bug.

## Steps to reproduce

1. Deploy the documented quickstart counter (or any program using
   `tsdk_txn_get_instr_data()` with a size check).
2. From another program, `tsys_invoke` it with a correctly formed payload
   for its instruction set.
3. The callee reverts with its own "bad instruction size" error: it saw the
   top-level transaction's instruction data, not the invoke payload.

## Expected

Either the txn accessors are frame-aware (return the current invocation's
data), or the C guide documents that CPI-capable programs must declare

```c
TSDK_ENTRYPOINT_FN void start(uchar const *instr_data, ulong instr_data_sz)
```

— which works for top-level execution as well (the VM initializes a0/a1
the same way), and is what we had to convert both test programs to before
any CPI succeeded.

## Actual

Nothing in the C guide or quickstart mentions the register form; every
example uses the txn accessors. The result is an ecosystem default in
which programs cannot be composed via CPI without a rebuild.
