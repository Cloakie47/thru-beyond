# A caller cannot catch a callee's revert — the transaction aborts first

## Environment

- thru CLI 0.3.2+54058649, C SDK v0.3.2, alphanet, node 0.0.0-local+599daf60

## Summary

The C cross-program-invocation reference shows callers branching on
`tsys_invoke`'s return value and the `invoke_err_code` out-parameter,
implying callee failures can be handled. In practice a callee that calls
`tsdk_revert(code)` (i.e. `tsys_exit(code, revert=1)`) aborts the ENTIRE
transaction immediately with the callee's error code. The caller's
post-invoke code never runs.

## Steps to reproduce

Caller (absorbs the failure by design):

```c
ulong invoke_err = 0, r;
r = tsys_invoke(payload, sz, callee_idx, NULL, &invoke_err);
ulong codes[2] = { r, invoke_err };
tsys_emit_event(codes, sizeof(codes));   /* never reached */
tsdk_return(TSDK_SUCCESS);
```

Callee: `tsdk_revert(0x7BAD);`

Execute: the transaction fails with `vm_error=-765 (VM_REVERT)`,
`user_error=0x7BAD` (the callee's code). No event is emitted; the caller's
handler demonstrably did not run.

## What IS catchable (for precision)

Syscall-level invoke failures DO return in-band: e.g. exceeding the call
depth limit returns −24 (`TN_VM_ERR_SYSCALL_CALL_DEPTH_TOO_DEEP`) as
`tsys_invoke`'s return value, and the caller can act on it. The
uncatchable class is callee *execution* reverts via `tsys_exit(_, 1)`.

## Expected

Either (a) callee reverts unwind only the callee frame and surface through
`invoke_err_code` as the reference implies, or (b) the reference documents
plainly that reverts are transaction-fatal from any frame and
`invoke_err_code` covers a narrower error class.

## Why it matters

"Try the call, fall back on failure" is a standard composition pattern;
today it cannot be built, and the reference's example suggests it can.
