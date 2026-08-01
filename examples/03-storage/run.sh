#!/bin/bash
# Reproduce example 03 measurements (assumes program + account already deployed;
# see README.md for the one-time init flow). Run inside WSL2 Ubuntu.
set -e
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
PROG="tasFvCl6TciwEVQO1tU-UJ2qDt7KXtx86qaZzWRf7l9_d1"
ACC="taCr3SBDQu395xbvc0eG6odoWC6uhwJEp5O1kgMOwqlJoi"

for op in "write_p0:010000000200" "write_p0_x2:020000000200" \
          "write_p0_p1:030000000200" "read_p1:040000000200"; do
  name="${op%%:*}"; hex="${op##*:}"
  for i in 1 2 3; do
    echo "=== $name run $i ==="
    thru --quiet $U txn execute --fee 0 --readwrite-accounts "$ACC" "$PROG" "$hex"
  done
done
