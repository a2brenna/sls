#!/usr/bin/env bash

set -o nounset

TEMP_STORE=$(mktemp -d /tmp/sls.XXXX)
TEMP_SOCKET=$(mktemp /tmp/sls.XXXX)
rm "$TEMP_SOCKET"

./sls --dir="$TEMP_STORE" --cache_min=100 --cache_max=150 --unix_domain_file="$TEMP_SOCKET" &
SLS_PID=$!

gdb --args ./test_client --unix_domain_file="$TEMP_SOCKET"

kill $SLS_PID
