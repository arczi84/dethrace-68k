/*
 * DethraceLauncher - small AmigaOS Intuition/GadTools launcher.
 *
 * Build:
 *   m68k-amigaos-gcc -m68020 -O2 -noixemul -o DethraceLauncher dethrace_launcher.c
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <libraries/locale.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/locale_protos.h>
#include <dos/dostags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFG_FILE "dethrace-launcher.cfg"
#define WIN_W 620
#define WIN_H 368
#define LEFT_X 12
#define RIGHT_X 315
#define LABEL_W 142
#define CONTROL_W 148
#define ROW_H 34
#define GADGET_H 20

enum {
    GID_GAME = 1,
    GID_RENDERER,
    GID_RESOLUTION,
    GID_DISPLAY,
    GID_FPS,
    GID_SHOW_FPS,
    GID_CAR_DETAIL,
    GID_SOUND_DETAIL,
    GID_WINDOW,
    GID_SOUND,
    GID_SOUND_OPTIONS,
    GID_SKIP_CUTSCENES,
    GID_REPLAY,
    GID_LOWMEM,
    GID_CD_DEVICE,
    GID_CD_UNIT,
    GID_SAVE,
    GID_PLAY,
    GID_QUIT,
    GID_COUNT
};

typedef struct LauncherConfig {
    int game;
    int renderer;
    int resolution;
    int display;
    int fps;
    int show_fps;
    int windowed;
    int sound;
    int sound_options;
    int skip_cutscenes;
    int replay;
    int lowmem;
    int car_detail;
    int sound_detail;
    char cd_device[64];
    int cd_unit;
} LauncherConfig;

typedef struct Lang {
    const char *title;
    const char *game;
    const char *renderer;
    const char *resolution;
    const char *display;
    const char *fps;
    const char *show_fps;
    const char *car_detail;
    const char *sound_detail;
    const char *windowed;
    const char *sound;
    const char *sound_options;
    const char *skip_cutscenes;
    const char *replay;
    const char *lowmem;
    const char *cd_device;
    const char *cd_unit;
    const char *save;
    const char *play;
    const char *quit;
    const char *saved;
} Lang;

static const Lang lang_pl = {
    "Dethrace - ustawienia",
    "Gra:",
    "Renderer:", "Rozdzielczosc:", "Ekran (software):", "Limit FPS:", "Pokaz FPS:",
    "Detale aut:", "Detale dzwieku:",
    "W oknie:", "Dzwiek:", "Menu opcji dzwieku:",
    "Pomin filmy:", "Action Replay:", "Tryb malej pamieci:",
    "Urzadzenie CD:", "Unit CD:",
    "Zapisz", "Graj!", "Wyjscie", "Dethrace - zapisano"
};

static const Lang lang_en = {
    "Dethrace - settings",
    "Game:",
    "Renderer:", "Resolution:", "Display (software):", "FPS limit:", "Show FPS:",
    "Car detail:", "Sound detail:",
    "Windowed:", "Sound:", "Sound options menu:",
    "Skip cutscenes:", "Action Replay:", "Low-memory mode:",
    "CD device:", "CD unit:",
    "Save", "Play!", "Quit", "Dethrace - saved"
};

static const Lang *L = &lang_pl;

static STRPTR game_names[] = {
    (STRPTR)"Carmageddon", (STRPTR)"Splat Pack", (STRPTR)"Demo",
    (STRPTR)"Splat Demo", NULL
};
static STRPTR renderer_names[] = { (STRPTR)"Software", (STRPTR)"MiniGL", NULL };
static STRPTR resolution_names[] = {
    (STRPTR)"320x200", (STRPTR)"640x480", (STRPTR)"800x600",
    (STRPTR)"1024x768", (STRPTR)"1280x720", (STRPTR)"1280x1024",
    (STRPTR)"1366x768", (STRPTR)"1600x900", (STRPTR)"1920x1080", NULL
};
static STRPTR display_names[] = {
    (STRPTR)"CyberGraphX 8-bit", (STRPTR)"AGA", (STRPTR)"HAM6", (STRPTR)"Wybierz...", NULL
};
static STRPTR display_names_en[] = {
    (STRPTR)"CyberGraphX 8-bit", (STRPTR)"AGA", (STRPTR)"HAM6", (STRPTR)"Choose...", NULL
};
static STRPTR fps_names[] = {
    (STRPTR)"Bez limitu", (STRPTR)"25", (STRPTR)"30", (STRPTR)"50", (STRPTR)"60", NULL
};
static STRPTR fps_names_en[] = {
    (STRPTR)"Unlocked", (STRPTR)"25", (STRPTR)"30", (STRPTR)"50", (STRPTR)"60", NULL
};
static STRPTR car_detail_names[] = {
    (STRPTR)"Najwyzsze", (STRPTR)"Wysokie", (STRPTR)"Srednie", (STRPTR)"Niskie", (STRPTR)"Najnizsze", NULL
};
static STRPTR car_detail_names_en[] = {
    (STRPTR)"Highest", (STRPTR)"High", (STRPTR)"Medium", (STRPTR)"Low", (STRPTR)"Lowest", NULL
};
static STRPTR sound_detail_names[] = {
    (STRPTR)"Niskie", (STRPTR)"Srednie", (STRPTR)"Wysokie", NULL
};
static STRPTR sound_detail_names_en[] = {
    (STRPTR)"Low", (STRPTR)"Medium", (STRPTR)"High", NULL
};

static LauncherConfig cfg = {
    0, /* Carmageddon */
    1, /* MiniGL */
    1, /* 640x480 */
    0, /* CyberGraphX */
    0, /* unlocked */
    1, /* show FPS */
    0, /* fullscreen */
    1, /* sound */
    1, /* sound options */
    0, /* show cutscenes */
    1, /* replay */
    0, /* normal memory */
    0, /* highest car detail */
    2, /* highest sound detail */
    "scsi.device",
    0
};

