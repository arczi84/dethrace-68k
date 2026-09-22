# Amiga MiniGL / PiStorm3D render fixes (R10)

The current branch also includes [Hedeon's optimizations](AMIGA_HEDEON.md),
and the [launcher draw-distance control](../tools/launcher/README.md), identified
as `0.10.1-amiga-r10-hedeon2`. The build procedure below is unchanged.

This branch publishes the current optimized Dethrace source and its matching
BRender submodule. It is based on the q3batch renderer, with the existing
1023-vertex immediate batches and HUD texture cache retained.

## Changes

- Commit the CPU-rendered gallery background before drawing cars, and preserve
  it when the 3D pass starts. Use a 16-bit RGB565 gallery fill value.
- Publish opaque cockpit strip writes to the HUD overlay, including black pixels.
- Isolate the 2D camera passes from world depth, and propagate the game's shadow
  near-plane adjustment into the PiStorm reconstructed projection.
- Defer BGLSPIKE cutout triangles to the BRender scene flush, preserving their
  depth test. This prevents later opaque surfaces from overwriting the blade
  when the texture-alpha workaround has disabled depth writes.
- Support the launcher's extended physical output sizes while retaining the
  game's original 640x480 high-resolution layout; print version and build date.
- Add a Classic-only startup workaround that clears the native bitmap and waits
  for GL completion before presenting. PiStorm skips this startup workaround.
  Log the loaded MiniGL library version and ID.

## Validation status

A clean cross-build of this publication snapshot passed with Amiga GCC 6.5.0b.
Both host blade regression checks below passed from the same checkout.
The C2P packaging fix was also built from a fresh recursive clone, with an
external support directory containing only headers (no C2P objects or libdl).
Reassembling the bundled C2P sources reproduced all three objects byte for byte;
removing the unused libdl link did not change the resulting executable bytes.

The user confirmed the cockpit, shadow and blade fixes. The blade queue also
passed host ASan/UBSan tests and a Mesa EGL visibility test using the real
BGLSPIKE texture mask. These host tests are not a complete game or MiniGL run.

The Classic first-frame garbage remains **hardware-unverified**. Older game
executables exhibit it too; PiStorm3D does not. This does not establish a new
MiniGL regression. No MiniGL library is installed or replaced by this branch.
No new hardware FPS claim is made for the startup changes.

## Build

Clone this branch with its submodule:

```sh
git clone --branch amiga-renderfix-r10 --recurse-submodules https://github.com/arczi84/dethrace-68k.git
cd dethrace-68k
M68K_TOOLCHAIN_PATH=/opt/amiga \
AMIGA_SUPPORT_PATH=/path/to/Amiga_SDK/common \
DETHRACE_RENDERFIX_BUILD_DIR=/tmp/dethrace-renderfix-build \
sh tools/build-renderfix.sh
```

Prerequisites: CMake 3.20+, make, the Amiga GCC 6.5.0b toolchain, and the Amiga
support headers (including Warp3D/Warp3D.h, plus AmigaOS, AHI and CyberGraphX
headers if these are not already in the toolchain). AMIGA_SUPPORT_PATH points
to the directory containing those extra include/ files; it no longer needs
C2P objects in lib/.
The native Amiga build does not use POSIX libdl and no longer links libdl.a.

The three tested C2P objects are bundled in tools/c2p/lib and used by default.
Their public-domain assembly sources and rebuild instructions are in
[tools/c2p](../tools/c2p/README.md). Vasm is needed only to rebuild these objects,
not for a normal game build. To use a different C2P directory, set
AMIGA_C2P_PATH when running the build script or pass -DAMIGA_C2P_PATH to CMake.

The minimal MiniGL client SDK and its import archive are in
tools/pistorm3d-v12-sdk; the game uses an installed compatible minigl.library at
runtime.

The output is dethrace_opt in the build directory. Install it as dethrace in
your existing game directory. The script does not deploy automatically.
Original Carmageddon/Splat Pack data is required and is not included.

Release uses GCC 6.5.0b with effective -O3, -m68060 and -mhard-float. The trailing
-O3 overrides the toolchain's earlier -O1. See BUILD_BASELINE.md for the remaining
flags. GCC 16.2 was used separately for the Classic library's null-colour patch;
it is not the compiler used for this game build.

## Host blade regression checks

```sh
python3 tools/tests/blade-order/test_queue.py
LIBGL_ALWAYS_SOFTWARE=1 python3 tools/tests/blade-order/test_gl.py /path/to/CARMA/DATA/PIXELMAP/EAGLE1.PIX
```

The first test needs a host C compiler with ASan/UBSan. The second needs Mesa
surfaceless EGL and OpenGL development libraries. Neither launches an emulator.
Diagnostic perspective/alpha examples and historical patch scripts under tools/
are not part of the normal build; do not apply them to this snapshot.
