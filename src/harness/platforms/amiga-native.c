#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <graphics/gfxbase.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <inline/timer.h>
#ifdef DETHRACE_AMIGA_SHARED_MINIGL
#include <proto/minigl.h>
#include <clib/minigl_open_protos.h>
#else
#include <mgl/gl.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "harness.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "common/globvars.h"
#include "common/graphics.h"
#include <SDI_compiler.h>

#define __USE_INLINE__ 1
#include <proto/dos.h>
#include <proto/asl.h>
#include <libraries/asl.h>

#include "amiga_rawcode_to_dinput.h"

typedef struct tAmiga_palette_entry {
    UBYTE peRed;
    UBYTE peGreen;
    UBYTE peBlue;
    UBYTE peFlags;
} PALETTEENTRY_;

extern void QuitGame(void);

static void (*gKeyHandler_func)(void);
static void (*gPalette_impl)(PALETTEENTRY_* palette);
static void handle_amiga_mouse_event(struct Window* win);
static void destroy_window(void);

extern void c2p1x1_8_c5_bm_040(int chunkyx __asm("d0"), int chunkyy __asm("d1"), int offsx __asm("d2"), int offsy __asm("d3"), void* c2pscreen __asm("a0"), struct BitMap* bitmap __asm("a1"));
extern void c2p1x1_6_c5_bm_040(int chunkyx __asm("d0"), int chunkyy __asm("d1"), int offsx __asm("d2"), int offsy __asm("d3"), void* c2pscreen __asm("a0"), struct BitMap* bitmap __asm("a1"));
extern void c2p1x1_4_c5_bm(int chunkyx __asm("d0"), int chunkyy __asm("d1"), int offsx __asm("d2"), int offsy __asm("d3"), void* c2pscreen __asm("a0"), struct BitMap* bitmap __asm("a1"));

#ifdef DETHRACE_AMIGA_SHARED_MINIGL
/* Shared MiniGL owns its private CyberGraphX base.  The application still
 * needs its own base for the native software renderer. */
struct Library *CyberGfxBase = NULL;
#else
/* Statically linked MiniGL owns this process-global library base. */
extern struct Library *CyberGfxBase;
#endif
static struct timeval basetime;
struct Library *TimerBase;

static struct RastPort TempRP;
struct ScreenModeRequester *sm;
static int BytesPerRow;
static UBYTE *GfxAddr;

extern tHarness_game_config harness_game_config;
struct Screen *amiga_screen = NULL;
struct Window *amiga_window = NULL;
struct RastPort *rp = NULL;
struct ColorMap *cm = NULL;
static UWORD *blank_pointer = NULL;
int render_width = 640, render_height = 400;
int depth = 0;
uint32_t converted_palette[256];

br_pixelmap* last_screen_src = NULL;

static ULONG last_frame_time = 0;

/* HAM6 constants and variables */
#define HAM6_DEPTH 6
#define HAM_HOLD 0x00  /* Use color from register (bits 00xxxxxx) */
#define HAM_MODB 0x10  /* Modify only blue component (bits 01xxxxxx) */
#define HAM_MODR 0x20  /* Modify only red component (bits 10xxxxxx) */
#define HAM_MODG 0x30  /* Modify only green component (bits 11xxxxxx) */

static UBYTE *ham_buffer = NULL;
static UBYTE *ham_fade_buffer = NULL;
static UBYTE *ham_source_buffer = NULL;
static int ham_buffer_capacity = 0;
static bool ham_source_frame_valid = 0;
static bool is_ham_mode = 0;
static bool is_aga_mode = 0;
static bool is_opengl_mode = 0;
static bool owns_amiga_screen = 0;
static bool is_public_window = 0;
extern int gGraf_spec_index;

static UWORD current_r = 0, current_g = 0, current_b = 0;
static int ham_stats[4] = {0, 0, 0, 0};  /* Stats for HOLD, MODB, MODR, MODG */
static UBYTE ham_best_register[256];
static int ham_best_register_error[256];
static ULONG ham_reference_palette[256];
static bool ham_reference_palette_valid = 0;
static bool ham_encoded_frame_valid = 0;
static int ham_fade_degree = 256;
static int ham_cached_width = 0;
static int ham_cached_height = 0;

/* Macros for math operations */
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

int reqmodeid(void) {
    int modeid;

    if ((AslBase = OpenLibrary("asl.library", 0)) != NULL) {
        if ((sm = AllocAslRequest(ASL_ScreenModeRequest, NULL))) {
            if (AslRequestTags(sm,
                    ASLSM_TitleText, (ULONG)"Select screen display mode",
                    ASLSM_InitialDisplayID, 0,
                    ASLSM_InitialDisplayDepth, 6,
                    ASLSM_InitialDisplayWidth, 320,
                    ASLSM_InitialDisplayHeight, 256,
                    ASLSM_MinWidth, 320,
                    ASLSM_MinHeight, 200,
					ASLSM_MaxWidth, 640,
					ASLSM_MaxHeight,480,
                    ASLSM_DoWidth, TRUE,
                    ASLSM_DoHeight, TRUE,
                    ASLSM_DoDepth, TRUE,
                    ASLSM_DoOverscanType, TRUE,
                    ASLSM_PropertyFlags, 0,
                    ASLSM_PropertyMask, DIPF_IS_DUALPF | DIPF_IS_PF2PRI,
                    TAG_DONE)) {
            }
            modeid = sm->sm_DisplayID;
            render_width = sm->sm_DisplayWidth;
            render_height = sm->sm_DisplayHeight;
            FreeAslRequest(sm);
        }
        if (AslBase)
            CloseLibrary(AslBase);
    }
    return modeid;
}

static ULONG GetTicks(void) {
    struct EClockVal time1;
    long efreq;
    long long eval;
    struct timeval tv;
    ULONG ticks;

    if (!TimerBase) {
        struct MsgPort *mp;
        mp = CreateMsgPort();
        struct timerequest *tr;
        tr = (struct timerequest *)CreateIORequest(mp, sizeof(struct timerequest));
        OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0);
        TimerBase = tr->tr_node.io_Device;
        if (!TimerBase) {
            exit(1);
        }
    }

    GetSysTime(&tv);
    if (basetime.tv_micro > tv.tv_micro) {
        tv.tv_secs--;
        tv.tv_micro += 1000000;
    }

    ticks = ((tv.tv_secs - basetime.tv_secs) * 1000) + ((tv.tv_micro - basetime.tv_micro) / 1000);
    return ticks;
}

