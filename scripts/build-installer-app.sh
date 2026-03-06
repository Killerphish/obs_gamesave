#!/usr/bin/env bash
# Build GameSave Installer.app using swiftc (no Xcode GUI required).
# Run from repo root: ./scripts/build-installer-app.sh
# Output: installer/build/Release/GameSave Installer.app

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALLER_DIR="$ROOT/installer"
SRC_DIR="$INSTALLER_DIR/GameSave Installer"
OUT_DIR="$INSTALLER_DIR/build/Release"
APP_NAME="GameSave Installer"
APP_BUNDLE="$OUT_DIR/$APP_NAME.app"
EXE_NAME="$APP_NAME"
SDK_PATH="$(xcrun --sdk macosx --show-sdk-path)"
ARCH="$(uname -m)"
TARGET="${ARCH}-apple-macos12.0"

cd "$ROOT"
mkdir -p "$OUT_DIR"

echo "==> Building $APP_NAME (swiftc, $ARCH)..."
swiftc \
  -target "$TARGET" \
  -sdk "$SDK_PATH" \
  -framework AppKit \
  -framework Foundation \
  -o "$OUT_DIR/$EXE_NAME" \
  "$SRC_DIR/main.swift" \
  "$SRC_DIR/AppDelegate.swift" \
  "$SRC_DIR/InstallerLogic.swift"

echo "==> Creating app bundle..."
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"
mv "$OUT_DIR/$EXE_NAME" "$APP_BUNDLE/Contents/MacOS/"

# Info.plist with CFBundleExecutable set to the actual binary name
sed "s/\$(EXECUTABLE_NAME)/$EXE_NAME/g" "$SRC_DIR/Info.plist" > "$APP_BUNDLE/Contents/Info.plist"

echo "==> Done. App: $APP_BUNDLE"
echo "    To embed the plugin, run: ./scripts/build-installer.sh (builds plugin then copies it into this app)"
