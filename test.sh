#!/usr/bin/env bash

set -o nounset

TEMP_STORE=$(mktemp -d /tmp/sls.XXXX)
TEMP_SOCKET=$(mktemp /tmp/sls.XXXX)
rm "$TEMP_SOCKET"

./sls --dir="$TEMP_STORE" --unix_domain_file="$TEMP_SOCKET" &
SLS_PID=$!

./test_client --unix_domain_file=/tmp/sls.sock

kill $SLS_PID
