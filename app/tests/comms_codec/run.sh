#!/usr/bin/env sh
# Builds and runs the host-side codec unit tests (no Zephyr/hardware needed).
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
APP_ROOT="$(cd "$DIR/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/test_comms_codec"

cc -std=c11 -Wall -Wextra -Werror \
    -I "$APP_ROOT/include" \
    "$APP_ROOT/src/comms_codec.c" \
    "$DIR/test_comms_codec.c" \
    -o "$OUT"

"$OUT"
