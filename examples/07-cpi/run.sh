#!/bin/bash
# Reproduce example 07 measurements (programs already deployed). WSL2 Ubuntu.
set -e
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
CALLER="ta9TmfhHffn5hJ3P83hC8NtwERjworfg7pSGxU_GrEPEmy"
CALLEE="taAf417KM3aXeDTILaGkk2kUMpJbDadZSGnm-1uSAJIRCu"
le32() { printf '%08x' "$1" | sed 's/\(..\)\(..\)\(..\)\(..\)/\4\3\2\1/'; }
le16() { printf '%04x' "$1" | sed 's/\(..\)\(..\)/\2\1/'; }
run() { # type n callee_idx aux
  for i in 1 2 3; do
    echo "=== type=$1 n=$2 run $i ==="
    thru --quiet $U txn execute --fee 0 --readonly-accounts "$CALLEE" \
      "$CALLER" "$(le32 "$1")$(le32 "$2")$(le16 "$3")$(le16 "$4")" || true
  done
}
run 0 0 0 0
run 1 0 2 0
for N in 1 2 4 8; do run 2 "$N" 2 0; done
for N in 1 2 4 8 13 14 15; do run 3 "$N" 2 0; done
run 4 0 2 0   # expected: whole transaction reverts with 0x7BAD
