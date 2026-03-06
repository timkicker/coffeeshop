#!/bin/bash
set -e

# change these please, i only hardcoded them for me personally
REPO=~/Repositories/modmanager
SD_PATH=/run/media/tim/HOMIE/wiiu/apps/coffeeshop
BUILD_DIR=$REPO/build

echo "=== CLEAN BUILD DEPLOY ==="

if [ ! -d "$SD_PATH" ]; then
    echo "ERROR: SD card not mounted at $SD_PATH"
    exit 1
fi

echo "Deleting old logs on SD..."
rm -f "$SD_PATH/app.log"
rm -f "$SD_PATH/early.log"

echo "Nuking build dir..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Running CMake..."
cd "$BUILD_DIR"
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake .. -DBUILD_MODE=hw

echo "Building..."
make -j$(nproc)

echo "Deploying to SD..."
cp wiiu_mod_store.wuhb "$SD_PATH/"

echo "=== DONE ==="
