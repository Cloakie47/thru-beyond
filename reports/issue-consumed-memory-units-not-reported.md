# Feature request: report consumed memory units in `thru txn execute` output

## Environment

- thru CLI: 0.3.2+54058649 (Linux x86_64, from the v0.3.2 GitHub release tarball)

## Current behavior

A transaction declares three budgets: `req_compute_units`, `req_state_units`,
and `req_memory_units`. After execution, the CLI reports consumption for two
of them:

```
Compute Units Consumed: 9582
State Units Consumed: 0
...
Pages Used: 2
```

There is no "Memory Units Consumed" line. `thru txn get <signature>` shows the
*requested* value:

```
Requested Memory Units: 10000
```

but consumed MU appears nowhere in `txn execute`, `txn get`, or any other CLI
output I could find.

## Expected / requested

A `Memory Units Consumed` line in the `thru txn execute` response (and in
`thru txn get`), alongside the existing compute and state lines.

## Why it matters

Memory units are one of the three budgets every transaction must declare
upfront, and MU charges peak usage — which makes them the hardest of the three
to reason about statically. Right now the only way to size `req_memory_units`
is to guess high, because actual consumption is unmeasurable from the CLI.
`Pages Used` is printed and correlates with copy-on-write activity, but it is
not denominated in MU and doesn't capture peak memory of heap/stack growth.

*Filed as https://github.com/Unto-Labs/thru/issues/36*
