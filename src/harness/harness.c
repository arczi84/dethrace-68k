#include "harness.h"
#include "ascii_tables.h"
#include "include/harness/config.h"
#include "include/harness/hooks.h"
#include "include/harness/os.h"
#include "ini.h"
#include "platforms/null.h"
#include "version.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

extern br_uint_32 gI_am_cheating;
extern int gAustere_override;
extern int gCar_simplification_level;
extern int gCut_scene_override;
extern int gReplay_override;
extern int gSound_override;
extern int gSound_detail_level;
extern int gSausage_override;
extern int gGraf_spec_index;

extern void Harness_Platform_Init(tHarness_platform* platform);

extern const tPlatform_bootstrap SDL1_bootstrap;
extern const tPlatform_bootstrap SDL2_bootstrap;
extern const tPlatform_bootstrap SDL3_bootstrap;
extern const tPlatform_bootstrap Amiga_bootstrap;

static const tPlatform_bootstrap* platform_bootstraps[] = {
#if defined(DETHRACE_PLATFORM_AMIGA)
    &Amiga_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#if defined(DETHRACE_PLATFORM_SDL3)
    &SDL3_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#if defined(DETHRACE_PLATFORM_SDL2)
    &SDL2_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#if defined(DETHRACE_PLATFORM_SDL1)
    &SDL1_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#ifndef HAS_PLATFORM_BOOTSTRAP
    // This is the case for MSVC 4.20 builds
    NULL
#endif
};

#define safe_strcpy(DEST, SRC) \
    do { \
        strncpy((DEST), (SRC), sizeof(DEST)); \
        (DEST)[sizeof(DEST) - 1] = '\0'; \
    } while (0)

// SplatPack or Carmageddon. This is where we represent the code differences between the two. For example, the intro smack file.
tHarness_game_info harness_game_info;

// Configuration options
tHarness_game_config harness_game_config;

// Platform hooks
tHarness_platform gHarness_platform;

static int force_null_platform = 0;

static int Harness_ProcessCommandLine(int* argc, char* argv[]);
static int Harness_ProcessIniFile(void);

static int Harness_GameDataExists(const char* path) {
    char marker[MAX_PATH];
    size_t length;
    const char* separator;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    length = strlen(path);
    separator = path[length - 1] == '/' || path[length - 1] == '\\' || path[length - 1] == ':' ? "" : "/";
    if (snprintf(marker, sizeof(marker), "%s%sDATA/GENERAL.TXT", path, separator) >= (int)sizeof(marker)) {
        return 0;
    }
    return access(marker, F_OK) == 0;
}

static int Harness_GetDemoFallbackPath(const char* path, char* fallback, size_t fallback_size) {
    const char* demo_dir;
    size_t end;
    size_t start;
    size_t name_length;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    end = strlen(path);
    while (end > 0 && (path[end - 1] == '/' || path[end - 1] == '\\')) {
        end--;
    }
    start = end;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\' && path[start - 1] != ':') {
        start--;
    }
    name_length = end - start;

    if (name_length == 5 && strncasecmp(path + start, "CARMA", name_length) == 0) {
        demo_dir = "CARMDEMO";
    } else if (name_length == 8 && strncasecmp(path + start, "CARSPLAT", name_length) == 0) {
        demo_dir = "SPLATDEMO";
    } else {
        return 0;
    }

    if (snprintf(fallback, fallback_size, "%.*s%s", (int)start, path, demo_dir) >= (int)fallback_size) {
        return 0;
    }
    return 1;
}

