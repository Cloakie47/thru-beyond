# `thru txn execute --help`: contradictory default for `--compute-units` (1000000000 vs 300000000)

## Environment

- thru CLI: 0.3.2+54058649 (Linux x86_64, from the v0.3.2 GitHub release tarball)
- Also present in the npm-installed Windows build of 0.3.2

## Steps to reproduce

1. Run: `thru txn execute --help`
2. Read the `--compute-units` option text.
3. Optionally: execute any transaction without passing `--compute-units`, then
   inspect it with `thru txn get <signature>`.

## Expected

The prose description and the clap `[default: ...]` annotation state the same
number, and that number matches what an unflagged transaction actually requests.

## Actual

The help text contradicts itself in a single line:

```
      --compute-units <COMPUTE_UNITS>
          Compute units (optional, defaults to 1000000000) [default: 300000000]
```

Observed behavior: a transaction executed without `--compute-units` requests
300,000,000 — `thru txn get` on such a transaction reports:

```
Requested Compute Units: 300000000
```

So the clap annotation is correct and the prose ("defaults to 1000000000") is
stale. The DevKit documentation quoting these defaults inherits the same
inconsistency.
