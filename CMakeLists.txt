cmake_minimum_required(VERSION 3.13)
project(wiiu_mod_store C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO environment variable not set")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})

# Include devkitPro helpers
include(${DEVKITPRO}/wut/share/wut.cmake OPTIONAL)

# SDL2 paths (manual, since find_package is flaky with this toolchain)
set(SDL2_DIR     ${DEVKITPRO}/portlibs/wiiu)
set(SDL2_INC     ${SDL2_DIR}/include/SDL2)
set(SDL2_LIB     ${SDL2_DIR}/lib)
set(PORTLIBS_PPC ${DEVKITPRO}/portlibs/ppc)

# Source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.c
)

add_executable(wiiu_mod_store ${SOURCES})

target_include_directories(wiiu_mod_store PRIVATE
    src
    libs
    ${SDL2_DIR}/include
    ${SDL2_INC}
    ${DEVKITPRO}/wut/include
    ${PORTLIBS_PPC}/include
)

target_link_directories(wiiu_mod_store PRIVATE
    ${SDL2_LIB}
    ${PORTLIBS_PPC}/lib
    ${DEVKITPRO}/wut/lib
    ${DEVKITPRO}/portlibs/wiiu/lib
)

target_link_libraries(wiiu_mod_store PRIVATE
    SDL2
    SDL2_ttf
    SDL2_image
    freetype
    png
    z
    m
    harfbuzz
    brotlidec
    brotlicommon
    bz2
    curl
    mbedtls
    mbedcrypto
    wut
)

# Generate .rpx
wut_create_rpx(wiiu_mod_store)

# Generate .wuhb
wut_create_wuhb(wiiu_mod_store
    NAME "Mod Store"
    AUTHOR "Tim"
    ICON  ${CMAKE_SOURCE_DIR}/meta/icon.png
    CONTENT ${CMAKE_SOURCE_DIR}/meta/fonts
)
