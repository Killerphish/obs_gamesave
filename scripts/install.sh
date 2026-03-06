#!/usr/bin/env bash
# Install GameSave plugin into OBS so it loads on next launch.
# Run from repo root: ./scripts/install.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OBS_PLUGINS="${OBS_PLUGINS:-$HOME/Library/Application Support/obs-studio/plugins}"

# Find the built .plugin bundle under build_macos
PLUGIN_BUNDLE=""
if [[ -d "$ROOT/build_macos" ]]; then
  PLUGIN_BUNDLE="$(find "$ROOT/build_macos" -maxdepth 4 -type d -name "game-save.plugin" 2>/dev/null | head -1)"
fi

if [[ -z "$PLUGIN_BUNDLE" || ! -d "$PLUGIN_BUNDLE" ]]; then
  echo "Error: game-save.plugin not found under build_macos. Run ./scripts/build.sh first."
  exit 1
fi

echo "==> Installing GameSave plugin to OBS"
echo "    From: $PLUGIN_BUNDLE"
echo "    To:   $OBS_PLUGINS"

mkdir -p "$OBS_PLUGINS"
rm -rf "$OBS_PLUGINS/game-save.plugin"
cp -R "$PLUGIN_BUNDLE" "$OBS_PLUGINS/"

echo "==> Done. Restart OBS, then open Docks → GameSave."
