#!/usr/bin/env bash
# Build and install GameSave plugin in one step. OBS will load it on next start.
# Run from repo root: ./scripts/build-and-install.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

"$SCRIPT_DIR/build.sh"
"$SCRIPT_DIR/install.sh"