struct Library *IntuitionBase = NULL;
struct Library *GfxBase = NULL;
struct Library *GadToolsBase = NULL;
static struct Window *win;
static struct Gadget *glist;
static struct Gadget *gadgets[GID_COUNT];
static APTR vi;

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void detect_language(void)
{
    struct Library *LocaleBase = OpenLibrary((STRPTR)"locale.library", 38);
    if (LocaleBase) {
        struct Locale *locale = OpenLocale(NULL);
        if (locale) {
            if (!locale->loc_LanguageName ||
                strncasecmp((char *)locale->loc_LanguageName, "polski", 6) != 0) {
                L = &lang_en;
            }
            CloseLocale(locale);
        }
        CloseLibrary(LocaleBase);
    }
}

static void load_config(void)
{
    FILE *f = fopen(CFG_FILE, "r");
    char line[128];
    char key[64];
    char text_value[64];
    int value;

    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, " %63[^=]=%63[^\r\n]", key, text_value) != 2) continue;
        if (strcasecmp(key, "cd_device") == 0) {
            strncpy(cfg.cd_device, text_value, sizeof(cfg.cd_device) - 1);
            cfg.cd_device[sizeof(cfg.cd_device) - 1] = '\0';
            continue;
        }
        value = atoi(text_value);
        if (strcasecmp(key, "game") == 0) cfg.game = value;
        else if (strcasecmp(key, "renderer") == 0) cfg.renderer = value;
        else if (strcasecmp(key, "resolution") == 0) cfg.resolution = value;
        else if (strcasecmp(key, "display") == 0) cfg.display = value;
        else if (strcasecmp(key, "fps") == 0) cfg.fps = value;
        else if (strcasecmp(key, "show_fps") == 0) cfg.show_fps = value;
        else if (strcasecmp(key, "windowed") == 0) cfg.windowed = value;
        else if (strcasecmp(key, "sound") == 0) cfg.sound = value;
        else if (strcasecmp(key, "sound_options") == 0) cfg.sound_options = value;
        else if (strcasecmp(key, "skip_cutscenes") == 0) cfg.skip_cutscenes = value;
        else if (strcasecmp(key, "replay") == 0) cfg.replay = value;
        else if (strcasecmp(key, "lowmem") == 0) cfg.lowmem = value;
        else if (strcasecmp(key, "car_detail") == 0) cfg.car_detail = value;
        else if (strcasecmp(key, "sound_detail") == 0) cfg.sound_detail = value;
        else if (strcasecmp(key, "cd_unit") == 0) cfg.cd_unit = value;
    }
    fclose(f);

    cfg.game = clamp_int(cfg.game, 0, 3);
    cfg.renderer = clamp_int(cfg.renderer, 0, 1);
    cfg.resolution = clamp_int(cfg.resolution, 0, 8);
    cfg.display = clamp_int(cfg.display, 0, 3);
    cfg.fps = clamp_int(cfg.fps, 0, 4);
    cfg.show_fps = !!cfg.show_fps;
    cfg.windowed = !!cfg.windowed;
    cfg.sound = !!cfg.sound;
    cfg.sound_options = !!cfg.sound_options;
    cfg.skip_cutscenes = !!cfg.skip_cutscenes;
    cfg.replay = !!cfg.replay;
    cfg.lowmem = !!cfg.lowmem;
    cfg.car_detail = clamp_int(cfg.car_detail, 0, 4);
    cfg.sound_detail = clamp_int(cfg.sound_detail, 0, 2);
    if (cfg.cd_device[0] == '\0') strcpy(cfg.cd_device, "scsi.device");
    cfg.cd_unit = clamp_int(cfg.cd_unit, 0, 255);
}

