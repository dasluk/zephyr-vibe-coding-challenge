#!/usr/bin/env bash
# Builds, flashes, and confirms the target is attachable for gdb debugging.
#
# Debugging uses the same host.docker.internal:3333 gdbserver that VS Code's
# "Zephyr IDE: Attach (macOS remote pyocd)" launch config connects to - it
# already runs persistently on the macOS host (started outside this
# container), this script does not start it.
#
# Right after a flash the probe/core can be momentarily out of sync with that
# gdbserver (it shares the physical probe with the flash request), so the
# first attach attempt or two can fail even though the flash itself
# succeeded. This retries the attach check several times before giving up.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="app/build/demo"
ELF="$BUILD_DIR/zephyr/zephyr.elf"
GDB="/opt/toolchains/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb"
GDB_TARGET="host.docker.internal:3333"
MAX_ATTEMPTS="${DEBUG_ATTACH_MAX_ATTEMPTS:-5}"
RETRY_DELAY="${DEBUG_ATTACH_RETRY_DELAY:-3}"

SKIP_FLASH=0
if [[ "${1:-}" == "--skip-flash" ]]; then
    SKIP_FLASH=1
    shift
fi

if [[ "$SKIP_FLASH" -eq 0 ]]; then
    "$SCRIPT_DIR/flash.sh" "$@"
else
    echo "Skipping build+flash (--skip-flash); reusing existing ${ELF}"
fi

if [[ ! -f "$ELF" ]]; then
    echo "ELF not found at ${ELF}" >&2
    exit 1
fi

check_log="$(mktemp)"
trap 'rm -f "$check_log"' EXIT

echo "Verifying gdb can attach to ${GDB_TARGET}..."
attempt=1
while true; do
    if "$GDB" --batch -q \
        -ex "set pagination off" \
        -ex "set remotetimeout 5" \
        -ex "target remote ${GDB_TARGET}" \
        -ex "info registers pc" \
        -ex "detach" \
        "$ELF" >"$check_log" 2>&1; then
        echo "Target is attachable at ${GDB_TARGET}."
        break
    fi

    if (( attempt >= MAX_ATTEMPTS )); then
        echo "Could not attach to target after ${MAX_ATTEMPTS} attempts. Last gdb output:" >&2
        cat "$check_log" >&2
        exit 1
    fi

    echo "Attach attempt ${attempt}/${MAX_ATTEMPTS} failed, retrying in ${RETRY_DELAY}s..." >&2
    attempt=$((attempt + 1))
    sleep "$RETRY_DELAY"
done

echo
echo "Ready to debug."
echo "  ELF:        ${ELF}"
echo "  gdb target: ${GDB_TARGET}"
echo "  gdb:        ${GDB}"
echo
echo "Launch gdb with a custom command file (do not reset/load - the target is already running the just-flashed firmware):"
echo "  ${GDB} ${ELF} -x path/to/your_commands.gdb"
