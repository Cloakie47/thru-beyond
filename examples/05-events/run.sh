#!/bin/bash
# Reproduce example 05 measurements (program already deployed). WSL2 Ubuntu.
set -e
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
PROG="tay1XampjPF__geQXy0YoyM24cCKzL6_AcTS-VTV2C-Add"
le32() { printf '%08x' "$1" | sed 's/\(..\)\(..\)\(..\)\(..\)/\4\3\2\1/'; }

run() { # type n
  for i in 1 2 3; do
    echo "=== type=$1 n=$2 run $i ==="
    thru --quiet $U txn execute --fee 0 "$PROG" "$(le32 "$1")$(le32 "$2")"
  done
}

run 0 0
for n in 0 8 64 512 2048 4000 4088 4096 4104 4200 8100 8192 8200; do run 1 "$n"; done
for n in 0 8 64 512 2048 4000 4088 4096 4104 4200 8100 8192 8200; do run 2 "$n"; done
for p in 1 2 3 4; do run 3 "$p"; done
