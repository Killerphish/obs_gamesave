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

# 1. Build plugin if not present (optional: requires cmake and full OBS build)
if [[ ! -d "$PLUGIN_PATH" ]]; then
  echo "==> Building GameSave plugin..."
  if "$SCRIPT_DIR/build.sh"; then
    echo "    Plugin built."
  else
    echo "    Warning: plugin build failed (e.g. cmake not in PATH). Installer app will run; use 'Choose Plugin…' to pick a game-save.plugin from disk if needed."
  fi
else
  echo "==> Using existing plugin: $PLUGIN_PATH"
fi

# 2. Build installer app with swiftc (reliable; xcodebuild often fails with plugin errors)
echo "==> Building GameSave Installer app (swiftc)..."
"$SCRIPT_DIR/build-installer-app.sh"

# 3. Copy plugin into app bundle when available
if [[ -d "$PLUGIN_PATH" ]]; then
  echo "==> Embedding plugin in installer app..."
  mkdir -p "$RESOURCES"
  rm -rf "$RESOURCES/game-save.plugin"
  cp -R "$PLUGIN_PATH" "$RESOURCES/"
else
  echo "==> Skipping plugin embed (plugin not built). Run the app and use 'Choose Plugin…' to select a plugin if needed."
fi

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
