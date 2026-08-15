#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"

struct Library *CyberGfxBase = NULL;
struct Library *TimerBase = NULL;

extern tHarness_game_config harness_game_config;
struct Screen *amiga_screen = NULL;
struct Window *amiga_window = NULL;
struct RastPort *rp = NULL;
struct ColorMap *cm = NULL;
int render_width = 640, render_height = 480;
int depth = 8;
uint32_t converted_palette[256];
br_pixelmap* last_screen_src = NULL;

static ULONG eclock_frequency = 0;
static ULONG last_frame_time = 0;

static ULONG GetTicks(void) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    struct EClockVal ev;
    ULONG freq = ReadEClock(&ev);
    if (eclock_frequency == 0) {
        eclock_frequency = freq;
    }

    double count = (double)ev.ev_lo + (double)ev.ev_hi * 4294967296.0; 
    double ms_d = (count * 1000.0) / (double)freq;
    ULONG ms = (ULONG)ms_d;

    return ms;
}

static void SleepMs(ULONG ms) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    ULONG ticks = (ms * 50) / 1000;
    if (ticks == 0) ticks = 1;
    Delay(ticks);
}

static void limit_fps(void) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    ULONG now = GetTicks();
    if (last_frame_time != 0) {
        ULONG frame_time = now - last_frame_time;
        ULONG desired_frame = 1000 / harness_game_config.fps;
        if (frame_time < desired_frame) {
            ULONG sleep_time = desired_frame - frame_time;
            if (sleep_time > 5) {
                SleepMs(sleep_time);
            }
        }
    }
    last_frame_time = GetTicks();
}

static int get_and_handle_message(MSG_* msg) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    struct IntuiMessage *imsg;
    BOOL quit = FALSE;

    while ((imsg = (struct IntuiMessage *)GetMsg(amiga_window->UserPort))) {
        ULONG class = imsg->Class;
        UWORD code = imsg->Code;
        ReplyMsg((struct Message*)imsg);

        switch (class) {
            case IDCMP_CLOSEWINDOW:
                msg->message = WM_QUIT;
                quit = TRUE;
                break;
            default:
                break;
        }
    }
    return quit ? 1 : 0;
}

static void present_screen8(br_pixelmap* src) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (!rp || !src) return;

    WriteLUTPixelArray(
        src->pixels,
        0,0,
        src->width,
        rp,
        NULL,
        0,0,
        src->width,
        src->height,
        CTABFMT_XRGB8
    );

    last_screen_src = src;
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

static void present_screen16(br_pixelmap* src) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (!rp || !src) return;

    UWORD *temp_buffer = AllocVec(src->width * src->height * sizeof(UWORD), MEMF_ANY);
    if (!temp_buffer) return;

    UBYTE *src_pixels = src->pixels;
    for (int i = 0; i < src->width * src->height; i++) {
        temp_buffer[i] = (UWORD)converted_palette[src_pixels[i]];
    }

    WritePixelArray(
        (UBYTE*)temp_buffer,
        0,0,
        src->width*2,
        rp,
        0,0,
        src->width,
        src->height,
        RECTFMT_RGB
    );

    FreeVec(temp_buffer);
    last_screen_src = src;
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

static void present_screen32(br_pixelmap* src) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (!rp || !src) return;

    ULONG *temp_buffer = AllocVec(src->width * src->height * sizeof(ULONG), MEMF_ANY);
    if (!temp_buffer) return;

    UBYTE *src_pixels = src->pixels;
    for (int i = 0; i < src->width * src->height; i++) {
        temp_buffer[i] = converted_palette[src_pixels[i]];
    }

    WritePixelArray(
        (UBYTE*)temp_buffer,
        0,0,
        src->width*4,
        rp,
        0,0,
        src->width,
        src->height,
        RECTFMT_ARGB
    );

    FreeVec(temp_buffer);
    last_screen_src = src;
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

static void set_palette8(PALETTEENTRY_* pal) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    struct ViewPort *vp = &amiga_screen->ViewPort;
    ULONG rgb32[1+(256*3)];
    rgb32[0] = 256UL <<16;

    for (int i = 0; i < 256; i++) {
        ULONG R = (ULONG)pal[i].peRed   << 24;
        ULONG G = (ULONG)pal[i].peGreen << 24;
        ULONG B = (ULONG)pal[i].peBlue  << 24;
        rgb32[1+(i*3)] = R;
        rgb32[1+(i*3)+1] = G;
        rgb32[1+(i*3)+2] = B;
    }

    LoadRGB32(vp, rgb32);

    if (last_screen_src != NULL) {
        present_screen8(last_screen_src);
    }
}