static void SleepMs(ULONG ms) {
    ULONG ticks = (ms * 50) / 1000;
    if (ticks == 0) ticks = 1;
    Delay(ticks);
}

 void limit_fps(void) {
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

static void get_keyboard_state(br_uint_32* buffer) {
    int i;
    memset(buffer, 0, sizeof(br_uint_32) * 8);
    for (i = 0; i < 256; i++) {
        if (directinput_key_state[i] != 0) {
            buffer[i >> 5] |= (1UL << (i & 0x1f));
        }
    }
}

static int amiga_MouseButton1 = 0;
static int amiga_MouseButton2 = 0;

static void get_and_handle_message(void) {
    struct IntuiMessage *imsg;
    int dinput_key;
    ULONG class;
    UWORD code;
    WORD mouseX, mouseY;
    ULONG rawKey;

    while ((imsg = (struct IntuiMessage *)GetMsg(amiga_window->UserPort)))
    {
        class = imsg->Class;
        code = imsg->Code;
        mouseX = imsg->MouseX;
        mouseY = imsg->MouseY;

        ReplyMsg((struct Message*)imsg);

        switch (class) {
            case IDCMP_CLOSEWINDOW:
                QuitGame();
                break;
            case IDCMP_MOUSEMOVE:
            case IDCMP_MOUSEBUTTONS:
                handle_amiga_mouse_event(amiga_window);
                if (code == IECODE_LBUTTON) {
                    amiga_MouseButton1 = 1;
                } else if (code == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {
                    amiga_MouseButton1 = 0;
                } else if (code == IECODE_RBUTTON) {
                    amiga_MouseButton2 = 1;
                } else if (code == (IECODE_RBUTTON | IECODE_UP_PREFIX)) {
                    amiga_MouseButton2 = 0;
                }
                break;
            case IDCMP_RAWKEY:
                rawKey = code & ~IECODE_UP_PREFIX;
                //printf("Raw key: %ld\n", rawKey);
                dinput_key = amigaRawKeyToDirectInputKeyNum[rawKey];
                //printf("DirectInput key: %d\n", dinput_key);
                if (dinput_key == -1) {
                    break;
                }
                /* Emergency exit for a broken fullscreen renderer.  Use the
                 * normal game shutdown so AHI/CD audio and MiniGL are closed. */
                if (!(code & IECODE_UP_PREFIX) && dinput_key == DIK_F10) {
                    printf("Emergency exit (F10)\n");
                    fflush(NULL);
                    QuitGame();
                }
                if (dinput_key == DIK_PLAYPAUSE) {
                    dinput_key = DIK_S;//msg->message = WM_PLAYPAUSE;
                } else if (dinput_key == DIK_MEDIASTOP) {
                    //msg->message = WM_STOP;
                } else if (dinput_key == DIK_NEXTTRACK) {
                   // msg->message = WM_NEXTTRACK;
                } else if (dinput_key == DIK_PREVTRACK) {
                    //msg->message = WM_PREVTRACK;
                }
                if (code & IECODE_UP_PREFIX) {
                    directinput_key_state[dinput_key] = 0;
                } else {
                    directinput_key_state[dinput_key] = 0x80;
                }
                if (gKeyHandler_func != NULL) {
                    gKeyHandler_func();
                }
                break;
            default:
                break;
        }
    }
}


void ChunkyToPlanar(UBYTE *chunky, struct BitMap *bitMap, int width, int height) {
    for (int bit = 0; bit < bitMap->Depth; bit++) {
        if (!bitMap->Planes[bit]) continue;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                UBYTE pixel = chunky[y * width + x];
                int byte_offset = (y * bitMap->BytesPerRow) + (x / 8);
                int bit_offset = 7 - (x % 8);

                if (pixel & (1 << bit)) {
                    bitMap->Planes[bit][byte_offset] |= (1 << bit_offset);
                } else {
                    bitMap->Planes[bit][byte_offset] &= ~(1 << bit_offset);
                }
            }
        }
    }
}

/* Calculate squared color difference between two RGB values */
static int color_distance_squared(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    return dr*dr + dg*dg + db*db;
}

/* Create window and renderer for HAM6 mode */
static void* create_window_and_renderer_pal(char* title, int x, int y, int width, int height) {

    initializeAmigaRawKeyNums();

    ULONG modeID;

    /* Open HAM6 screen with optimal mode ID */
    if (is_ham_mode) {
        if (width >= 640) {
            /* BestModeID() may prefer a 320-wide HAM mode even for a 640
             * framebuffer.  Select the native AGA PAL HiRes HAM interlaced
             * mode explicitly; OpenScreenTags clips its 512-line PAL raster
             * to the requested 640x480 game display. */
            modeID = PAL_MONITOR_ID | HIRESHAMLACE_KEY;
            width = 640;
            height = 480;
        } else {
            modeID = BestModeID(
                BIDTAG_NominalWidth, width,
                BIDTAG_NominalHeight, height,
                BIDTAG_DesiredWidth, width,
                BIDTAG_DesiredHeight, height,
                BIDTAG_Depth, HAM6_DEPTH,
                BIDTAG_DIPFMustHave, DIPF_IS_HAM,
                TAG_DONE);
        }
        if (modeID == INVALID_ID) {
            printf("ERROR: No HAM6 mode available for %ldx%ld\n", width, height);
            exit(1);
        }
    }
    else {
        //modeID = LORES_KEY ;//| HIRESLACE_KEY;
        modeID = 0x00011000; // PAL AGA LORES
        width = 320;
        height = 256;
        if (gGraf_spec_index) {
            //modeID = HIRES_KEY;
            modeID = 0x00019004;
            // modeID = 0x00029004;  //AGA 640x480x8
            width = 640;
            height = 512;
           // modeID |= LACE;
        }

    }
    render_width = width;
    render_height = height;

    if  (harness_game_config.custom_screen)
        modeID = reqmodeid();

    amiga_screen = OpenScreenTags(NULL,
        SA_Depth, depth,
        SA_Width, width,
        SA_Height, height,
        SA_Quiet, TRUE,
        SA_Title, (ULONG)title,
        SA_DisplayID, modeID,
        SA_ShowTitle, FALSE,
        TAG_END
    );

    if (!amiga_screen) {
        printf("ERROR: Failed to open HAM6 screen\n");
        exit(1);
    }
    owns_amiga_screen = 1;
    is_public_window = 0;


    BytesPerRow = amiga_screen->RastPort.BitMap->BytesPerRow;

    /* Open window on HAM6 screen */
    amiga_window = OpenWindowTags(NULL,
        WA_CustomScreen, amiga_screen,
        WA_Left, 0,
        WA_Top, 0,
        WA_Width, width,
        WA_Height, height,
        WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_CLOSEWINDOW,
        WA_InnerWidth, width,
        WA_InnerHeight, height,
        WA_Borderless, TRUE,
        WA_RMBTrap, TRUE,
        WA_Activate, TRUE,
        WA_ReportMouse, TRUE,
        TAG_END
    );

    if (!amiga_window) {
        CloseScreen(amiga_screen);
        printf("ERROR: Failed to open HAM6 window\n");
        exit(1);
    }

    rp = amiga_window->RPort;
    cm = amiga_screen->ViewPort.ColorMap;

    /* Allocate HAM buffer */
    ham_buffer = AllocVec(width * height, MEMF_CLEAR | MEMF_ANY);
    ham_fade_buffer = AllocVec(width * height, MEMF_CLEAR | MEMF_ANY);
    ham_source_buffer = AllocVec(width * height, MEMF_CLEAR | MEMF_ANY);
    ham_buffer_capacity = width * height;
    if (!ham_buffer || !ham_fade_buffer || !ham_source_buffer) {
        printf("ERROR: Failed to allocate HAM buffers\n");
        if (ham_buffer) {
            FreeVec(ham_buffer);
            ham_buffer = NULL;
        }
        if (ham_fade_buffer) {
            FreeVec(ham_fade_buffer);
            ham_fade_buffer = NULL;
        }
        if (ham_source_buffer) {
            FreeVec(ham_source_buffer);
            ham_source_buffer = NULL;
        }
        ham_buffer_capacity = 0;
        CloseWindow(amiga_window);
        CloseScreen(amiga_screen);
        exit(1);
    }

    return amiga_window;
}