static int Harness_ClampInt(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int Harness_ProcessLauncherConfigFile(const char* argv0) {
    static const int fps_values[] = { 0, 25, 30, 50, 60 };
    char config_path[MAX_PATH];
    char game_path[MAX_PATH];
    char line[128];
    char key[64];
    const char* program_dir;
    const char* game_dir = "CARMA";
    FILE* f;
    int game = 0;
    int renderer = 0;
    int resolution = 0;
    int display = 0;
    int fps = 0;
    int show_fps = 0;
    int windowed = 0;
    int sound = 1;
    int sound_options = 0;
    int skip_cutscenes = 0;
    int replay = 1;
    int lowmem = 0;
    int car_detail = 0;
    int sound_detail = 1;
    char cd_device[64] = "scsi.device";
    int cd_unit = 0;
    char text_value[64];
    int value;
    size_t program_dir_length;
    const char* separator;

    program_dir = OS_GetWorkingDirectory((char*)argv0);
    if (program_dir == NULL) {
        fprintf(stderr, "Unable to locate dethrace-launcher.cfg\n");
        return 1;
    }

    program_dir_length = strlen(program_dir);
    separator = program_dir_length > 0
            && (program_dir[program_dir_length - 1] == '/'
                || program_dir[program_dir_length - 1] == '\\'
                || program_dir[program_dir_length - 1] == ':')
        ? ""
        : "/";
    if (snprintf(config_path, sizeof(config_path), "%s%sdethrace-launcher.cfg",
            program_dir, separator) >= (int)sizeof(config_path)) {
        fprintf(stderr, "Path to dethrace-launcher.cfg is too long\n");
        return 1;
    }

    f = fopen(config_path, "r");
    if (f == NULL) {
        fprintf(stderr, "Unable to open %s: %s\n", config_path, strerror(errno));
        return 1;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, " %63[^=]=%63[^\r\n]", key, text_value) != 2) {
            continue;
        }
        if (strcasecmp(key, "cd_device") == 0) {
            safe_strcpy(cd_device, text_value);
            continue;
        }
        value = atoi(text_value);
        if (strcasecmp(key, "game") == 0) game = value;
        else if (strcasecmp(key, "renderer") == 0) renderer = value;
        else if (strcasecmp(key, "resolution") == 0) resolution = value;
        else if (strcasecmp(key, "display") == 0) display = value;
        else if (strcasecmp(key, "fps") == 0) fps = value;
        else if (strcasecmp(key, "show_fps") == 0) show_fps = value;
        else if (strcasecmp(key, "windowed") == 0) windowed = value;
        else if (strcasecmp(key, "sound") == 0) sound = value;
        else if (strcasecmp(key, "sound_options") == 0) sound_options = value;
        else if (strcasecmp(key, "skip_cutscenes") == 0) skip_cutscenes = value;
        else if (strcasecmp(key, "replay") == 0) replay = value;
        else if (strcasecmp(key, "lowmem") == 0) lowmem = value;
        else if (strcasecmp(key, "car_detail") == 0) car_detail = value;
        else if (strcasecmp(key, "sound_detail") == 0) sound_detail = value;
        else if (strcasecmp(key, "cd_unit") == 0) cd_unit = value;
    }
    fclose(f);

    game = Harness_ClampInt(game, 0, 3);
    renderer = !!renderer;
    resolution = !!resolution;
    display = Harness_ClampInt(display, 0, 3);
    fps = Harness_ClampInt(fps, 0, 4);

    if (game == 1) {
        game_dir = "CARSPLAT";
    } else if (game == 2) {
        game_dir = "CARMDEMO";
    } else if (game == 3) {
        game_dir = "SPLATDEMO";
    }
    if (snprintf(game_path, sizeof(game_path), "%s%s%s",
            program_dir, separator, game_dir) >= (int)sizeof(game_path)) {
        fprintf(stderr, "Configured game path is too long\n");
        return 1;
    }

    safe_strcpy(harness_game_config.selected_dir, game_path);
    harness_game_config.opengl_3dfx_mode = renderer;
    /* The original Carmageddon demo has no DATA/64X48X8 set.  The old
     * launcher omitted -hires for this exact combination. */
    gGraf_spec_index = game == 2 && !renderer ? 0 : resolution;
    harness_game_config.bpp = display == 2 ? 6 : 8;
    harness_game_config.aga_screen = display == 1;
    harness_game_config.custom_screen = display == 3;
    harness_game_config.fps = fps_values[fps];
    harness_game_config.show_fps = !!show_fps;
    harness_game_config.start_full_screen = !windowed;
    gSound_override = !sound;
    harness_game_config.sound_options = !!sound_options;
    safe_strcpy(harness_game_config.cd_device, cd_device);
    harness_game_config.cd_unit = Harness_ClampInt(cd_unit, 0, 255);
    gCut_scene_override = !!skip_cutscenes;
    gReplay_override = !replay;
    gAustere_override = !!lowmem;
    gCar_simplification_level = Harness_ClampInt(car_detail, 0, 4);
    gSound_detail_level = Harness_ClampInt(sound_detail, 0, 2);

    printf("Using launcher config: %s\n", config_path);
    return 0;
}

