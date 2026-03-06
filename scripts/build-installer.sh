#!/usr/bin/env bash
# Build GameSave plugin and the GameSave Installer app, then embed the plugin in the app.
# Run from repo root: ./scripts/build-installer.sh
# Output: installer/build/Release/GameSave Installer.app (with plugin inside when plugin was built)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALLER_DIR="$ROOT/installer"
# Plugin may be under build_macos (Ninja) or build_macos/RelWithDebInfo or build_macos/rundir/RelWithDebInfo
PLUGIN_PATH=""
if [[ -d "$ROOT/build_macos" ]]; then
  PLUGIN_PATH="$(find "$ROOT/build_macos" -maxdepth 4 -type d -name "game-save.plugin" 2>/dev/null | head -1)"
fi
if [[ -z "$PLUGIN_PATH" || ! -d "$PLUGIN_PATH" ]]; then
  PLUGIN_PATH="$ROOT/build_macos/RelWithDebInfo/game-save.plugin"
fi
APP_PATH="$INSTALLER_DIR/build/Release/GameSave Installer.app"
RESOURCES="$APP_PATH/Contents/Resources"

cd "$ROOT"

# 1. Build plugin if not present (required so the installer always includes the plugin)
if [[ ! -d "$PLUGIN_PATH" ]]; then
  echo "==> Building GameSave plugin..."
  if ! "$SCRIPT_DIR/build.sh"; then
    echo ""
    echo "Error: Plugin build failed. The installer must include the plugin."
    echo "  - Install Command Line Tools: xcode-select --install"
    echo "  - Ensure CMake and Ninja are available (e.g. brew install cmake ninja)"
    echo "  - Then run: $SCRIPT_DIR/build-installer.sh"
    exit 1
  fi
  echo "    Plugin built."
  # Re-detect path in case build put it somewhere else
  PLUGIN_PATH=""
  if [[ -d "$ROOT/build_macos" ]]; then
    PLUGIN_PATH="$(find "$ROOT/build_macos" -maxdepth 4 -type d -name "game-save.plugin" 2>/dev/null | head -1)"
  fi
  [[ -z "$PLUGIN_PATH" || ! -d "$PLUGIN_PATH" ]] && PLUGIN_PATH="$ROOT/build_macos/RelWithDebInfo/game-save.plugin"
fi
if [[ ! -d "$PLUGIN_PATH" ]]; then
  echo "Error: Plugin not found at $PLUGIN_PATH after build. Cannot create installer." >&2
  exit 1
fi
echo "==> Using plugin: $PLUGIN_PATH"

# 2. Build installer app with swiftc (reliable; xcodebuild often fails with plugin errors)
echo "==> Building GameSave Installer app (swiftc)..."
"$SCRIPT_DIR/build-installer-app.sh"

# 3. Embed plugin in app bundle (required)
echo "==> Embedding plugin in installer app..."
mkdir -p "$RESOURCES"
rm -rf "$RESOURCES/game-save.plugin"
cp -R "$PLUGIN_PATH" "$RESOURCES/"

echo "==> Done."
APP_PATH_ABS="$ROOT/installer/build/Release/GameSave Installer.app"
if [[ -d "$APP_PATH_ABS" ]]; then
  echo "    Installer app: $APP_PATH_ABS"
  echo "    Run it to install GameSave into OBS (restart OBS afterward)."
  echo "    Opening folder in Finder..."
  open "$(dirname "$APP_PATH_ABS")"
else
  echo "    Installer app not found at: $APP_PATH_ABS"
fi
