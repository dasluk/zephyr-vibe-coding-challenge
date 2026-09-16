#!/usr/bin/env bash
# Builds and flashes the board over the remote pyocd probe server running on
# the macOS host (the "mac-pyocd" runner profile from .vscode/zephyr-ide.json).
#
# Docker Desktop for Mac can't reliably pass the ST-Link's raw USB device
# through to the devcontainer, so pyocd runs as a server on the host and this
# container talks to it via pyocd's "remote:" probe protocol over
# host.docker.internal. That extra hop occasionally drops a request, so
# flashing is retried a few times before giving up.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="app/build/demo"
DEV_ID="remote:host.docker.internal"
MAX_ATTEMPTS="${FLASH_MAX_ATTEMPTS:-5}"
RETRY_DELAY="${FLASH_RETRY_DELAY:-3}"

"$SCRIPT_DIR/build.sh" "$@"

echo "Flashing via pyocd (dev-id=${DEV_ID})..."
attempt=1
while true; do
    if west flash -d "$BUILD_DIR" --runner pyocd --dev-id="$DEV_ID"; then
        echo "Flash succeeded."
        exit 0
    fi

    if (( attempt >= MAX_ATTEMPTS )); then
        echo "Flash failed after ${MAX_ATTEMPTS} attempts." >&2
        exit 1
    fi

    echo "Flash attempt ${attempt}/${MAX_ATTEMPTS} failed, retrying in ${RETRY_DELAY}s..." >&2
    attempt=$((attempt + 1))
    sleep "$RETRY_DELAY"
done