static int Harness_InitPlatform(void) {
    int required_caps = 0;

    if (force_null_platform) {
        Null_Platform_Init(&gHarness_platform);
    } else {
        const tPlatform_bootstrap* selected_bootstrap = NULL;

        if (harness_game_config.opengl_3dfx_mode) {
            required_caps &= ~ePlatform_cap_video_mask;
            required_caps |= ePlatform_cap_opengl;
        }

        if (strlen(harness_game_config.platform_name) != 0) {
            size_t i;
            for (i = 0; i < BR_ASIZE(platform_bootstraps); i++) {
                if (strcasecmp(platform_bootstraps[i]->name, harness_game_config.platform_name) == 0) {
                    if ((platform_bootstraps[i]->capabilities & required_caps) != required_caps) {
                        fprintf(stderr, "Platform \"%s\" does not support required capabilities. Try another video driver and/or add/remove --opengl\n", platform_bootstraps[i]->name);
                        return 1;
                    }
                    selected_bootstrap = platform_bootstraps[i];
                    break;
                }
            }
            if (selected_bootstrap == NULL) {
                fprintf(stderr, "Could not find a platform named \"%s\"\n", harness_game_config.platform_name);
                return 1;
            }
            if (selected_bootstrap->init(&gHarness_platform) != 0) {
                fprintf(stderr, "%s initialization failed\n", selected_bootstrap->name);
                return 1;
            }
        } else {
            size_t i;
            for (i = 0; i < BR_ASIZE(platform_bootstraps); i++) {
                LOG_DEBUG2("Attempting video driver \"%s\"", platform_bootstraps[i]->name);
                if ((platform_bootstraps[i]->capabilities & required_caps) != required_caps) {
                    fprintf(stderr, "Skipping platform \"%s\". Does not support required capabilities.\n", platform_bootstraps[i]->name);
                    continue;
                }
                LOG_DEBUG2("Try platform \"%s\"...", platform_bootstraps[i]->name);
                if (platform_bootstraps[i]->init(&gHarness_platform) == 0) {
                    selected_bootstrap = platform_bootstraps[i];
                    break;
                }
            }
            if (selected_bootstrap == NULL) {
                fprintf(stderr, "Could not find a supported platform\n");
                return 1;
            }
        }
        if (selected_bootstrap == NULL) {
            fprintf(stderr, "Could not find a supported platform\n");
            return 1;
        }
        LOG_INFO3("Platform: %s (%s)", selected_bootstrap->name, selected_bootstrap->description);
    }

    return 0;
}

