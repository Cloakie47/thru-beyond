# Failed transactions report no consumed compute units anywhere in the CLI

## Environment

- thru CLI 0.3.2+54058649 (Linux x86_64, from the v0.3.2 GitHub release tarball)
- Alphanet (`https://rpc.alphanet.thru.org`)

## Current behavior

When a transaction fails (e.g. `vm_error=-767 TN_RUNTIME_TXN_ERR_VM_FAILED`
or `-765 VM_REVERT`), the CLI reports only the error:

```
Error: Transaction failed (execution_result=0xFFFFFFFFFFFFFFFD,
vm_error=-767 (TN_RUNTIME_TXN_ERR_VM_FAILED), user_error=0x0)
```

- No `Compute Units Consumed` (or state units / pages) in the error output.
- The `--json` error object contains only the error fields — no signature,
  no consumption figures.
- The failed transaction does not appear in `thru account transactions` for
  the fee payer, so `thru txn get` cannot be used to recover the figures
  either.

## Expected

Failed and reverted transactions consumed real compute before failing; their
CU/SU/pages (and a signature) should be reported the same way successful
ones are — in the `txn execute` response and via `txn get`.

## Why it matters

Failure paths cost compute and count against declared budgets, but they are
currently unmeasurable, so programs cannot be profiled or budgeted for their
revert paths at all. (Concretely: measuring the cost of a program's
guard-clause reverts, or of a deliberate fault, is impossible from the CLI
today.)

*Filed as https://github.com/Unto-Labs/thru/issues/39*
