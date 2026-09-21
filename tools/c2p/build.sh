#!/bin/sh
# Optional rebuild; normal game builds use the checked-in objects.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
toolchain_dir=${M68K_TOOLCHAIN_PATH:-/opt/amiga}
assembler=${VASM:-$toolchain_dir/bin/vasmm68k_mot}
includes=${AMIGA_NDK_INCLUDE:-$toolchain_dir/m68k-amigaos/ndk-include}
output_dir=${C2P_OUTPUT_DIR:-$source_dir/lib}

if [ ! -f "$includes/graphics/gfx.i" ]; then
    echo "Missing graphics/gfx.i; set AMIGA_NDK_INCLUDE to the Amiga NDK assembly include directory." >&2
    exit 1
fi
mkdir -p "$output_dir"
for name in c2p1x1_4_c5_bm c2p1x1_6_c5_bm_040 c2p1x1_8_c5_bm_040; do
    "$assembler" -Fhunk -m68040 -I"$includes" -o "$output_dir/$name.o" "$source_dir/src/$name.s"
done
