# Build baseline — 2026-09-21

Keep compiler/optimisation unchanged while fixing startup rendering.

## Game: user-confirmed rendering baseline R7

- Source: this R10 snapshot, based on the optimized q3batch tree.
- Compiler: /opt/amiga/bin/m68k-amigaos-gcc, GCC 6.5.0b 20260819091705.
- CMake Release. Effective optimisation: -O3 (appears after toolchain -O1).
- CPU/FPU: -m68060 -mhard-float.
- Other current code-generation flags: -fbbb=- -fno-strict-aliasing
  -fno-unroll-loops -fomit-frame-pointer; runtime -noixemul.
- No added -ffast-math or LTO for the game.
- Shared MiniGL dispatch; existing tools/pistorm3d-v12-sdk headers.
- Both experimental PiStorm ORIGINAL_Q_PATH and VERTEX4_PATH options OFF;
  backend detection selects the established path.
- Build with tools/build-renderfix.sh. Toolchain/support/build paths can be
  overridden; see docs/AMIGA_RENDERFIX.md. It does not auto-deploy.
- R7 has user-confirmed blade fix; prior cockpit/shadow confirmations retained.
  R8 onward modify startup only. Startup workarounds now run on Classic only.

This is a reproducible working baseline, not a benchmark proving globally best
compiler flags. No controlled GCC 6.5 versus GCC 16.2 game comparison exists.

## Separate MiniGL Classic g16f null-colour build

- /opt/amiga16-copy/bin/m68k-amigaos-gcc: GCC 16.2.0b 20260825082934.
- Existing library recipe uses -O3 -m68060 -mhard-float -mcrt=clib2,
  -ffast-math -fomit-frame-pointer -fno-strict-aliasing -fno-strict-overflow
  -fsigned-char -fno-baserel -fno-builtin.
- These library flags are not a recommendation to change the game flags.
- Null-colour patch changes draw.c and texture.c only. context.c is byte-for-byte
  identical to PiStorm3D_V25_Classic source used for the g16f baseline.
- Startup garbage reported with older EXEs too, on Classic/WinUAE but not
  PiStorm3D. A new library regression is not established by that alone.