static void set_palette16(PALETTEENTRY_* pal) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    for (int i = 0; i < 256; i++) {
        UBYTE r = pal[i].peRed   >> 3;
        UBYTE g = pal[i].peGreen >> 2;
        UBYTE b = pal[i].peBlue  >> 3;
        converted_palette[i] = (r << 11) | (g << 5) | b;
    }
    if (last_screen_src != NULL) {
        present_screen16(last_screen_src);
    }
}

static void set_palette32(PALETTEENTRY_* pal) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = (0xFF << 24) | (pal[i].peRed << 16) | (pal[i].peGreen << 8) | pal[i].peBlue;
    }
    if (last_screen_src != NULL) {
        present_screen32(last_screen_src);
    }
}

static void* create_window_and_renderer(char* title, int x, int y, int width, int height) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    CyberGfxBase = OpenLibrary("cybergraphics.library", 37);
    if (!CyberGfxBase) {
        kprintf("ERROR: can't open cybergraphics.library V37.\n");	
        exit(1);
    }

    render_width = width;
    render_height = height;

    depth = harness_game_config.bpp;
    if (depth > 31) depth = 32;
    else if (depth > 15 && depth < 32) depth = 16;
    else depth = 8;
/*
    ULONG modeID = BestModeID(
        BIDTAG_NominalWidth,  width,
        BIDTAG_NominalHeight, height,
        BIDTAG_Depth,         depth,
        TAG_END
    );*/
        ULONG modeID = BestCModeIDTags(CYBRBIDTG_Depth,
                                 depth,
                                 CYBRBIDTG_NominalWidth,
                                 width,
                                 CYBRBIDTG_NominalHeight,
                                 height,
                                 TAG_DONE);
    if (modeID == INVALID_ID) {
        LOG_PANIC("No suitable screen mode for %dx%d @ %d-bit", width, height, depth);
    }

    amiga_screen = OpenScreenTags(NULL,
        SA_DisplayID, modeID,
        SA_Depth, depth,
        SA_Width, width,
        SA_Height, height,
        SA_Quiet, TRUE,
        SA_DisplayID,
        modeID,
        SA_Title, (ULONG)title,
        TAG_END
    );

    if (!amiga_screen) {
        LOG_PANIC("OpenScreen failed");
    }

    amiga_window = OpenWindowTags(NULL,
        WA_CustomScreen,     (ULONG)amiga_screen,
        WA_Title,            (ULONG)title,
        WA_Left,             x,
        WA_Top,              y,
        WA_Width,            width,
        WA_Height,           height,
        WA_IDCMP,            IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS,
        WA_Flags,            WFLG_SMART_REFRESH | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET,
        TAG_END
    );

    if (!amiga_window) {
        CloseScreen(amiga_screen);
        LOG_PANIC("OpenWindow failed");
    }

    rp = amiga_window->RPort;
    cm = amiga_screen->ViewPort.ColorMap;

    return amiga_window;
}

static int show_cursor(int toggle) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    return 0;
}

void Harness_Platform_Init(tHarness_platform* platform) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (depth == 8) {
        platform->Renderer_SetPalette = set_palette8;
        platform->Renderer_Present = present_screen8;
    } else if (depth == 16) {
        platform->Renderer_SetPalette = set_palette16;
        platform->Renderer_Present = present_screen16;
    } else {
        platform->Renderer_SetPalette = set_palette32;
        platform->Renderer_Present = present_screen32;
    }

    platform->ProcessWindowMessages = get_and_handle_message;
    platform->Sleep = SleepMs;
    platform->GetTicks = GetTicks;
    platform->CreateWindowAndRenderer = create_window_and_renderer;
    platform->ShowCursor = show_cursor;
    platform->GetKeyboardState = NULL;
    platform->GetMousePosition = NULL;
    platform->GetMouseButtons = NULL;
    platform->ShowErrorMessage = NULL;
    platform->SetWindowPos = NULL;
    platform->DestroyWindow = NULL;
}

void Null_Platform_Init(tHarness_platform* platform) {
    kprintf("[%s:%d]\n", __FUNCTION__, __LINE__);
    Harness_Platform_Init(platform);
}
