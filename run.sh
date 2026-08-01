#!/bin/bash
# Reproduce example 01 from scratch. Run inside WSL2 Ubuntu.
# Requires: thru CLI extracted at ~/thru-cli, toolchain + C SDK installed
# via `thru dev toolchain install` and `thru dev sdk install c`,
# a funded `default` key (thru account create default; thru faucet withdraw default 10000).
set -e
export PATH="$HOME/thru-cli:$PATH"
# The SDK locates the toolchain by walking up from the project dir, which
# fails when the repo lives on /mnt/c — point it at the WSL home install.
export RISCV_TOOLCHAIN_ROOT="$HOME/.thru/sdk/toolchain"
U="--url https://rpc.alphanet.thru.org"
SEED="example_01_empty"
BIN="./build/thruvm/bin/tn_example_01_empty_c.bin"

cd "$(dirname "$0")"
make

# Branch on existence: a failed `program create` on an existing seed still
# uploads (and orphans) a temp buffer before erroring, so don't use it as a probe.
PROG="taIjGXEaz6jCa8ORd1YWClEQgbxCdw-hDSpzGtYkZAXk-_"
if thru --quiet $U getaccountinfo "$PROG" >/dev/null 2>&1; then
  thru --quiet $U program upgrade "$SEED" "$BIN"
else
  thru --quiet $U program create "$SEED" "$BIN"
fi
for i in 1 2 3; do
  echo "=== run $i ==="
  thru --quiet $U txn execute --fee 0 "$PROG" ""
done