static void Harness_DetectGameMode(void) {
    FILE* f;
    int filesize;
    char* buffer;
    int nb;

    if (access("DATA/RACES/CASTLE.TXT", F_OK) != -1) {
        // All splatpack edition have the castle track
        if (access("DATA/RACES/CASTLE2.TXT", F_OK) != -1) {
            // Only the full splat release has the castle2 track
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
            harness_game_info.defines.GERMAN_LOADSCRN = "LOADSCRN.PIX";
            harness_game_info.mode = eGame_splatpack;
            printf("Game mode: Splat Pack\n");
        } else if (access("DATA/RACES/TINSEL.TXT", F_OK) != -1) {
            // Only the the splat x-mas demo has the tinsel track
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
            harness_game_info.defines.GERMAN_LOADSCRN = "";
            harness_game_info.mode = eGame_splatpack_xmas_demo;
            printf("Game mode: Splat Pack X-mas demo\n");
        } else {
            // Assume we're using the splatpack demo
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
            harness_game_info.defines.GERMAN_LOADSCRN = "";
            harness_game_info.mode = eGame_splatpack_demo;
            printf("Game mode: Splat Pack demo\n");
        }
    } else if (access("DATA/RACES/CITYB3.TXT", F_OK) != -1) {
        // All non-splatpack edition have the cityb3 track
        if (access("DATA/RACES/CITYA1.TXT", F_OK) == -1) {
            // The demo does not have the citya1 track
            harness_game_info.defines.INTRO_SMK_FILE = "";
            harness_game_info.defines.GERMAN_LOADSCRN = "COWLESS.PIX";
            harness_game_info.mode = eGame_carmageddon_demo;
            printf("Game mode: Carmageddon demo\n");
        } else {
            goto carmageddon;
        }
    } else {
    carmageddon:
        if (access("DATA/CUTSCENE/Mix_intr.smk", F_OK) == -1) {
            harness_game_info.defines.INTRO_SMK_FILE = "Mix_intr.smk";
        } else {
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
        }
        harness_game_info.defines.GERMAN_LOADSCRN = "LOADSCRN.PIX";
        harness_game_info.mode = eGame_carmageddon;
        printf("Game mode: Carmageddon\n");
    }

    harness_game_info.localization = eGameLocalization_none;
    if (access("DATA/TRNSLATE.TXT", F_OK) != -1) {
        f = fopen("DATA/TRNSLATE.TXT", "rb");
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(filesize + 1);
        nb = fread(buffer, 1, filesize, f);
        if (nb != filesize) {
            LOG_PANIC("Unable to read DATA/TRNSLATE.TXT");
        }
        buffer[filesize] = '\0';
        fclose(f);
        if (strstr(buffer, "NEUES SPIEL") != NULL) {
            harness_game_info.localization = eGameLocalization_german;
            LOG_INFO2("Language: \"%s\"", "German");
        } else if (strstr(buffer, "NOWA GRA") != NULL) {
            harness_game_info.localization = eGameLocalization_polish;
            LOG_INFO2("Language: \"%s\"", "Polish");
        } else if (strstr(buffer, "NOUVELLE PARTIE") != NULL) {
            harness_game_info.localization = eGameLocalization_french;
            LOG_INFO2("Language: \"%s\"", "French");
        } else {
            LOG_INFO("Language: unrecognized");
        }
        free(buffer);
    }

    switch (harness_game_info.mode) {
    case eGame_carmageddon:
        switch (harness_game_info.localization) {
        case eGameLocalization_german:
            harness_game_info.defines.requires_ascii_table = 1;
            harness_game_info.defines.ascii_table = carmageddon_german_ascii_tables.ascii;
            harness_game_info.defines.ascii_shift_table = carmageddon_german_ascii_tables.ascii_shift;
            break;
        default:
            harness_game_info.defines.ascii_table = carmageddon_ascii_tables.ascii;
            harness_game_info.defines.ascii_shift_table = carmageddon_ascii_tables.ascii_shift;
            break;
        }
        break;
    case eGame_carmageddon_demo:
        harness_game_info.defines.ascii_table = demo_ascii_tables.ascii;
        harness_game_info.defines.ascii_shift_table = demo_ascii_tables.ascii_shift;
        break;
    case eGame_splatpack_demo:
    case eGame_splatpack_xmas_demo:
        harness_game_info.defines.ascii_table = xmas_ascii_tables.ascii;
        harness_game_info.defines.ascii_shift_table = xmas_ascii_tables.ascii_shift;
        break;
    default:
        break;
    }

    // 3dfx code paths require at least smoke.pix which is used instead of writing smoke directly to framebuffer
    if (access("DATA/PIXELMAP/SMOKE.PIX", F_OK) != -1) {
        harness_game_info.data_dir_has_3dfx_assets = 1;
    }
}

