#!/usr/bin/env bash
# Builds the "demo" board target for the app.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="app/build/demo"

echo "Building app (board: demo) into ${BUILD_DIR}..."
west build -b demo app -d "$BUILD_DIR" "$@"
