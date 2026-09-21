# Amiga MiniGL / PiStorm3D render fixes (R10)

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
support SDK. The support path must contain include/ and lib/ with
c2p1x1_4_c5_bm.o, c2p1x1_8_c5_bm_040.o and c2p1x1_6_c5_bm_040.o. These external
SDK objects are not game assets and are not bundled here. The minimal MiniGL
client SDK and its import archive are in tools/pistorm3d-v12-sdk; the game uses
an installed compatible minigl.library at runtime.

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