static int save_config(void)
{
    FILE *f = fopen(CFG_FILE, "w");
    if (!f) return 0;
    fprintf(f, "game=%d\n", cfg.game);
    fprintf(f, "renderer=%d\n", cfg.renderer);
    fprintf(f, "resolution=%d\n", cfg.resolution);
    fprintf(f, "display=%d\n", cfg.display);
    fprintf(f, "fps=%d\n", cfg.fps);
    fprintf(f, "show_fps=%d\n", cfg.show_fps);
    fprintf(f, "windowed=%d\n", cfg.windowed);
    fprintf(f, "sound=%d\n", cfg.sound);
    fprintf(f, "sound_options=%d\n", cfg.sound_options);
    fprintf(f, "skip_cutscenes=%d\n", cfg.skip_cutscenes);
    fprintf(f, "replay=%d\n", cfg.replay);
    fprintf(f, "lowmem=%d\n", cfg.lowmem);
    fprintf(f, "car_detail=%d\n", cfg.car_detail);
    fprintf(f, "sound_detail=%d\n", cfg.sound_detail);
    fprintf(f, "cd_device=%s\n", cfg.cd_device);
    fprintf(f, "cd_unit=%d\n", cfg.cd_unit);
    fclose(f);
    return 1;
}

static struct Gadget *make_cycle(struct Gadget *prev, int gid, int x, int y,
                                 const char *label, STRPTR *names, int active)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x + LABEL_W;
    ng.ng_TopEdge = y;
    ng.ng_Width = CONTROL_W;
    ng.ng_Height = GADGET_H;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_GadgetID = gid;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = PLACETEXT_LEFT;
    return CreateGadget(CYCLE_KIND, prev, &ng,
                        GTCY_Labels, (ULONG)names,
                        GTCY_Active, active,
                        TAG_DONE);
}

static struct Gadget *make_check(struct Gadget *prev, int gid, int x, int y,
                                 const char *label, int checked)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x + LABEL_W;
    ng.ng_TopEdge = y;
    ng.ng_Width = 24;
    ng.ng_Height = GADGET_H;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_GadgetID = gid;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = PLACETEXT_LEFT;
    return CreateGadget(CHECKBOX_KIND, prev, &ng,
                        GTCB_Checked, checked,
                        GTCB_Scaled, TRUE,
                        TAG_DONE);
}

static struct Gadget *make_button(struct Gadget *prev, int gid, int x, int y,
                                  int width, const char *label)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x;
    ng.ng_TopEdge = y;
    ng.ng_Width = width;
    ng.ng_Height = 24;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_GadgetID = gid;
    ng.ng_VisualInfo = vi;
    return CreateGadget(BUTTON_KIND, prev, &ng, TAG_DONE);
}

static struct Gadget *make_string(struct Gadget *prev, int gid, int x, int y,
                                  const char *label, char *value, int max_chars)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x + LABEL_W;
    ng.ng_TopEdge = y;
    ng.ng_Width = CONTROL_W;
    ng.ng_Height = GADGET_H;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_GadgetID = gid;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = PLACETEXT_LEFT;
    return CreateGadget(STRING_KIND, prev, &ng,
                        GTST_String, (ULONG)value,
                        GTST_MaxChars, max_chars,
                        TAG_DONE);
}

static struct Gadget *make_integer(struct Gadget *prev, int gid, int x, int y,
                                   const char *label, int value)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x + LABEL_W;
    ng.ng_TopEdge = y;
    ng.ng_Width = CONTROL_W;
    ng.ng_Height = GADGET_H;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_GadgetID = gid;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = PLACETEXT_LEFT;
    return CreateGadget(INTEGER_KIND, prev, &ng,
                        GTIN_Number, value,
                        GTIN_MaxChars, 3,
                        TAG_DONE);
}

