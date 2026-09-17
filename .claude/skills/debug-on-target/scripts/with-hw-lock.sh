#!/usr/bin/env bash
# Serializes access to the single physical board (one ST-Link probe, one
# gdbserver, one serial bridge) across however many agents/worktrees are
# working on this repo at once. Without this, two agents flashing or
# watching serial at the same time will race the same hardware and corrupt
# each other's session.
#
# Usage: with-hw-lock.sh <command> [args...]
#   ./scripts/with-hw-lock.sh ./scripts/build.sh
#   ./scripts/with-hw-lock.sh ./scripts/flash.sh
#   ./scripts/with-hw-lock.sh ./scripts/debug_on_target.sh --skip-flash
#   ./scripts/with-hw-lock.sh ./scripts/watch_serial.py
#
# The lock file lives under /tmp (shared by every git worktree, not
# per-worktree) so it actually serializes across worktrees, not just
# within one. It's a plain flock: whoever's holding it blocks everyone
# else until the wrapped command exits.
#
# IMPORTANT for long-running commands (watch_serial.py, a gdb session):
# the lock is held for as long as the wrapped command runs. Stop it
# (ctrl-C, or kill the background job) as soon as you're done reading -
# don't leave a serial watcher or gdb session open "just in case", or
# every other agent queued on this lock stalls until you do.
set -euo pipefail

LOCK_FILE="${ZEPHYR_HW_LOCK_FILE:-/tmp/zephyr-hw.lock}"

if [[ $# -eq 0 ]]; then
    echo "usage: $0 <command> [args...]" >&2
    exit 1
fi

exec 9>"$LOCK_FILE"
echo "[hw-lock] waiting for ${LOCK_FILE} (pid $$)..." >&2
flock 9
echo "[hw-lock] acquired by pid $$: $*" >&2

set +e
"$@"
status=$?
set -e

echo "[hw-lock] releasing (pid $$, exit ${status})" >&2
exit "$status"