static void* create_window_and_renderer(char* title, int x, int y, int width, int height) {
    initializeAmigaRawKeyNums();

    ULONG modeID = 0;
    bool use_public_window = !harness_game_config.start_full_screen
        && !is_aga_mode && !is_ham_mode;
    if (!is_aga_mode) {
        CyberGfxBase = OpenLibrary("cybergraphics.library", 37);
        if (!CyberGfxBase) {
            printf("ERROR: can't open cybergraphics.library V37.\n");
            exit(1);
        }
    }

    if (use_public_window) {
        amiga_screen = LockPubScreen(NULL);
        if (!amiga_screen) {
            printf("ERROR: Failed to lock public screen\n");
            exit(1);
        }

        amiga_window = OpenWindowTags(NULL,
            WA_PubScreen, amiga_screen,
            WA_Left, 20,
            WA_Top, 20,
            WA_InnerWidth, width,
            WA_InnerHeight, height,
            WA_Title, (ULONG)title,
            WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_CLOSEWINDOW,
            WA_DragBar, TRUE,
            WA_DepthGadget, TRUE,
            WA_CloseGadget, TRUE,
            WA_RMBTrap, TRUE,
            WA_Activate, TRUE,
            WA_ReportMouse, TRUE,
            TAG_END);

        UnlockPubScreen(NULL, amiga_screen);
        if (!amiga_window) {
            amiga_screen = NULL;
            printf("ERROR: Failed to open software window\n");
            exit(1);
        }

        owns_amiga_screen = 0;
        is_public_window = 1;
        render_width = width;
        render_height = height;
        BytesPerRow = width;
        rp = amiga_window->RPort;
        cm = amiga_screen->ViewPort.ColorMap;
        return amiga_window;
    }

    /* Open HAM6 screen with optimal mode ID */
    if (is_ham_mode) {
        modeID = LORES_KEY | HAM_KEY;

    }
    else if (is_aga_mode) {
        //modeID = LORES_KEY ;//| HIRESLACE_KEY;
        modeID = 0x00011000; // PAL AGA LORES
        width = 320;
        height = 200;//56;
        if (gGraf_spec_index) {
            //modeID = HIRES_KEY;
            modeID = 0x00029004;
            width = 640;
            height = 480;//512;
           // modeID |= LACE;
        }

    }
    render_width = width;
    render_height = height;
    if (!is_aga_mode && !is_ham_mode) {
        modeID = BestCModeIDTags(CYBRBIDTG_Depth,
                                    depth,
                                    CYBRBIDTG_NominalWidth,
                                    width,
                                    CYBRBIDTG_NominalHeight,
                                    height,
                                    TAG_DONE);
    }
    //modeID = 0x00029004;  //AGA 640x480x8
    if  (harness_game_config.custom_screen)
        modeID = reqmodeid();

    amiga_screen = OpenScreenTags(NULL,
        SA_Depth, depth,
        SA_Width, width,
        SA_Height, height,
        SA_Quiet, TRUE,
        SA_Title, (ULONG)title,
        SA_DisplayID, modeID,
        TAG_END
    );

    if (!amiga_screen) {
        printf("ERROR: Failed to open screen\n");
        exit(1);
    }
    owns_amiga_screen = 1;
    is_public_window = 0;

    BytesPerRow = ((amiga_screen->Width + 15) & ~15);


    amiga_window = OpenWindowTags(NULL,
        WA_CustomScreen, amiga_screen,
        WA_Left, 0,
        WA_Top, 0,
        WA_Width, width,
        WA_Height, height,
        WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_CLOSEWINDOW,
        WA_InnerWidth, width,
        WA_InnerHeight, height,
        WA_Borderless, TRUE,
        WA_RMBTrap, TRUE,
        amiga_screen ? WA_PubScreen : TAG_IGNORE,
        (ULONG)amiga_screen,
        WA_Activate, TRUE,
        WA_ReportMouse, TRUE,
        TAG_END
    );

    if (!amiga_window) {
        CloseScreen(amiga_screen);
        printf("ERROR: Failed to open window\n");
        exit(1);
    }

    rp = amiga_window->RPort;
    cm = amiga_screen->ViewPort.ColorMap;

    return amiga_window;
}

static UBYTE *screen_buffers[2] = {NULL, NULL};
static int current_buffer = 0;

static void present_screen8(br_pixelmap* src) {
    int copy_width;
    int copy_height;
    int y;

    if (!rp || !src || !src->pixels) {
        printf("ERROR: `rp` or `src->pixels` is NULL!\n");
        return;
    }

    int screen_width = is_public_window ? render_width : amiga_screen->Width;
    int screen_height = is_public_window ? render_height : amiga_screen->Height;

    if (!screen_buffers[0]) {
        screen_buffers[0] = AllocVec(BytesPerRow * screen_height, MEMF_FAST);
        screen_buffers[1] = AllocVec(BytesPerRow * screen_height, MEMF_FAST);
        if (!screen_buffers[0] || !screen_buffers[1]) {
            printf("ERROR: Failed to allocate double buffers\n");
            return;
        }
    }

    UBYTE *src_pixels = (UBYTE *)src->pixels;
    UBYTE *temp_buffer = screen_buffers[current_buffer];

    /* set_palette8() installs an identity palette: a source byte is already
     * the exact LUT8 value required by CyberGraphX and by the AGA C2P path.
     * The old loop performed a 32-bit palette lookup plus a comparison for
     * every pixel, even though a moving race frame changes almost everywhere.
     * Copy complete rows and present unconditionally instead. */
    copy_width = src->width < screen_width ? src->width : screen_width;
    copy_height = src->height < screen_height ? src->height : screen_height;
    for (y = 0; y < copy_height; y++) {
        memcpy(temp_buffer + y * BytesPerRow,
            src_pixels + y * src->row_bytes, copy_width);
    }

if (!is_aga_mode) {
        WritePixelArray(
            (UBYTE*)temp_buffer,
            0, 0,
            src->width,
            rp,
            0, 0,
            src->width,
            src->height,
            RECTFMT_LUT8);
} else {
    struct BitMap aga_bm = {
            .BytesPerRow = BytesPerRow / 8,     // 1 bajt = 8 pikseli w trybie planar
            .Rows        = screen_height,
            .Flags       = 0,
            .Depth       = 8,
            .Planes      = {
                amiga_screen->RastPort.BitMap->Planes[0],
                amiga_screen->RastPort.BitMap->Planes[1],
                amiga_screen->RastPort.BitMap->Planes[2],
                amiga_screen->RastPort.BitMap->Planes[3],
                amiga_screen->RastPort.BitMap->Planes[4],
                amiga_screen->RastPort.BitMap->Planes[5],
                amiga_screen->RastPort.BitMap->Planes[6],
                amiga_screen->RastPort.BitMap->Planes[7]
            }
        };

        c2p1x1_8_c5_bm_040(
            src->width,
            src->height,
            0,             // scroffsx
            0,             // scroffsy
            temp_buffer,
        &aga_bm
        );
        }

    current_buffer = 1 - current_buffer;
    //last_screen_src = src;

    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}