static int get_cycle(int gid)
{
    ULONG active = 0;
    if (gadgets[gid]) {
        GT_GetGadgetAttrs(gadgets[gid], win, NULL,
                          GTCY_Active, (ULONG)&active, TAG_DONE);
    }
    return (int)active;
}

static int get_check(int gid)
{
    return gadgets[gid] && (gadgets[gid]->Flags & SELECTED) ? 1 : 0;
}

static const char *get_string(int gid)
{
    struct StringInfo *info;
    if (!gadgets[gid] || !gadgets[gid]->SpecialInfo) return "";
    info = (struct StringInfo *)gadgets[gid]->SpecialInfo;
    return (const char *)info->Buffer;
}

static int get_integer(int gid)
{
    return atoi(get_string(gid));
}

static void update_renderer_controls(void)
{
    int game;
    int renderer;
    int display;
    int resolution;
    int renderer_disabled;
    int resolution_disabled;

    if (!win || !gadgets[GID_GAME] || !gadgets[GID_RENDERER] || !gadgets[GID_DISPLAY]
        || !gadgets[GID_RESOLUTION]) return;

    game = get_cycle(GID_GAME);
    renderer = get_cycle(GID_RENDERER);
    display = get_cycle(GID_DISPLAY);
    resolution = get_cycle(GID_RESOLUTION);
    renderer_disabled = game == 2;

    /* The original Carmageddon demo only contains the software 320x200 data. */
    if (game == 2) renderer = 0;

    GT_SetGadgetAttrs(gadgets[GID_RENDERER], win, NULL,
                      GTCY_Active, renderer,
                      GA_Disabled, renderer_disabled,
                      TAG_DONE);

    resolution_disabled = game == 2 || display == 2;
    /* The original demo and HAM6 remain fixed at 320x200.  MiniGL accepts
     * the extended RTG modes, using the 640x480 game assets as its layout. */
    if (game == 2) resolution = 0;
    else if (display == 2) resolution = 0;
    else if (renderer == 1 && resolution == 0) resolution = 1;
    else if (renderer == 0 && resolution > 1) resolution = 1;

    GT_SetGadgetAttrs(gadgets[GID_RESOLUTION], win, NULL,
                      GTCY_Active, resolution,
                      GA_Disabled, resolution_disabled,
                      TAG_DONE);
}

static void read_gadgets(void)
{
    cfg.game = get_cycle(GID_GAME);
    cfg.renderer = get_cycle(GID_RENDERER);
    cfg.resolution = get_cycle(GID_RESOLUTION);
    cfg.display = get_cycle(GID_DISPLAY);
    cfg.fps = get_cycle(GID_FPS);
    cfg.show_fps = get_check(GID_SHOW_FPS);
    cfg.car_detail = get_cycle(GID_CAR_DETAIL);
    cfg.sound_detail = get_cycle(GID_SOUND_DETAIL);
    cfg.windowed = get_check(GID_WINDOW);
    cfg.sound = get_check(GID_SOUND);
    cfg.sound_options = get_check(GID_SOUND_OPTIONS);
    cfg.skip_cutscenes = get_check(GID_SKIP_CUTSCENES);
    cfg.replay = get_check(GID_REPLAY);
    cfg.lowmem = get_check(GID_LOWMEM);
    strncpy(cfg.cd_device, get_string(GID_CD_DEVICE), sizeof(cfg.cd_device) - 1);
    cfg.cd_device[sizeof(cfg.cd_device) - 1] = '\0';
    if (cfg.cd_device[0] == '\0') strcpy(cfg.cd_device, "scsi.device");
    cfg.cd_unit = clamp_int(get_integer(GID_CD_UNIT), 0, 255);
}

