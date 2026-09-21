#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${DETHRACE_RENDERFIX_BUILD_DIR:-/tmp/dethrace-renderfix-build}
toolchain_dir=${M68K_TOOLCHAIN_PATH:-/opt/amiga}
support_dir=${AMIGA_SUPPORT_PATH:-/mnt/d/dev/Amiga_SDK/common}
export C_INCLUDE_PATH="$source_dir/tools/showcase/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
cmake -S "$source_dir" -B "$build_dir" -DCMAKE_TOOLCHAIN_FILE="$source_dir/cmake/toolchains/amiga-gcc6.cmake" -DM68K_TOOLCHAIN_PATH="$toolchain_dir" -DAMIGA_SUPPORT_PATH="$support_dir" -DCMAKE_BUILD_TYPE=Release -DDETHRACE_PLATFORM_AMIGA=ON -DDETHRACE_PLATFORM_SDL2=OFF -DDETHRACE_AMIGA_SHARED_MINIGL=ON -DDETHRACE_AMIGA_SHARED_MINIGL_ROOT="$source_dir/tools/pistorm3d-v12-sdk" -DDETHRACE_NET_ENABLED=OFF -DDETHRACE_SOUND_ENABLED=ON -DBRENDER_BUILD_EXAMPLES=OFF -DBRENDER_INSTALL=OFF
cmake --build "$build_dir" --target dethrace --parallel 4
