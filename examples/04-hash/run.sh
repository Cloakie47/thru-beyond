#!/bin/bash
# Reproduce example 04 measurements (programs already deployed). WSL2 Ubuntu.
set -e
export PATH="$HOME/thru-cli:$PATH"
U="--url https://rpc.alphanet.thru.org"
PA="ta-rWexuBmL558uxLZXqOb23DM0HeThZGSxG2mOm3-6oxv"
PB="taUgLhBWu3NCyYud3ioz-8XS-K8ly2BxzHk3-HRaQ0MMcb"

for arm in "A:$PA" "B:$PB"; do
  name="${arm%%:*}"; prog="${arm##*:}"
  for size in 0 32 256 1024 4096; do
    hex=$(head -c "$size" /dev/zero | xxd -p | tr -d '\n')
    for i in 1 2 3; do
      echo "=== arm $name size $size run $i ==="
      thru --quiet $U txn execute --fee 0 "$prog" "$hex"
    done
  done
done