static int create_gui(struct Screen *screen)
{
    struct Gadget *prev;
    int top;
    int button_y;
    int x;
    int y;
    STRPTR *displays = (L == &lang_pl) ? display_names : display_names_en;
    STRPTR *fps = (L == &lang_pl) ? fps_names : fps_names_en;
    STRPTR *cars = (L == &lang_pl) ? car_detail_names : car_detail_names_en;
    STRPTR *sounds = (L == &lang_pl) ? sound_detail_names : sound_detail_names_en;

    vi = GetVisualInfo(screen, TAG_DONE);
    if (!vi) return 0;

    x = ((int)screen->Width - WIN_W) / 2;
    y = ((int)screen->Height - WIN_H) / 2;
    if (x < 0) x = 0;
    if (y < screen->BarHeight + 1) y = screen->BarHeight + 1;

    win = OpenWindowTags(NULL,
        WA_Left, x,
        WA_Top, y,
        WA_Width, WIN_W,
        WA_Height, WIN_H,
        WA_Title, (ULONG)L->title,
        WA_Activate, TRUE,
        WA_CloseGadget, TRUE,
        WA_DragBar, TRUE,
        WA_DepthGadget, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_GADGETDOWN |
                  BUTTONIDCMP | CYCLEIDCMP | CHECKBOXIDCMP | STRINGIDCMP | INTEGERIDCMP,
        TAG_DONE);
    if (!win) return 0;

    prev = CreateContext(&glist);
    if (!prev) return 0;
    top = win->BorderTop + 18;

    gadgets[GID_GAME] = prev = make_cycle(prev, GID_GAME, LEFT_X,
        top + ROW_H * 0, L->game, game_names, cfg.game);
    gadgets[GID_RENDERER] = prev = make_cycle(prev, GID_RENDERER, LEFT_X,
        top + ROW_H * 1, L->renderer, renderer_names, cfg.renderer);
    gadgets[GID_RESOLUTION] = prev = make_cycle(prev, GID_RESOLUTION, LEFT_X,
        top + ROW_H * 2, L->resolution, resolution_names, cfg.resolution);
    gadgets[GID_DISPLAY] = prev = make_cycle(prev, GID_DISPLAY, LEFT_X,
        top + ROW_H * 3, L->display, displays, cfg.display);
    gadgets[GID_FPS] = prev = make_cycle(prev, GID_FPS, LEFT_X,
        top + ROW_H * 4, L->fps, fps, cfg.fps);
    gadgets[GID_CAR_DETAIL] = prev = make_cycle(prev, GID_CAR_DETAIL, LEFT_X,
        top + ROW_H * 5, L->car_detail, cars, cfg.car_detail);
    gadgets[GID_SOUND_DETAIL] = prev = make_cycle(prev, GID_SOUND_DETAIL, LEFT_X,
        top + ROW_H * 6, L->sound_detail, sounds, cfg.sound_detail);

    gadgets[GID_SHOW_FPS] = prev = make_check(prev, GID_SHOW_FPS, RIGHT_X,
        top + ROW_H * 0, L->show_fps, cfg.show_fps);
    gadgets[GID_WINDOW] = prev = make_check(prev, GID_WINDOW, RIGHT_X,
        top + ROW_H * 1, L->windowed, cfg.windowed);
    gadgets[GID_SOUND] = prev = make_check(prev, GID_SOUND, RIGHT_X,
        top + ROW_H * 2, L->sound, cfg.sound);
    gadgets[GID_SOUND_OPTIONS] = prev = make_check(prev, GID_SOUND_OPTIONS, RIGHT_X,
        top + ROW_H * 3, L->sound_options, cfg.sound_options);
    gadgets[GID_SKIP_CUTSCENES] = prev = make_check(prev, GID_SKIP_CUTSCENES, RIGHT_X,
        top + ROW_H * 4, L->skip_cutscenes, cfg.skip_cutscenes);
    gadgets[GID_REPLAY] = prev = make_check(prev, GID_REPLAY, RIGHT_X,
        top + ROW_H * 5, L->replay, cfg.replay);
    gadgets[GID_LOWMEM] = prev = make_check(prev, GID_LOWMEM, RIGHT_X,
        top + ROW_H * 6, L->lowmem, cfg.lowmem);

    gadgets[GID_CD_DEVICE] = prev = make_string(prev, GID_CD_DEVICE, LEFT_X,
        top + ROW_H * 7, L->cd_device, cfg.cd_device, sizeof(cfg.cd_device) - 1);
    gadgets[GID_CD_UNIT] = prev = make_integer(prev, GID_CD_UNIT, RIGHT_X,
        top + ROW_H * 7, L->cd_unit, cfg.cd_unit);

    button_y = WIN_H - win->BorderBottom - 36;
    gadgets[GID_SAVE] = prev = make_button(prev, GID_SAVE, 12, button_y, 96, L->save);
    gadgets[GID_PLAY] = prev = make_button(prev, GID_PLAY, (WIN_W - 112) / 2,
        button_y, 112, L->play);
    gadgets[GID_QUIT] = prev = make_button(prev, GID_QUIT, WIN_W - 108,
        button_y, 96, L->quit);

    AddGList(win, glist, (UWORD)-1, -1, NULL);
    RefreshGList(glist, win, NULL, -1);
    GT_RefreshWindow(win, NULL);
    update_renderer_controls();
    return 1;
}

