#!/bin/bash
set -e
cd "$(dirname "$0")"

mkdir -p build
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/wut/share/wut.toolchain.cmake \
         -DBUILD_MODE=${1:-cemu} 2>/dev/null || true

make -j$(nproc)

if [ "${1:-cemu}" = "cemu" ]; then
    cemu -g "$(pwd)/wiiu_mod_store.wuhb" &
fi
