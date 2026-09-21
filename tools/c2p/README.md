# Amiga chunky-to-planar support

These three routines by Mikael Kalms support the native planar display paths.
They are linked into the Amiga executable even when running with MiniGL.
They are build dependencies, not Carmageddon data files or runtime libraries.

`lib/` contains the exact objects used by the R10 game build:

- `c2p1x1_4_c5_bm.o`
- `c2p1x1_6_c5_bm_040.o`
- `c2p1x1_8_c5_bm_040.o`

The corresponding sources from Kalms' C2P collection (`bitmap/`) are in `src/`.
They are public domain; the original author notice and terms are preserved in
[UPSTREAM-README.txt](UPSTREAM-README.txt). The normal game build links these
bundled objects without requiring an assembler or a separate C2P download.

## Optional rebuild

With Vasm's Motorola assembler and the Amiga NDK assembly includes installed:

```sh
M68K_TOOLCHAIN_PATH=/opt/amiga sh tools/c2p/build.sh
```

Override `VASM` for the assembler executable, `AMIGA_NDK_INCLUDE` for the
directory containing `graphics/gfx.i`, or `C2P_OUTPUT_DIR` to write the objects
elsewhere. For example, to check a rebuild without replacing bundled files:

```sh
C2P_OUTPUT_DIR=/tmp/dethrace-c2p-rebuilt sh tools/c2p/build.sh
for object in tools/c2p/lib/*.o; do
    cmp "$object" "/tmp/dethrace-c2p-rebuilt/${object##*/}"
done
```

Vasm 2.0d with `-Fhunk -m68040` reproduced all three bundled objects byte for
byte. This publication change does not alter the routines or game compiler
flags. `AMIGA_C2P_PATH` selects an alternative object directory in the game
build; it is independent of the extra SDK headers in `AMIGA_SUPPORT_PATH`.
