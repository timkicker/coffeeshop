#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
WUHB="$BUILD_DIR/wiiu_mod_store.wuhb"
RPX="$BUILD_DIR/wiiu_mod_store.rpx"
TOOLCHAIN="/opt/devkitpro/wut/share/wut.toolchain.cmake"
CEMU="${CEMU:-cemu}"

# Load devkitPro env
source /etc/profile.d/devkit-env.sh 2>/dev/null || true

echo "[1/3] Configuring..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Debug \
    --log-level=WARNING

echo "[2/3] Building..."
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo "[3/3] Launching Cemu..."

# Prefer .wuhb, fallback to .rpx
if [[ -f "$WUHB" ]]; then
    TARGET="$WUHB"
elif [[ -f "$RPX" ]]; then
    TARGET="$RPX"
else
    echo "Error: No .wuhb or .rpx found in $BUILD_DIR"
    exit 1
fi

echo "Loading: $TARGET"

if ! command -v "$CEMU" &>/dev/null && [[ ! -x "$CEMU" ]]; then
    echo "Cemu not found at '$CEMU'. Set CEMU=/path/to/cemu or launch manually."
    exit 0
fi

"$CEMU" -g "$TARGET"