//#if 1

static void rebuild_ham6_register_cache(void) {
    int palette_index;
    int reg;

    for (palette_index = 0; palette_index < 256; palette_index++) {
        ULONG colour = converted_palette[palette_index];
        UBYTE r4 = ((colour >> 16) & 0xff) >> 4;
        UBYTE g4 = ((colour >> 8) & 0xff) >> 4;
        UBYTE b4 = (colour & 0xff) >> 4;
        int best_reg = 0;
        int best_error = 999999;

        for (reg = 0; reg < 16; reg++) {
            ULONG reg_colour = converted_palette[reg];
            UBYTE reg_r4 = ((reg_colour >> 16) & 0xff) >> 4;
            UBYTE reg_g4 = ((reg_colour >> 8) & 0xff) >> 4;
            UBYTE reg_b4 = (reg_colour & 0xff) >> 4;
            int error = color_distance_squared(r4, g4, b4,
                reg_r4, reg_g4, reg_b4);

            if (error < best_error) {
                best_error = error;
                best_reg = reg;
            }
        }
        ham_best_register[palette_index] = best_reg;
        ham_best_register_error[palette_index] = best_error;
    }
}

static int detect_ham6_fade_degree(const PALETTEENTRY_ *pal) {
    br_colour *reference;
    int max_reference = 0;
    int matching_value = 0;
    int candidate;
    int first;
    int last;
    int degree;
    int i;

    if (!gCurrent_palette || !gCurrent_palette->pixels) {
        return -1;
    }
    reference = (br_colour *)gCurrent_palette->pixels;

    for (i = 0; i < 256; i++) {
        int values[3] = { BR_RED(reference[i]), BR_GRN(reference[i]), BR_BLU(reference[i]) };
        int faded[3] = { pal[i].peRed, pal[i].peGreen, pal[i].peBlue };
        int component;
        for (component = 0; component < 3; component++) {
            if (values[component] > max_reference) {
                max_reference = values[component];
                matching_value = faded[component];
            }
        }
    }

    if (max_reference == 0) {
        for (i = 0; i < 256; i++) {
            if (pal[i].peRed || pal[i].peGreen || pal[i].peBlue) {
                return -1;
            }
        }
        return 256;
    }

    candidate = (matching_value * 256 + max_reference / 2) / max_reference;
    first = candidate > 2 ? candidate - 2 : 0;
    last = candidate < 254 ? candidate + 2 : 256;
    for (degree = first; degree <= last; degree++) {
        bool matches = 1;
        for (i = 0; i < 256 && matches; i++) {
            if (pal[i].peRed != degree * BR_RED(reference[i]) / 256
                || pal[i].peGreen != degree * BR_GRN(reference[i]) / 256
                || pal[i].peBlue != degree * BR_BLU(reference[i]) / 256) {
                matches = 0;
            }
        }
        if (matches) {
            return degree;
        }
    }
    return -1;
}

static bool set_ham6_reference_palette(const PALETTEENTRY_ *pal, int fade_degree) {
    br_colour *reference = gCurrent_palette && gCurrent_palette->pixels
        ? (br_colour *)gCurrent_palette->pixels : NULL;
    bool changed = !ham_reference_palette_valid;
    int i;

    for (i = 0; i < 256; i++) {
        ULONG colour;
        if (fade_degree >= 0 && reference) {
            colour = (BR_RED(reference[i]) << 16)
                | (BR_GRN(reference[i]) << 8) | BR_BLU(reference[i]);
        } else {
            colour = (pal[i].peRed << 16) | (pal[i].peGreen << 8) | pal[i].peBlue;
        }
        if (ham_reference_palette[i] != colour) {
            changed = 1;
        }
        ham_reference_palette[i] = colour;
        converted_palette[i] = colour;
    }
    ham_reference_palette_valid = 1;
    if (changed) {
        rebuild_ham6_register_cache();
        ham_encoded_frame_valid = 0;
    }
    return changed;
}

/* Find the best HAM6 representation for a pixel */
static UBYTE find_best_ham6_pixel(UBYTE palette_index, UBYTE r, UBYTE g, UBYTE b) {
    /* Scale down from 8-bit to 4-bit color */
    UBYTE r4 = r >> 4;
    UBYTE g4 = g >> 4;
    UBYTE b4 = b >> 4;

    /* Option 1: Use a color register */
    int best_reg = ham_best_register[palette_index];
    int best_reg_error = ham_best_register_error[palette_index];

    /* Option 2: Modify red */
    int error_mod_r = color_distance_squared(r4, g4, b4, r4, current_g, current_b);

    /* Option 3: Modify green */
    int error_mod_g = color_distance_squared(r4, g4, b4, current_r, g4, current_b);

    /* Option 4: Modify blue */
    int error_mod_b = color_distance_squared(r4, g4, b4, current_r, current_g, b4);

    /* Find the option with minimum error */
    UBYTE result;

    if (best_reg_error <= error_mod_r && best_reg_error <= error_mod_g && best_reg_error <= error_mod_b) {
        /* Use color register */
        result = best_reg;
        current_r = (converted_palette[best_reg] >> 16) & 0xFF;
        current_g = (converted_palette[best_reg] >> 8) & 0xFF;
        current_b = converted_palette[best_reg] & 0xFF;
        current_r >>= 4;
        current_g >>= 4;
        current_b >>= 4;
        ham_stats[0]++;
    } else if (error_mod_r <= error_mod_g && error_mod_r <= error_mod_b) {
        /* Modify red */
        result = HAM_MODR | r4;
        current_r = r4;
        ham_stats[2]++;
    } else if (error_mod_g <= error_mod_r && error_mod_g <= error_mod_b) {
        /* Modify green */
        result = HAM_MODG | g4;
        current_g = g4;
        ham_stats[3]++;
    } else {
        /* Modify blue */
        result = HAM_MODB | b4;
        current_b = b4;
        ham_stats[1]++;
    }

    return result;
}

