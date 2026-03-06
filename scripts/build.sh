#!/usr/bin/env bash
# Build GameSave OBS plugin (macOS).
# Run from repo root: ./scripts/build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

# Prefer cmake from PATH; otherwise try common macOS locations
if command -v cmake &>/dev/null; then
  CMAKE=cmake
else
  for candidate in /opt/homebrew/bin/cmake /usr/local/bin/cmake "/Applications/CMake.app/Contents/bin/cmake"; do
    if [[ -x "$candidate" ]]; then
      CMAKE="$candidate"
      break
    fi
  done
  if [[ -z "${CMAKE:-}" ]]; then
    echo "Error: cmake not found. Install CMake (e.g. brew install cmake) or add it to PATH." >&2
    exit 1
  fi
  echo "==> Using cmake: $CMAKE"
fi

# On macOS, use Command Line Tools only so Xcode.app (and broken IDESimulatorFoundation) is never loaded
if [[ "$(uname -s)" == "Darwin" ]]; then
  CLT_DIR="/Library/Developer/CommandLineTools"
  if [[ -x "$CLT_DIR/usr/bin/clang" ]]; then
    export DEVELOPER_DIR="$CLT_DIR"
    CLANG_C="$CLT_DIR/usr/bin/clang"
    CLANG_CXX="$CLT_DIR/usr/bin/clang++"
    echo "==> Using Command Line Tools (DEVELOPER_DIR=$CLT_DIR)"
  else
    CLANG_C=""
    CLANG_CXX=""
    if [[ -x /usr/bin/clang ]]; then
      CLANG_C=/usr/bin/clang
      CLANG_CXX=/usr/bin/clang++
    fi
    if [[ -z "$CLANG_C" ]]; then
      XCODE_SELECT="$(xcode-select -p 2>/dev/null)"
      if [[ -n "$XCODE_SELECT" && -x "$XCODE_SELECT/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang" ]]; then
        CLANG_C="$XCODE_SELECT/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
        CLANG_CXX="$XCODE_SELECT/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
      fi
    fi
    if [[ -z "$CLANG_C" || ! -x "$CLANG_C" ]]; then
      echo "Error: No C/C++ compiler found. Install Xcode Command Line Tools: xcode-select --install" >&2
      exit 1
    fi
  fi
  export CMAKE_C_COMPILER="${CLANG_C}"
  export CMAKE_CXX_COMPILER="${CLANG_CXX}"
  CMAKE_CC_ARGS=(-DCMAKE_C_COMPILER="${CLANG_C}" -DCMAKE_CXX_COMPILER="${CLANG_CXX}")
  echo "==> Using C compiler: $CMAKE_C_COMPILER"
fi

# If build dir was previously configured with a non-Ninja generator, clear cache so preset's Ninja is used
BUILD_DIR="$ROOT/build_macos"
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  CACHED_GEN="$(grep -s "CMAKE_GENERATOR:INTERNAL=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | sed 's/.*=//')"
  if [[ -n "$CACHED_GEN" && "$CACHED_GEN" != "Ninja" ]]; then
    echo "==> Clearing cache (generator was '$CACHED_GEN') so Ninja preset can configure..."
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
  fi
fi

echo "==> Configuring (macOS preset)..."
if [[ -n "${CMAKE_CC_ARGS:-}" ]]; then
  "$CMAKE" --preset macos "${CMAKE_CC_ARGS[@]}"
else
  "$CMAKE" --preset macos
fi

echo "==> Building..."
"$CMAKE" --build --preset macos

echo "==> Build complete."
PLUGIN_FOUND="$(find build_macos -maxdepth 4 -type d -name "game-save.plugin" 2>/dev/null | head -1)"
if [[ -n "$PLUGIN_FOUND" ]]; then
  echo "    Plugin bundle: $PLUGIN_FOUND"
else
  echo "    Plugin bundle: under build_macos/ (run ./scripts/install.sh to install)"
fi
echo "    Install with:  ./scripts/install.sh"
