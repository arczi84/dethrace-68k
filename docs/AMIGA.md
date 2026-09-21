# AmigaOS 68k port

This branch ports Dethrace 0.10.1 to classic AmigaOS 3 on m68k. It uses a native Intuition/CyberGraphX display backend and AHI audio; SDL is not used by the Amiga build.

The port was brought forward from the older working SDL 1.2-era Amiga port. The old source tree should be kept as a compatibility reference, but it is not required to build this branch.

## Requirements

- bebbo Amiga GCC 6.5.0b toolchain; the tested default is `/opt/amiga-debian`
- CMake 3.23 or newer
- AmigaOS and AHI headers
- the following chunky-to-planar objects (now bundled in `tools/c2p/lib`):
  - `c2p1x1_4_c5_bm.o`
  - `c2p1x1_6_c5_bm_040.o`
  - `c2p1x1_8_c5_bm_040.o`

The toolchain uses the bundled C2P objects by default. See
[tools/c2p](../tools/c2p/README.md) for their sources and optional rebuild.
`AMIGA_C2P_PATH` overrides their directory independently of `AMIGA_SUPPORT_PATH`,
which supplies extra SDK headers and libraries. For the current MiniGL R10
build, use the [R10 build instructions](AMIGA_RENDERFIX.md) and compiler flags in
[BUILD_BASELINE.md](../BUILD_BASELINE.md); the rest of this document describes
the earlier native port.

## Building

```sh
cmake -S . -B build-amiga \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/amiga-gcc6.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DDETHRACE_PLATFORM_AMIGA=ON \
  -DDETHRACE_SOUND_ENABLED=ON \
  -DDETHRACE_NET_ENABLED=OFF \
  -DBUILD_TESTS=OFF \
  -DDETHRACE_INSTALL=OFF

cmake --build build-amiga -j4
```

To use different dependency locations, add for example:

```sh
-DM68K_TOOLCHAIN_PATH=/path/to/amiga-gcc \
-DAMIGA_SUPPORT_PATH=/path/to/amiga-support
```

The resulting executable is `build-amiga/dethrace`. Copy it to the Carmageddon directory containing `DATA`, then run it from that directory.

## Video modes

The default mode is 8-bit CyberGraphX. Select one display mode per invocation:

| Option | Backend | Output |
| --- | --- | --- |
| no video option or `--bpp=8` | CyberGraphX | 8-bit chunky |
| `--aga` | native AGA | 8-bit planar through C2P |
| `--bpp=6` | native chipset | HAM6 through C2P |
| `--ask` | ASL requester | manual screen-mode selection |

The old `--bpp=16` path is not currently supported and falls back to 8-bit output.

Example:

```text
dethrace -nocutscenes --sound-options --debug=0 --aga
```

The backend also provides raw-key to DirectInput key mapping, mouse buttons and movement, palette updates, frame limiting, and a blank CHIP-memory pointer used to hide the Intuition cursor.

The FPS limiter is disabled by default on Amiga, matching the stable older port. AmigaOS `Delay()` has a 20 ms minimum step, so trying to implement the desktop default of 60 FPS with it can add an entire tick and make the game noticeably slower. A deliberate limit can still be selected with `--fps=25`, `--fps=50`, or another value; use `--fps=0` for unlocked rendering.

## Audio

The Amiga build replaces miniaudio with the native AHI implementation in `src/harness/audio/miniaudio-ahi-music.c`.

- sound effects use AHI sample channels;
- game music uses the same backend and supports 16-bit PCM data;
- Carmageddon 8-bit effects are converted from unsigned PCM to signed PCM when loaded, as required by AHI;
- AHI must be installed and configured in the Amiga or emulator environment.

## Paths, saves and CD handling

The Amiga OS layer uses `PROGDIR:` as both the working directory and preferences directory. File opening includes a case-insensitive directory lookup because game data may use DOS-style case conventions.

Savegames are therefore read from `PROGDIR:DATA/SAVEGAME`. Dethrace 0.10.1 accepts save files matching its expected 948-byte version-6 format; other legacy save layouts are ignored.

The original physical-CD checks are disabled on Amiga and the installation is treated as complete. CD paths resolve to `PROGDIR:` so a copied Carmageddon installation can run without a mounted disc.

## Big-endian handling

All Dethrace, harness, S3 and BRender targets must agree on the byte order. The Amiga build defines:

```text
BR_ENDIAN_BIG=1
BR_ENDIAN_LITTLE=0
```

Do not use the old `IS_BIGENDIAN` CMake variable. It was unset in this build and caused both endian macros to become true in different parts of the program. That corrupted data interpretation and affected savegame loading and potentially model data.

## GCC 68k optimization workarounds

The toolchain intentionally builds release code with `-Ofast -ffast-math -funroll-loops`. Two functions inherited from the tested older port must not use loop unrolling:

| Function | Local options | Reason |
| --- | --- | --- |
| `CollCheck()` | `-O2 -fno-unroll-loops` | GCC 6.5 for m68k miscompiles the collision loop at the global settings; the car passes through terrain and buildings. This fix has been verified at runtime. |
| `BrDbModelRender()` | `-fno-unroll-loops` | Preserved from the stable Amiga BRender port to avoid the corresponding renderer miscompile. |

The older BRender fork also placed `optimize("Ofast")` on these five rasterizer functions:

- `TriangleRender_ZTI_I8_D16_POW2()`
- `TriangleSetup_ZPTI()`
- `ScanlineRender_ZPT_I8_D16()`
- `TrapeziumRender_ZPTI_I8_D16()`
- `TriangleRender_ZPTI_I8_D16_64()`

Those annotations are not copied into the current BRender because the complete Amiga target is already built with `-Ofast`. They did not disable an unsafe optimization and would be redundant.

## BRender changes

The port uses the BRender revision bundled with Dethrace 0.10.1 rather than copying the complete older Amiga fork. Only the required compatibility changes are kept:

- `HostImage*` uses the existing no-op DOS implementation on Amiga because dynamic host-image loading is unavailable;
- `BrDbModelRender()` disables loop unrolling on Amiga as described above;
- big-endian definitions are supplied consistently by CMake.

## Other Amiga-specific behavior

- `--aga`, `--bpp=` and `--ask` are parsed by the harness.
- The native platform bootstrap disables all SDL platform backends.
- The FPS value remains visible during a race even when the full developer information overlay is disabled.
- Network support is disabled in the tested build.

## Runtime status

The following have been tested in the current 0.10.1 port:

- CyberGraphX 8-bit, native AGA 8-bit and HAM6 startup;
- terrain and building collisions;
- AHI effects and music;
- loading compatible saves;
- hidden system cursor;
- on-screen FPS counter.