/* Deklaracja globalnych buforów HAM */
static UBYTE *ham_buffers[3] = {NULL, NULL, NULL};

static void present_screen_ham6_3buf(br_pixelmap* src) {
    if (!rp || !src || !src->pixels || !ham_buffer) {
        printf("ERROR: Invalid parameters for HAM6 rendering\n");
        return;
    }

    /* Reset HAM statistics */
    ham_stats[0] = ham_stats[1] = ham_stats[2] = ham_stats[3] = 0;

    int screen_width = amiga_screen->Width;
    int screen_height = amiga_screen->Height;
    UBYTE *src_pixels = (UBYTE *)src->pixels;

    /* Triple buffering implementation */
    static UBYTE *ham_buffers[3] = {NULL, NULL, NULL};
    static int current_buffer = 0;
    static int render_buffer = 0;
    static int display_buffer = 0;

    /* Initialize buffers if needed */
    if (ham_buffers[0] == NULL) {
        for (int i = 0; i < 3; i++) {
            ham_buffers[i] = AllocMem(screen_width * screen_height, MEMF_CLEAR | MEMF_PUBLIC);
            if (!ham_buffers[i]) {
                printf("ERROR: Failed to allocate triple buffer %d\n", i);
                /* Free previously allocated buffers */
                for (int j = 0; j < i; j++) {
                    if (ham_buffers[j]) FreeMem(ham_buffers[j], screen_width * screen_height);
                }
                return;
            }
        }
    }

    /* Update render buffer index */
    render_buffer = (current_buffer + 1) % 3;

    /* Reset current color at beginning of each frame */
    current_r = current_g = current_b = 0;

    /* Convert image to HAM6 format into the render buffer */
    for (int y = 0; y < screen_height; y++) {
        /* Reset current color at the beginning of each line */
        current_r = current_g = current_b = 0;

        for (int x = 0; x < screen_width; x++) {
            int src_index = y * src->width + x;
            int dst_index = y * screen_width + x;

            if (src_index < src->width * src->height) {
                /* Get pixel color from palette */
                ULONG pixel_color = converted_palette[src_pixels[src_index]];
                UBYTE r = (pixel_color >> 16) & 0xFF;
                UBYTE g = (pixel_color >> 8) & 0xFF;
                UBYTE b = pixel_color & 0xFF;

                /* Find best HAM6 representation */
                ham_buffers[render_buffer][dst_index] = find_best_ham6_pixel(src_pixels[src_index], r, g, b);
            } else {
                /* Out of bounds - use black */
                ham_buffers[render_buffer][dst_index] = 0;
            }
        }
    }

    /* Wait for vertical blank to minimize tearing */
    //WaitTOF();

    /* Update display buffer index to the previously rendered buffer */
    display_buffer = current_buffer;

    /* Update current buffer to the one we just rendered */
    current_buffer = render_buffer;

#if USE_WPAL8
    /* Display the HAM6 image from the display buffer */
    for (int y = 0; y < screen_height; y++) {
        WritePixelLine8(rp, 0, y, screen_width, &ham_buffers[display_buffer][y * screen_width], NULL);
    }
#else
    struct BitMap temp_bm = {
        .BytesPerRow = (src->width + 15) / 16 * 2,     // 1 bajt = 8 pikseli w trybie planar
        .Rows        = screen_height,
        .Flags       = 0,
        .Depth       = 6,                   // HAM6 używa 6 płaszczyzn bitowych
        .Planes      = {
            amiga_screen->RastPort.BitMap->Planes[0],
            amiga_screen->RastPort.BitMap->Planes[1],
            amiga_screen->RastPort.BitMap->Planes[2],
            amiga_screen->RastPort.BitMap->Planes[3],
            amiga_screen->RastPort.BitMap->Planes[4],
            amiga_screen->RastPort.BitMap->Planes[5]
        }
    };

    // Użyj funkcji c2p dla 6 płaszczyzn zamiast 8
    c2p1x1_6_c5_bm_040(
        src->width,
        src->height,
        0,                                   // scroffsx
        0,                                   // scroffsy
        ham_buffers[display_buffer],         // Używamy bufora wyświetlania
        &temp_bm
    );
#endif

    last_screen_src = src;

    /* Apply FPS limiting */
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

/* Funkcja do czyszczenia buforów podczas zamykania programu */
void cleanup_ham6_buffers(void) {
    extern UBYTE *ham_buffers[3];

    if (ham_buffers[0] != NULL) {
        int screen_width = amiga_screen->Width;
        int screen_height = amiga_screen->Height;

        for (int i = 0; i < 3; i++) {
            if (ham_buffers[i]) {
                FreeMem(ham_buffers[i], screen_width * screen_height);
                ham_buffers[i] = NULL;
            }
        }
    }
}

static void display_cached_ham6(void) {
    UBYTE *display_buffer = ham_buffer;
    int pixel_count = ham_cached_width * ham_cached_height;
    int i;

    if (!ham_encoded_frame_valid || ham_cached_width <= 0
        || ham_cached_height <= 0) {
        return;
    }

    if (ham_fade_degree < 256 && ham_fade_buffer) {
        for (i = 0; i < pixel_count; i++) {
            UBYTE pixel = ham_buffer[i];
            if (pixel & 0x30) {
                int faded_component = ((pixel & 0x0f) * ham_fade_degree + 128) / 256;
                if (faded_component > 15) {
                    faded_component = 15;
                }
                pixel = (pixel & 0x30)
                    | faded_component;
            }
            ham_fade_buffer[i] = pixel;
        }
        display_buffer = ham_fade_buffer;
    }

    /* Use graphics.library for HAM output.  The custom 6-plane C2P routine
     * was the only rendering path exercised by HAM6 before the race but not
     * by ordinary software mode, and it was corrupting state later consumed
     * by the first 3D track rasterisation. */
    for (i = 0; i < ham_cached_height; i++) {
        WritePixelLine8(rp, 0, i, ham_cached_width,
            &display_buffer[i * ham_cached_width], NULL);
    }
}

static void encode_owned_screen_ham6(void) {
    /* Reset HAM statistics */
    ham_stats[0] = ham_stats[1] = ham_stats[2] = ham_stats[3] = 0;

    int screen_width = ham_cached_width;
    int screen_height = ham_cached_height;

    /* Reset current color at beginning of each frame */
    current_r = current_g = current_b = 0;

    /* Convert image to HAM6 format */
    for (int y = 0; y < screen_height; y++) {
        /* Reset current color at the beginning of each line */
        current_r = current_g = current_b = 0;

        for (int x = 0; x < screen_width; x++) {
            int src_index = y * screen_width + x;
            int dst_index = y * screen_width + x;
            UBYTE palette_index = ham_source_buffer[src_index];
            ULONG pixel_color = converted_palette[palette_index];
            UBYTE r = (pixel_color >> 16) & 0xFF;
            UBYTE g = (pixel_color >> 8) & 0xFF;
            UBYTE b = pixel_color & 0xFF;

            ham_buffer[dst_index] = find_best_ham6_pixel(palette_index, r, g, b);
        }
    }
#if 0
    /* Print HAM statistics periodically */
    static int frame_count = 0;
    if (++frame_count % 60 == 0) {
      //  printf("HAM6 stats: HOLD=%d, MODB=%d, MODR=%d, MODG=%d\n",
       //        ham_stats[0], ham_stats[1], ham_stats[2], ham_stats[3]);
    }
#endif
    ham_encoded_frame_valid = 1;
    display_cached_ham6();
}

/* Present screen using HAM6 mode */
static void present_screen_ham6(br_pixelmap* src) {
    int screen_width;
    int screen_height;
    int y;

    if (!rp || !src || !src->pixels || !ham_buffer || !ham_source_buffer) {
        printf("ERROR: Invalid parameters for HAM6 rendering\n");
        return;
    }

    /* Keep an owned indexed copy. Palette callbacks can outlive the game's
     * pixelmap during the menu-to-race transition, so retaining src here is
     * unsafe and used to make HAM6 hang while the music kept playing. */
    screen_width = src->width;
    screen_height = src->height;
    if (screen_width <= 0 || screen_height <= 0
        || screen_width * screen_height > ham_buffer_capacity
        || src->row_bytes < screen_width) {
        printf("ERROR: Invalid HAM6 source dimensions\n");
        return;
    }
    for (y = 0; y < screen_height; y++) {
        memcpy(ham_source_buffer + y * screen_width,
            (UBYTE *)src->pixels + y * src->row_bytes, screen_width);
    }
    ham_cached_width = screen_width;
    ham_cached_height = screen_height;
    ham_source_frame_valid = 1;
    encode_owned_screen_ham6();

    /* Apply FPS limiting */
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

void set_palette_ham6_normal(PALETTEENTRY_ *pal) {
    /* Palette fades run in a tight 500 ms loop without swapping frames.
     * Synchronise each visible HAM update to vertical blank to avoid the
     * characteristic horizontal tearing/banding. */
    if (ham_encoded_frame_valid) {
        WaitTOF();
    }

    // Ustaw bazowe 16 kolorów dla HAM6
    for (int i = 0; i < 16; i++) {
        // Konwersja z wartości 8-bit (0-255) na 4-bit (0-15)
        UBYTE r4 = (pal[i].peRed * 15 + 127) / 255;
        UBYTE g4 = (pal[i].peGreen * 15 + 127) / 255;
        UBYTE b4 = (pal[i].peBlue * 15 + 127) / 255;

        // Ustaw kolor w palecie Amigi
        SetRGB4(&amiga_screen->ViewPort, i, r4, g4, b4);

    }

    /* Re-encode the cached indexed frame against the actual faded palette.
     * Scaling already-encoded HAM modify nibbles is faster, but introduces
     * visible colour steps and dirty edges because HAM colour decisions are
     * dependent on the preceding pixel. */
    ham_fade_degree = 256;
    set_ham6_reference_palette(pal, -1);
    if (ham_source_frame_valid) {
        encode_owned_screen_ham6();
    }
}
/* Initialize HAM6 palette */
static void set_palette_ham6_loading(PALETTEENTRY_* pal) {
    set_palette_ham6_normal(pal);
}

extern bool loading_palette;

void set_palette_ham6(PALETTEENTRY_ *pal) {
    if (loading_palette) {
        set_palette_ham6_loading(pal);
    } else {
        set_palette_ham6_normal(pal);
    }
}

static void set_palette8(PALETTEENTRY_* pal) {
    if (is_ham_mode) {
        /* If in HAM mode, use the HAM palette setup instead */
        set_palette_ham6(pal);
        return;
    }

    /* Normal 8-bit palette setup */
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = i;  /* Direct index for 8-bit mode */
        SetRGB32(&amiga_screen->ViewPort, i,
                 pal[i].peRed << 24,
                 pal[i].peGreen << 24,
                 pal[i].peBlue << 24);
    }

    if (last_screen_src != NULL) {
        present_screen8(last_screen_src);
    }

}

// Globalna tablica na paletę 16-bitową (RGB 5-6-5)
USHORT converted_palette_16[256];


// --- Poprawiona set_palette16 ---
static void set_palette16(PALETTEENTRY_* pal) {
    if (!amiga_screen) return; // Sprawdzenie bezpieczeństwa

    for (int i = 0; i < 256; i++) {
        // 1. Ustawienie tablicy 32-bit (tak jak w set_palette32)
        //    Format ARGB 8-8-8-8
        converted_palette[i] = (0xFF << 24) |              // Alpha (pełna nieprzezroczystość)
                               (pal[i].peRed   << 16) |    // Czerwony
                               (pal[i].peGreen << 8)  |    // Zielony
                                pal[i].peBlue;             // Niebieski

        // 2. Ustawienie tablicy 16-bit (użyj jednej z wersji z poprzedniej odpowiedzi)
        //    Np. wersja RGB 5-6-5:
        USHORT r5 = (pal[i].peRed   >> 3) & 0x1F;
        USHORT g6 = (pal[i].peGreen >> 2) & 0x3F;
        USHORT b5 = (pal[i].peBlue  >> 3) & 0x1F;
        converted_palette_16[i] = (r5 << 11) | (g6 << 5) | b5;

        // --- LUB INNA WERSJA 16-BIT (testuj osobno) ---
        // np. RGB 5-6-5 z zamianą bajtów:
        // USHORT rgb565 = (r5 << 11) | (g6 << 5) | b5;
        // converted_palette_16[i] = SWAP_BYTES_16(rgb565);
        // --- Koniec innych wersji ---

        // Diagnostyka (opcjonalna)
        // if (i < 5) {
        //     printf("Pal[%d]: 32b=0x%08lX, 16b=0x%04X\n", i,
        //            converted_palette[i], converted_palette_16[i]);
        // }
    }

    // Odświeżenie nie jest tu zwykle potrzebne, chyba że logika gry tego wymaga
     if (last_screen_src != NULL) {
         present_screen16(last_screen_src);
     }
}
// --- Skonwertowana funkcja present_screen16 ---
 void present_screen16(br_pixelmap* src) {
    // Sprawdzenie warunków początkowych
    if (!rp || !src || !src->pixels) {
        // Można dodać logowanie błędu
        return;
    }

    int width = src->width;
    int height = src->height;
    int num_pixels = width * height;

    // 1. Alokacja tymczasowego bufora na dane 16-bitowe
    //    Rozmiar = szerokość * wysokość * 2 bajty na piksel (USHORT)
    USHORT *temp_buffer = (USHORT *)AllocVec(num_pixels * sizeof(USHORT), MEMF_ANY);
    if (!temp_buffer) {
        // Logowanie błędu alokacji
        return;
    }

    // 2. Konwersja danych 8-bit indeksowanych na 16-bit RGB
    UBYTE *src_ptr = (UBYTE *)src->pixels;
    USHORT *dst_ptr = temp_buffer;

    for (int i = 0; i < num_pixels; i++) {
        // Pobierz indeks koloru 8-bit
        UBYTE index = *src_ptr++;
        // Pobierz odpowiadający kolor 16-bit z przygotowanej tablicy
        // i zapisz w buforze tymczasowym
        *dst_ptr++ = converted_palette_16[index];
    }

    // 3. Użycie WritePixelArray do skopiowania bufora 16-bit na ekran CGX
    //    CyberGraphX oczekuje wskaźnika UBYTE*, stąd rzutowanie (UBYTE*)
    WritePixelArray(
        (UBYTE*)temp_buffer,    // Wskaźnik do danych źródłowych (nasz bufor 16-bit)
        0,                      // Początkowe X w buforze źródłowym
        0,                      // Początkowe Y w buforze źródłowym
        width * 2,              // Modulo źródła (liczba bajtów na linię w buforze = szerokość * 2)
        rp,                     // RastPort docelowy (okno/ekran Amigi)
        0,                      // Początkowe X w RastPorcie docelowym
        0,                      // Początkowe Y w RastPorcie docelowym
        width,                  // Szerokość obszaru do skopiowania (w pikselach)
        height,                 // Wysokość obszaru do skopiowania (w pikselach)
        RECTFMT_RGB           // Format danych źródłowych - 16bit RGB
                                // Możliwe alternatywy to RECTFMT_RGB15, RECTFMT_RGB16PC
                                // w zależności od konfiguracji CGX i endianu.
                                // RECTFMT_RGB może też działać, jeśli CGX wykryje
                                // głębię RastPortu jako 16-bit. Zacznij od RECTFMT_RGB16.
                                // Jeśli kolory są zamienione (np. czerwony z niebieskim),
                                // spróbuj RECTFMT_BGR16 lub sprawdź format zwracany przez
                                // GetCyberMapAttr(.., CYBRMATTR_PIXFMT).
                                // **EDIT:** W twoim oryginalnym kodzie CGX używałeś RECTFMT_RGB,
                                // co sugeruje, że CGX sam określa format na podstawie
                                // głębi bitowej ekranu. Możesz spróbować najpierw z:
                                // RECTFMT_RGB
    );


    // 4. Zwolnienie bufora tymczasowego
    FreeVec(temp_buffer);

    // Zapisanie ostatnio użytego źródła (na potrzeby set_palette)
    last_screen_src = src;

    // Ograniczenie FPS
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

static void present_screen32(br_pixelmap* src) {
    if (!rp || !src) return;

    ULONG *temp_buffer = AllocVec(src->width * src->height * sizeof(ULONG), MEMF_ANY);
    if (!temp_buffer) return;

    UBYTE *src_pixels = src->pixels;
    for (int i = 0; i < src->width * src->height; i++) {
        temp_buffer[i] = converted_palette[src_pixels[i]];
    }

    WritePixelArray(
        (UBYTE*)temp_buffer,
        0, 0,
        src->width * 4,
        rp,
        is_public_window ? amiga_window->BorderLeft : 0,
        is_public_window ? amiga_window->BorderTop : 0,
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

static void set_palette32(PALETTEENTRY_* pal) {
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = (0xFF << 24) | (pal[i].peRed << 16) | (pal[i].peGreen << 8) | pal[i].peBlue;
    }

    if (last_screen_src != NULL) {
        present_screen32(last_screen_src);
    }
}

static int show_cursor(int toggle) {
    if (!amiga_window) {
        return 0;
    }

    if (toggle) {
        ClearPointer(amiga_window);
    } else {
        if (!blank_pointer) {
            blank_pointer = AllocVec(6 * sizeof(*blank_pointer), MEMF_CHIP | MEMF_CLEAR);
        }
        if (blank_pointer) {
            SetPointer(amiga_window, blank_pointer, 1, 16, 0, 0);
        }
    }
    return 0;
}

static int show_error_message(char* title, char* message) {
    printf("ERROR:%s\n", message);
    return 0;
}

static int set_window_pos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    if (nWidth == 320 && nHeight == 200) {
        nWidth = 640;
        nHeight = 400;
    }
    return 0;
}

static int amiga_MouseX = 0;
static int amiga_MouseY = 0;

static void handle_amiga_mouse_event(struct Window* win) {
    amiga_MouseX = win->MouseX;
    amiga_MouseY = win->MouseY;
}

static int get_mouse_buttons(int* pButton1, int* pButton2) {
    *pButton1 = amiga_MouseButton1;
    *pButton2 = amiga_MouseButton2;
    return 0;
}

static int get_mouse_position(int* pX, int* pY) {
    int lX = amiga_MouseX;
    int lY = amiga_MouseY;
    int input_width = render_width;
    int input_height = render_height;

    if (is_public_window && amiga_window) {
        lX -= amiga_window->BorderLeft;
        lY -= amiga_window->BorderTop;
        input_width = amiga_window->Width
            - amiga_window->BorderLeft - amiga_window->BorderRight;
        input_height = amiga_window->Height
            - amiga_window->BorderTop - amiga_window->BorderBottom;
    }

#if defined(DETHRACE_FIX_BUGS)
    /* Menus temporarily switch gGraf_spec_index to low resolution even when
     * the actual display remains 640x480.  PDGetMousePosition performs the
     * real-to-menu conversion later, so return coordinates in the permanent
     * display resolution here to avoid scaling them twice. */
    const int game_width = gReal_graf_data_index ? 640 : 320;
    const int game_height = gReal_graf_data_index ? 480 : 200;

    lX = (lX * game_width) / input_width;
    lY = (lY * game_height) / input_height;
#endif

    *pX = lX;
    *pY = lY;
    return 0;
}

static void destroy_window(void) {
    bool was_opengl_mode = is_opengl_mode;

    /* Palette callbacks are allowed to redisplay the most recent frame.
     * Resolution changes destroy that pixelmap immediately after this call,
     * so never carry its address into the newly-created display. */
    last_screen_src = NULL;

    if (ham_buffer) {
        FreeVec(ham_buffer);
        ham_buffer = NULL;
    }
    if (ham_fade_buffer) {
        FreeVec(ham_fade_buffer);
        ham_fade_buffer = NULL;
    }
    if (ham_source_buffer) {
        FreeVec(ham_source_buffer);
        ham_source_buffer = NULL;
    }
    ham_buffer_capacity = 0;
    ham_source_frame_valid = 0;
    ham_reference_palette_valid = 0;
    ham_encoded_frame_valid = 0;
    ham_fade_degree = 256;
    ham_cached_width = 0;
    ham_cached_height = 0;

    if (is_opengl_mode) {
#ifdef DETHRACE_AMIGA_SHARED_MINIGL
        mglDeleteContext();
        MiniGLClose();
#else
        if (mini_CurrentContext) {
            mglDeleteContext();
            mini_CurrentContext = NULL;
        }
#endif
        is_opengl_mode = 0;
        amiga_window = NULL;
        amiga_screen = NULL;
        rp = NULL;
        cm = NULL;
    } else if (amiga_window) {
        ClearPointer(amiga_window);
        CloseWindow(amiga_window);
        amiga_window = NULL;
    }
    if (blank_pointer) {
        FreeVec(blank_pointer);
        blank_pointer = NULL;
    }
    if (amiga_screen && owns_amiga_screen)
        CloseScreen(amiga_screen);
    amiga_screen = NULL;
    rp = NULL;
    cm = NULL;
    owns_amiga_screen = 0;
    is_public_window = 0;
    if (screen_buffers[0]) {
        FreeVec(screen_buffers[0]);
        screen_buffers[0] = NULL;
    }
    if (screen_buffers[1]) {
        FreeVec(screen_buffers[1]);
        screen_buffers[1] = NULL;
    }
    if (CyberGfxBase && !was_opengl_mode) {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
    /* TimerBase belongs to timer.device, not to a library.  Closing it with
     * CloseLibrary() corrupts Exec state.  The process-wide timer device is
     * intentionally retained until process exit. */
}


static void set_key_handler(void (*handler_func)(void)) {
    gKeyHandler_func = handler_func;
}

static void create_window(const char* title, int width, int height, tHarness_window_type window_type) {
    /* A new renderer must not inherit a cached frame from the previous
     * resolution/window. */
    last_screen_src = NULL;
    ham_encoded_frame_valid = 0;
    ham_cached_width = 0;
    ham_cached_height = 0;

    if (window_type == eWindow_type_opengl) {
        (void)title;
#ifdef DETHRACE_AMIGA_SHARED_MINIGL
        if (!MiniGLOpen()) {
            printf("ERROR: unable to open minigl.library v4\n");
            exit(1);
        }
#endif
        mglChooseWindowMode(harness_game_config.start_full_screen ? GL_FALSE : GL_TRUE);
        mglChooseNumberOfBuffers(2);
        mglChoosePixelDepth(16);
        mglChooseVertexBufferSize(4096);
        if (!mglCreateContext(0, 0, width, height)) {
            printf("ERROR: unable to create MiniGL context\n");
#ifdef DETHRACE_AMIGA_SHARED_MINIGL
            MiniGLClose();
#endif
            exit(1);
        }
        /* Do not quantize the game to fractions of the host refresh rate.
         * This renderer is benchmarked independently from display VSync. */
        mglEnableSync(GL_FALSE);
        is_opengl_mode = 1;
        render_width = width;
        render_height = height;
        amiga_window = (struct Window*)mglGetWindowHandle();
        if (!amiga_window) {
            printf("ERROR: MiniGL did not return an Intuition window\n");
            exit(1);
        }
        amiga_screen = amiga_window->WScreen;
        rp = amiga_window->RPort;
        cm = amiga_screen ? amiga_screen->ViewPort.ColorMap : NULL;
        if (harness_game_config.start_full_screen && amiga_screen) {
            ScreenToFront(amiga_screen);
            WindowToFront(amiga_window);
            ActivateWindow(amiga_window);
        }
        ModifyIDCMP(amiga_window, IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY |
            IDCMP_MOUSEMOVE | IDCMP_CLOSEWINDOW);
        initializeAmigaRawKeyNums();
    } else if (is_ham_mode) {
        create_window_and_renderer_pal((char*)title, 0, 0, width, height);
    } else {
        create_window_and_renderer((char*)title, 0, 0, width, height);
    }
    show_cursor(0);
}

static void swap_buffers(br_pixelmap* back_buffer) {
    get_and_handle_message();
    if (!is_opengl_mode)
        gHarness_platform.Renderer_Present(back_buffer);
}

static void palette_changed(br_colour entries[256]) {
    PALETTEENTRY_ palette[256];
    int i;

    for (i = 0; i < 256; i++) {
        palette[i].peRed = BR_RED(entries[i]);
        palette[i].peGreen = BR_GRN(entries[i]);
        palette[i].peBlue = BR_BLU(entries[i]);
        palette[i].peFlags = 0;
    }
    gPalette_impl(palette);
}

static void* get_gl_proc_address(const char* name) {
    return NULL;
}

static void get_viewport(int* x, int* y, float* width_multiplier, float* height_multiplier) {
    *x = 0;
    *y = 0;
    *width_multiplier = 1.0f;
    *height_multiplier = 1.0f;
}

static int Amiga_Harness_Platform_Init(tHarness_platform* platform) {
    depth = harness_game_config.bpp;

    if (harness_game_config.aga_screen) {
        is_aga_mode = 1;
        depth = 8;
    }
    if (harness_game_config.bpp == HAM6_DEPTH) {
        is_ham_mode = 1;
        depth = HAM6_DEPTH;
    }
    if (depth == 16) {
        printf("WARNING: 16-bit palette not supported, using 8-bit.\n");
        depth = 8;
    }
    if (depth == HAM6_DEPTH) {
        is_ham_mode = 1;
        gPalette_impl = set_palette_ham6;
        platform->Renderer_Present = present_screen_ham6;
    } else if (!harness_game_config.start_full_screen && !is_aga_mode) {
        /* A public screen owns its palette, so draw converted ARGB pixels
         * instead of changing the Workbench palette. */
        gPalette_impl = set_palette32;
        platform->Renderer_Present = present_screen32;
    } else if (depth == 8) {
        gPalette_impl = set_palette8;
        platform->Renderer_Present = present_screen8;
    } else if (depth == 16) {
        gPalette_impl = set_palette16;
        platform->Renderer_Present = present_screen16;
    } else {
        gPalette_impl = set_palette32;
        platform->Renderer_Present = present_screen32;
    }

    platform->ProcessWindowMessages = get_and_handle_message;
    platform->Sleep = SleepMs;
    platform->GetTicks = GetTicks;
    platform->ShowCursor = show_cursor;
    platform->GetKeyboardState = get_keyboard_state;
    platform->GetMousePosition = get_mouse_position;
    platform->GetMouseButtons = get_mouse_buttons;
    platform->ShowErrorMessage = show_error_message;
    platform->SetWindowPos = set_window_pos;
    platform->DestroyWindow = destroy_window;
    platform->SetKeyHandler = set_key_handler;
    platform->CreateWindow_ = create_window;
    platform->Swap = swap_buffers;
    platform->PaletteChanged = palette_changed;
    platform->GL_GetProcAddress = get_gl_proc_address;
    platform->GetViewport = get_viewport;
    return 0;
}

const tPlatform_bootstrap Amiga_bootstrap = {
    "amiga",
    "Native AmigaOS AGA, HAM6 and CyberGraphX video backend",
    ePlatform_cap_software | ePlatform_cap_opengl,
    Amiga_Harness_Platform_Init,
};
