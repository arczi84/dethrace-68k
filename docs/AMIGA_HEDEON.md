# Hedeon optimizations: R10 integration

Version: `0.10.1-amiga-r10-hedeon1`.

Imported from the Hedeon source snapshot supplied by the user on 2026-09-22
(`dethrace-68k-amiga-renderfix-hedeon`). The eight game source files and eight
BRender source files changed in that snapshot are preserved byte for byte.
The integration retains our bundled C2P objects, removal of the unused libdl
dependency, build script and GCC baseline. The version label distinguishes
this executable from the earlier R10 build. Credit for the imported
optimizations belongs to Hedeon.

## Included changes

- Skip unchanged HUD rows and segments; upload small changed tile regions on
  PiStorm3D, retaining full texture replacement on Classic.
- Use vertex arrays and indexed batches for eligible textured perspective
  draws, with shared-vertex lookup and cached client-array setup.
- Avoid redundant texture selection, conversion and uploads.
- Defer writes to the CPU depth-buffer copy until it is accessed.
- Group HUD dimming passes and combine masked HUD copying with overlay marking.
- Cache eligible flat-lighting results within material groups and bound the
  ambient-light scan using the current lighting state.
- Reset new GL caches when a context is created.

## Build and compare

Use [the R10 build instructions](AMIGA_RENDERFIX.md). The integrated build uses
GCC 6.5.0b, effective `-O3 -m68060 -mhard-float`, `-fbbb=-`,
`-fno-strict-aliasing -fno-unroll-loops -fomit-frame-pointer`, and `-noixemul`.
It does not enable `FXA_SHIM_STATS` or `FXA_AB_TEST`: the expensive measurement
and automatic A/B instrumentation is compiled out of the normal build.

The imported optimizations default to on. Append `--fxa-off=all` to existing
launch arguments to disable the named optional paths, or disable a subset,
for example `--fxa-off=arrays,indexed`. This is an in-build comparison, not a
byte-identical restoration of the old R10 renderer: shared refactoring such
as the HUD row pre-pass remains.

The Help key toggles the allowed options live. `--fxa-ab=names` restricts that
toggle to selected options. The available names are:

```text
segments arrays depthfill texhint subupload indexed texmerge fastkey
dimqueue markfuse texdedup cc4memo constmemo arrayptr lightbound
```

## Validation and limits

Both the supplied snapshot and the integrated source passed a clean Amiga
cross-build with the existing GCC 6.5.0b flags. Existing blade queue and Mesa
visibility regressions passed. The additional host test exercises the actual
deferred depth-fill functions against eager pixel writes:

```sh
python3 tools/tests/hedeon/test_depth_fill.py
```

It covers overlapping fills, queue overflow, odd widths, pointer exposure and
optimization toggles using ASan and UBSan (except host alignment checks, since
the 68k routine uses word-aligned longword accesses). It does not exercise the
GPU depth buffer or the full MiniGL renderer. The other imported optimizations
have not been validated by a full gameplay replay here.

No emulator was launched and no hardware FPS result is claimed. The earlier
Classic first-frame workaround still requires runtime confirmation.