static const char *find_game_executable(void)
{
    return "dethrace";
}

static void append_arg(char *command, size_t capacity, const char *arg)
{
    size_t used = strlen(command);
    size_t needed = strlen(arg) + 1;
    if (used + needed + 1 >= capacity) return;
    command[used++] = ' ';
    strcpy(command + used, arg);
}

static void build_command(char *command, size_t capacity)
{
    strncpy(command, find_game_executable(), capacity - 1);
    command[capacity - 1] = '\0';
    append_arg(command, capacity, "--use-cfg");
    append_arg(command, capacity, "--debug=0");
}

static void close_gui(void)
{
    if (win && glist) RemoveGList(win, glist, -1);
    if (glist) {
        FreeGadgets(glist);
        glist = NULL;
    }
    if (win) {
        CloseWindow(win);
        win = NULL;
    }
    if (vi) {
        FreeVisualInfo(vi);
        vi = NULL;
    }
}

static void launch_game(const char *command)
{
    BPTR in_nil = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out_nil = Open((STRPTR)"NIL:", MODE_NEWFILE);
    LONG result = SystemTags((STRPTR)command,
        SYS_Input, (ULONG)in_nil,
        SYS_Output, (ULONG)out_nil,
        SYS_Asynch, TRUE,
        NP_StackSize, 500000,
        TAG_DONE);

    if (result == -1) {
        if (in_nil) Close(in_nil);
        if (out_nil) Close(out_nil);
        Execute((STRPTR)command, 0, 0);
    }
}

int main(int argc, char **argv)
{
    struct Screen *screen;
    int running = 1;
    int play = 0;
    char command[512];

    (void)argc;
    (void)argv;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase) {
        puts("DethraceLauncher requires Intuition and GadTools v37+");
        goto cleanup;
    }

    detect_language();
    load_config();

    screen = LockPubScreen(NULL);
    if (!screen) {
        puts("DethraceLauncher: cannot lock public screen");
        goto cleanup;
    }
    if (!create_gui(screen)) {
        UnlockPubScreen(NULL, screen);
        puts("DethraceLauncher: cannot create window");
        goto cleanup;
    }
    UnlockPubScreen(NULL, screen);

    while (running) {
        struct IntuiMessage *message;
        WaitPort(win->UserPort);
        while ((message = GT_GetIMsg(win->UserPort)) != NULL) {
            ULONG msg_class = message->Class;
            struct Gadget *gadget = (struct Gadget *)message->IAddress;
            int gid = gadget ? gadget->GadgetID : 0;
            GT_ReplyIMsg(message);

            if (msg_class == IDCMP_CLOSEWINDOW) {
                running = 0;
            } else if (msg_class == IDCMP_GADGETUP) {
                if (gid == GID_GAME || gid == GID_RENDERER || gid == GID_RESOLUTION
                    || gid == GID_DISPLAY) {
                    update_renderer_controls();
                } else if (gid == GID_SAVE) {
                    read_gadgets();
                    if (save_config()) {
                        SetWindowTitles(win, (STRPTR)L->saved, (STRPTR)~0UL);
                        Delay(25);
                        SetWindowTitles(win, (STRPTR)L->title, (STRPTR)~0UL);
                    }
                } else if (gid == GID_PLAY) {
                    read_gadgets();
                    save_config();
                    build_command(command, sizeof(command));
                    play = 1;
                    running = 0;
                } else if (gid == GID_QUIT) {
                    running = 0;
                }
            }
        }
    }

cleanup:
    close_gui();
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary(GfxBase);
    if (IntuitionBase) CloseLibrary(IntuitionBase);

    if (play) {
        BPTR program_dir = GetProgramDir();
        BPTR old_dir = CurrentDir(program_dir);
        launch_game(command);
        CurrentDir(old_dir);
    }
    return 0;
}
