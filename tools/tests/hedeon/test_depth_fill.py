"""Compare the actual deferred CPU-depth fill queue with eager pixel writes.

Host fixtures exercise queue ordering, capacity, pointer exposure and toggles.
This does not test GPU depth, MiniGL or the full game. Alignment sanitizer is
disabled because the 68k routine intentionally writes longwords at word-aligned
addresses; ASan and the remaining UBSan checks are enabled.
"""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[3]
source = (repo / "lib/BRender-v1.3.2/drivers/3dfx_amiga/glide_shim.c").read_text()
declarations = source[source.index("#define FXA_DEPTH_FILLS 8"):
                      source.index("static GLuint lfb_tile_textures")]
functions = source[source.index("static void apply_depth_fill("):
                   source.index("void guFbReadRegion(")]
header = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t FxU32;
typedef uint16_t FxU16;
typedef int FxBool;
typedef int GrBuffer_t;
#define FXFALSE 0
#define FXTRUE 1
#define GR_BUFFER_DEPTHBUFFER 1
#define FXA_LFB_STRIDE_PIXELS 32
#define FXA_OPT_DEPTHFILL 4
#define FXA_STAT(x) ((void)0)
static int enabled = 1;
#define FXA_OPT(x) enabled
static FxU16 actual[32 * 16], expected[32 * 16];
static FxU16 *lfb_depth = actual, *lfb_colour;
'''
test = r'''
static uint32_t seed = 91419;
static uint32_t rnd(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
/* Legacy rectangleFill pairs use addition, not OR. Preserve that even for
   colours such as 0xffffffff; use pixel writes independently of the queue. */
static void eager(int x, int y, int w, int h, FxU32 colour) {
    FxU32 packed = colour + (colour << 16);
    FxU16 pair[2];
    int r, c;
    memcpy(pair, &packed, sizeof(pair));
    for(r = 0; r < h; ++r)
        for(c = 0; c < w; ++c)
            expected[(y + r) * 32 + x + c] =
                ((w & 1) && c == w - 1) ? (FxU16)colour : pair[c & 1];
}
static void fill(int x, int y, int w, int h, FxU32 colour) {
    eager(x, y, w, h, colour);
    FXA_LfbDeferDepthFill(x, y, w, h, colour);
    assert(depth_fill_count <= FXA_DEPTH_FILLS);
}
static void check(void) {
    assert(grLfbGetReadPtr(GR_BUFFER_DEPTHBUFFER) == (const FxU32 *)actual);
    assert(depth_fill_count == 0 && depth_pointer_live);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
}
int main(void) {
    int i;
    fill(0, 0, 32, 16, 0xffffffffu);
    fill(0, 0, 32, 16, 0x12345678u);
    assert(depth_fill_count == 1); /* later full overwrite discards old fill */
    check();
    depth_pointer_live = FXFALSE; /* fully released LFB lock */
    for(i = 0; i < 9; ++i)
        fill(i, 0, 1, 1, (FxU32)i);
    assert(depth_fill_count == 1); /* full queue flushed before ninth fill */
    check();
    depth_pointer_live = FXFALSE;
    for(i = 0; i < 10000; ++i) {
        int x = rnd() % 32, y = rnd() % 16;
        int w = rnd() % (33 - x), h = rnd() % (17 - y);
        if(i % 31 == 0) enabled = !enabled;
        fill(x, y, w, h, rnd());
        if(depth_pointer_live || (!enabled && w > 0 && h > 0))
            assert(memcmp(actual, expected, sizeof(actual)) == 0);
        if(i % 19 == 0) {
            check();
            assert(grLfbGetWritePtr(GR_BUFFER_DEPTHBUFFER) == actual);
            actual[37] = expected[37] = 0x4567;
        }
        if(i % 23 == 0) depth_pointer_live = FXFALSE;
    }
    check();
    puts("PASS: deferred depth fills match eager writes across overlaps, overflow, odd widths, live pointers and optimization toggles");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="dethrace-hedeon-depth-") as tmp:
    path = Path(tmp)
    (path / "test.c").write_text(header + declarations + functions + test)
    subprocess.run(["cc", "-std=c11", "-O2", "-fno-strict-aliasing",
                    "-fsanitize=address,undefined", "-fno-sanitize=alignment",
                    "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
                    "-no-pie", str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
