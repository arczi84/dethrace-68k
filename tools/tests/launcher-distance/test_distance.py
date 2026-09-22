"""Host checks of actual launcher persistence and camera-distance functions.

Uses small host fixtures; does not open GadTools or run MiniGL.
"""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[3]
launcher = (repo / "tools/launcher/dethrace_launcher.c").read_text()
depth = (repo / "src/DETHRACE/common/depth.c").read_text()


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    nesting = 1
    end = brace + 1
    while nesting:
        nesting += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


header = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define CFG_FILE "launcher-test.cfg"
typedef float br_scalar;
typedef struct { br_scalar yon_z; } br_camera;
typedef struct { void *type_data; } br_actor;
static br_camera cameras[2];
static br_actor actors[2] = {{&cameras[0]}, {&cameras[1]}};
static br_actor *gCamera_list[2] = { &actors[0], &actors[1] };
static br_scalar gCamera_yon, gYon_multiplier = 1.0f, sight_distance;
static struct { float draw_distance_multiplier; } harness_game_config;
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#define BR_ASIZE(x) COUNT_OF(x)
static void SetSightDistance(float yon) { sight_distance = yon; }
br_scalar GetCameraYon(void);
'''
config = launcher[launcher.index("typedef struct LauncherConfig"):
                  launcher.index("typedef struct Lang")]
config += launcher[launcher.index("static LauncherConfig cfg ="):
                   launcher.index("struct Library *IntuitionBase")]
code = header + config
for signature in ("static int clamp_int(", "static void load_config(", "static int save_config("):
    code += function(launcher, signature)
for signature in ("void SetYon(", "br_scalar GetYon(", "br_scalar GetCameraYon(",
                  "void AssertYons(", "br_scalar DepthCueingShiftToDistance("):
    code += function(depth, signature)
code += r'''
static void write_config(const char *text) {
    FILE *f = fopen(CFG_FILE, "w");
    assert(f); fputs(text, f); fclose(f);
}
int main(void) {
    const float factors[] = {1.0f, 1.5f, 2.0f, 3.0f};
    unsigned i, repeat;
    assert(cfg.draw_distance == 0);
    write_config("resolution=4\nsound_detail=2\n");
    load_config();
    assert(cfg.draw_distance == 0); /* old config keeps standard distance */
    assert(cfg.resolution == 4 && cfg.sound_detail == 2);
    for(i = 0; i < 4; i++) {
        cfg.draw_distance = i;
        assert(save_config());
        cfg.draw_distance = -1;
        load_config();
        assert(cfg.draw_distance == (int)i && cfg.resolution == 4);
    }
    write_config("draw_distance=-3\n"); load_config(); assert(cfg.draw_distance == 0);
    write_config("draw_distance=999\n"); load_config(); assert(cfg.draw_distance == 3);
    for(i = 0; i < 4; i++) {
        harness_game_config.draw_distance_multiplier = factors[i];
        SetYon(35.0f);
        for(repeat = 0; repeat < 10; repeat++) {
            SetYon(GetYon()); /* saving/restoring the base cannot compound scaling */
            assert(GetYon() == 35.0f);
            assert(GetCameraYon() == 35.0f * factors[i]);
            assert(cameras[0].yon_z == GetCameraYon());
            assert(cameras[1].yon_z == GetCameraYon());
            assert(DepthCueingShiftToDistance(0) == GetCameraYon());
        }
        gYon_multiplier = 1.25f;
        AssertYons();
        assert(cameras[0].yon_z == 35.0f * factors[i] * 1.25f);
        if(factors[i] > 1.0f) assert(sight_distance == cameras[0].yon_z);
        SetYon(20.0f); /* in-game menu still selects its original base value */
        assert(GetYon() == 20.0f && GetCameraYon() == 20.0f * factors[i]);
    }
    harness_game_config.draw_distance_multiplier = 0;
    SetYon(35.0f); assert(GetCameraYon() == 35.0f);
    harness_game_config.draw_distance_multiplier = NAN;
    assert(GetCameraYon() == 35.0f);
    gCamera_list[0] = gCamera_list[1] = NULL;
    SetYon(0.0f); assert(GetYon() == 5.0f); /* options before camera allocation */
    puts("PASS: launcher config compatibility, persistence and clamping; camera/fog scaling, track multiplier and menu round trips");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="dethrace-launcher-distance-") as tmp:
    path = Path(tmp)
    (path / "test.c").write_text(code)
    subprocess.run(["cc", "-std=c11", "-O2", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-no-pie", str(path / "test.c"),
                    "-lm", "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], cwd=path, check=True)