void Harness_DetectAndSetWorkingDirectory(char* argv0) {
    char* path;
    char* env_var;
    char pref_path[MAX_PATH];
    char fallback_path[MAX_PATH];

    env_var = getenv("DETHRACE_ROOT_DIR");

    if (strlen(harness_game_config.selected_dir) > 0) {
        path = harness_game_config.selected_dir;
    } else if (env_var != NULL) {
        LOG_INFO2("DETHRACE_ROOT_DIR is set to '%s'", env_var);
        path = env_var;
    } else {
        path = OS_GetWorkingDirectory(argv0);
        if (access("DATA/GENERAL.TXT", F_OK) == 0) {
            // good, found
        } else {
            OS_GetPrefPath(pref_path, "dethrace");
            path = pref_path;
        }
    }

    if (path != NULL
        && strlen(harness_game_config.selected_dir) > 0
        && !Harness_GameDataExists(path)
        && Harness_GetDemoFallbackPath(path, fallback_path, sizeof(fallback_path))
        && Harness_GameDataExists(fallback_path)) {
        printf("Game data not found in %s; trying %s\n", path, fallback_path);
        safe_strcpy(harness_game_config.selected_dir, fallback_path);
        path = harness_game_config.selected_dir;
    }

    // if root_dir is null or empty, no need to chdir
    if (path != NULL && path[0] != '\0') {
        printf("Using game directory: %s\n", path);
        if (chdir(path) != 0) {
            LOG_PANIC2("Failed to chdir. Error is %s", strerror(errno));
        }
    }
}

int Harness_Init(int* argc, char* argv[]) {

    printf("Dethrace version: %s\n", DETHRACE_VERSION);

    memset(&harness_game_info, 0, sizeof(harness_game_info));

    // disable the original CD check code
    harness_game_config.enable_cd_check = 0;
    // original physics time step. Lower values seem to work better at 30+ fps
    harness_game_config.physics_step_time = 40;
#ifdef AMIGA
    // AmigaOS Delay() has a 20 ms minimum step, so a 60 FPS limiter would
    // stall otherwise fast frames. Match the stable Amiga port and run unlocked.
    harness_game_config.fps = 0;
    harness_game_config.show_fps = 0;
#else
    // limit to 60 fps by default
    harness_game_config.fps = 60;
    harness_game_config.show_fps = 0;
#endif
    // do not freeze timer
    harness_game_config.freeze_timer = 0;
    // default demo time out is 240s
    harness_game_config.demo_timeout = 240000;
    // disable developer diagnostics by default
    harness_game_config.enable_diagnostics = 0;
    // no savegame auto-load: start at the logos/main menu as usual
    harness_game_config.load_slot = -1;
    // no volume multiplier
    harness_game_config.volume_multiplier = 1.0f;
    // start window in windowed mode
    harness_game_config.start_full_screen = 1;
    // Disable gore check emulation
    harness_game_config.gore_check = 0;
    // Disable "Sound Options" menu
    harness_game_config.sound_options = 0;
    // Skip binding socket to allow local network testing
    harness_game_config.no_bind = 0;
    // Disable verbose logging
    harness_game_config.verbose = 0;
    // Amiga native video defaults to CGX 8-bit. --aga has priority over --bpp.
    harness_game_config.bpp = 8;
    harness_game_config.aga_screen = 0;
    harness_game_config.custom_screen = 0;
    safe_strcpy(harness_game_config.cd_device, "scsi.device");
    harness_game_config.cd_unit = 0;

    // install signal handler
    harness_game_config.install_signalhandler = 1;

    // first load ini file if exists
    Harness_ProcessIniFile();

    // then override any config options with command line args
    if (Harness_ProcessCommandLine(argc, argv) != 0) {
        fprintf(stderr, "Failed to parse harness command line\n");
        return 1;
    }

    // now resolve the platform
    if (Harness_InitPlatform() != 0) {
        return 1;
    }

    if (harness_game_config.install_signalhandler) {
        OS_InstallSignalHandler(argv[0]);
    }

    Harness_DetectAndSetWorkingDirectory(argv[0]);

    if (harness_game_info.mode == eGame_none) {
        Harness_DetectGameMode();
    }

    if (harness_game_config.opengl_3dfx_mode && !harness_game_info.data_dir_has_3dfx_assets) {
        printf("Error: data directory does not contain 3dfx assets so opengl mode cannot be used\n");
        exit(1);
    }

    return 0;
}

void Harness_Quit(void) {

    if (harness_game_config.install_signalhandler) {
        OS_RemoveSignalHandler();
    }
}

// used by unit tests
void Harness_ForceNullPlatform(void) {
    force_null_platform = 1;
}

int Harness_ProcessCommandLine(int* argc, char* argv[]) {
    int i, j;
    char* val;
    char explicit_dir[MAX_PATH] = "";
    for (i = 1; i < *argc;) {
        int consumed = -1;

        if (strcasecmp(argv[i], "--use-cfg") == 0) {
            if (Harness_ProcessLauncherConfigFile(argv[0]) != 0) {
                return 1;
            }
            /* A direct launch script may use the shared launcher settings for
             * graphics and sound while selecting its own game with --dir. */
            if (explicit_dir[0] != '\0') {
                safe_strcpy(harness_game_config.selected_dir, explicit_dir);
            }
            consumed = 1;
        } else if (strcasecmp(argv[i], "--game") == 0) {
            if (i < *argc + 1) {
                val = argv[i + 1];
                for (i = 0; i < harness_game_config.game_dirs_count; i++) {
                    if (strcmp(harness_game_config.game_dirs[i].name, val) == 0) {
                        safe_strcpy(harness_game_config.selected_dir, harness_game_config.game_dirs[i].directory);
                        break;
                    }
                }
                if (i == harness_game_config.game_dirs_count) {
                    LOG_PANIC2("Failed to find game %s", val);
                }
                consumed = 2;
            }
        } else if (strcasecmp(argv[i], "--dir") == 0) {
            if (i < *argc + 1) {
                safe_strcpy(harness_game_config.selected_dir, argv[i + 1]);
                safe_strcpy(explicit_dir, argv[i + 1]);
                consumed = 2;
            }
        } else if (strcasecmp(argv[i], "--cdcheck") == 0) {
            harness_game_config.enable_cd_check = 1;
            consumed = 1;
        } else if (strstr(argv[i], "--debug=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_debug_level = atoi(s + 1);
            LOG_INFO2("debug level set to %d", harness_debug_level);
            consumed = 1;
        } else if (strstr(argv[i], "--physics-step-time=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.physics_step_time = atoi(s + 1);
            LOG_INFO2("Physics step time set to %d", harness_game_config.physics_step_time);
            consumed = 1;
        } else if (strstr(argv[i], "--fps=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.fps = atoi(s + 1);
            LOG_INFO2("FPS limiter set to %f", harness_game_config.fps);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--show-fps") == 0) {
            harness_game_config.show_fps = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--freeze-timer") == 0) {
            LOG_INFO("Timer frozen");
            harness_game_config.freeze_timer = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--no-signal-handler") == 0) {
            LOG_INFO("Don't install the signal handler");
            harness_game_config.install_signalhandler = 0;
            consumed = 1;
        } else if (strstr(argv[i], "--load=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.load_slot = atoi(s + 1);
            LOG_INFO2("Loading savegame slot %d, skipping the main menu", harness_game_config.load_slot);
            consumed = 1;
        } else if (strstr(argv[i], "--demo-timeout=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.demo_timeout = atoi(s + 1) * 1000;
            LOG_INFO2("Demo timeout set to %d milliseconds", harness_game_config.demo_timeout);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--i-am-cheating") == 0) {
            gI_am_cheating = 0xa11ee75d;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--enable-diagnostics") == 0) {
            harness_game_config.enable_diagnostics = 1;
            consumed = 1;
        } else if (strstr(argv[i], "--volume-multiplier=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.volume_multiplier = atof(s + 1);
            LOG_INFO2("Volume multiplier set to %f", harness_game_config.volume_multiplier);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--full-screen") == 0) {
            // option left for backwards compatibility
            harness_game_config.start_full_screen = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--window") == 0) {
            harness_game_config.start_full_screen = 0;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--gore-check") == 0) {
            harness_game_config.gore_check = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--sound-options") == 0) {
            harness_game_config.sound_options = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--opengl") == 0) {
            harness_game_config.opengl_3dfx_mode = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--game-completed") == 0) {
            harness_game_config.game_completed = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--no-bind") == 0) {
            harness_game_config.no_bind = 1;
            consumed = 1;
        } else if (strstr(argv[i], "--network-adapter-name") != NULL) {
            if (i < *argc + 1) {
                safe_strcpy(harness_game_config.network_adapter_name, argv[i + 1]);
                consumed = 2;
            }
        } else if (strcasecmp(argv[i], "--platform") == 0) {
            if (i < *argc + 1) {
                safe_strcpy(harness_game_config.platform_name, argv[i + 1]);
                consumed = 2;
            }
        } else if (strstr(argv[i], "--bpp=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.bpp = atoi(s + 1);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--aga") == 0) {
            harness_game_config.aga_screen = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--ask") == 0) {
            harness_game_config.custom_screen = 1;
            consumed = 1;
        }

        if (consumed > 0) {
            // shift args downwards
            for (j = i; j < *argc - consumed; j++) {
                argv[j] = argv[j + consumed];
            }
            *argc -= consumed;
        } else {
            i += 1;
        }
    }

    return 0;
}

static int Harness_Ini_Callback(void* user, const char* section, const char* name, const char* value) {
    int i;
    float f;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
#define MATCH_NAME(s, n) strcmp(name, s) == 0

    if (strcmp(section, "Games") == 0) {
        safe_strcpy(harness_game_config.game_dirs[harness_game_config.game_dirs_count].name, name);
        safe_strcpy(harness_game_config.game_dirs[harness_game_config.game_dirs_count].directory, value);
        harness_game_config.game_dirs_count++;
    }

    else if (MATCH("General", "CdCheck")) {
        harness_game_config.enable_cd_check = (value[0] == '1');
    } else if (MATCH("General", "GoreCheck")) {
        harness_game_config.gore_check = (value[0] == '1');
    } else if (MATCH("General", "FPSLimit")) {
        i = atoi(value);
        harness_game_config.fps = i;
    } else if (MATCH("General", "DemoTimeout")) {
        i = atoi(value);
        harness_game_config.demo_timeout = i * 1000;
    } else if (MATCH("General", "Windowed")) {
        harness_game_config.start_full_screen = (value[0] == '0');
    } else if (MATCH("General", "Emulate3DFX")) {
        harness_game_config.opengl_3dfx_mode = (value[0] == '1');
    } else if (MATCH("General", "DefaultGame")) {
        safe_strcpy(harness_game_config.default_game, value);
    } else if (MATCH("General", "BoringMode")) {
        gSausage_override = (value[0] == '1');
    } else if (MATCH("General", "Hires")) {
        gGraf_spec_index = (value[0] == '1');
    }

    else if (MATCH("Cheats", "EditMode")) {
        if (value[0] == '1') {
            gI_am_cheating = 0xa11ee75d;
        }
    } else if (MATCH("Cheats", "FreezeTimer")) {
        harness_game_config.freeze_timer = (value[0] == '1');
    } else if (MATCH("Cheats", "GameCompleted")) {
        harness_game_config.game_completed = (value[0] == '1');
    }

    else if (MATCH("Sound", "Enabled")) {
        // gSound_override=1 means sound is disabled
        gSound_override = (value[0] == '0');
    } else if (MATCH("Sound", "SoundOptionsScreen")) {
        harness_game_config.sound_options = (value[0] == '1');
    } else if (MATCH("Sound", "VolumeMultiplier")) {
        f = atof(value);
        harness_game_config.volume_multiplier = f;
    }

    else if (MATCH("Network", "AdapterName")) {
        safe_strcpy(harness_game_config.network_adapter_name, value);
    }

    else if (MATCH("Developers", "Diagnostics")) {
        harness_game_config.enable_diagnostics = (value[0] == '1');
    } else if (MATCH("Developers", "PhysicsStepTime")) {
        i = atoi(value);
        harness_game_config.physics_step_time = i;
    } else if (MATCH("Developers", "InstallSignalHandler")) {
        harness_game_config.install_signalhandler = (value[0] == '1');
    }

    else {
        // unknown section/name
    }

    return 1;
}

int Harness_ProcessIniFile(void) {
    int i;
    char path[1024];

    OS_GetPrefPath(path, "dethrace");
    strcat(path, "dethrace.ini");
    LOG_INFO2("Loading ini file %s", path);

    if (ini_parse(path, Harness_Ini_Callback, NULL) < 0) {
        LOG_DEBUG2("Failed to load config file %s", path);
        return 1;
    }

    // set default game dir
    if (harness_game_config.game_dirs_count > 0) {
        if (strlen(harness_game_config.default_game) > 0) {
            for (i = 0; i < harness_game_config.game_dirs_count; i++) {
                if (strcmp(harness_game_config.game_dirs[i].name, harness_game_config.default_game) == 0) {
                    safe_strcpy(harness_game_config.selected_dir, harness_game_config.game_dirs[i].directory);
                    break;
                }
            }
            // if no default found, and we have some game dirs, default to first one
            if (i == harness_game_config.game_dirs_count) {
                safe_strcpy(harness_game_config.selected_dir, harness_game_config.game_dirs[i].directory);
            }
        }
    }

    return 0;
}

// Filesystem hooks
FILE* Harness_Hook_fopen(const char* pathname, const char* mode) {
    return OS_fopen(pathname, mode);
}

// Localization
int Harness_Hook_isalnum(int c) {
    int i;
    if (harness_game_info.localization == eGameLocalization_polish) {
        // Polish diacritic letters in Windows-1250
        unsigned char letters[] = { 140, 143, 156, 159, 163, 165, 175, 179, 185, 191, 198, 202, 209, 211, 230, 234, 241, 243 };
        for (i = 0; i < (int)sizeof(letters); i++) {
            if ((unsigned char)c == letters[i]) {
                return 1;
            }
        }
    }
    if (harness_game_info.localization == eGameLocalization_french) {
        // French diacritic letters in Windows-1252
        unsigned char letters[] = { 140, 156, 159, 192, 194, 198, 199, 200, 201, 202, 203, 206, 207, 212, 217, 219, 220, 224, 226, 230, 231, 232, 233, 234, 235, 238, 239, 244, 249, 251, 252, 255 };
        for (i = 0; i < (int)sizeof(letters); i++) {
            if ((unsigned char)c == letters[i]) {
                return 1;
            }
        }
    }

    return isalnum(c);
}
