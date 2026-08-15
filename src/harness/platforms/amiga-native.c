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
#include <inline/timer.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "harness.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "common/graphics.h"
#include <SDI_compiler.h>

#define __USE_INLINE__ 1
#include <proto/dos.h>
#include <proto/asl.h>
#include <libraries/asl.h>

#include "amiga_rawcode_to_dinput.h"

extern void c2p1x1_8_c5_bm_040(int chunkyx __asm("d0"), int chunkyy __asm("d1"), int offsx __asm("d2"), int offsy __asm("d3"), void* c2pscreen __asm("a0"), struct BitMap* bitmap __asm("a1"));
extern void c2p1x1_6_c5_bm_040(int chunkyx __asm("d0"), int chunkyy __asm("d1"), int offsx __asm("d2"), int offsy __asm("d3"), void* c2pscreen __asm("a0"), struct BitMap* bitmap __asm("a1"));
extern void c2p1x1_4_c5_bm(int chunkyx __asm("d0"), int chunkyy __asm("d1"), int offsx __asm("d2"), int offsy __asm("d3"), void* c2pscreen __asm("a0"), struct BitMap* bitmap __asm("a1"));

struct Library *CyberGfxBase = NULL;
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
static bool is_ham_mode = 0;
static bool is_aga_mode = 0;
extern int gGraf_spec_index;

static UWORD current_r = 0, current_g = 0, current_b = 0;
static int ham_stats[4] = {0, 0, 0, 0};  /* Stats for HOLD, MODB, MODR, MODG */

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
                printf("Width: %ld\n", sm->sm_DisplayWidth);
                printf("Height: %ld\n", sm->sm_DisplayHeight);
                printf("Depth: %d\n", sm->sm_DisplayDepth);
                printf("ModeID: 0x%08x\n", sm->sm_DisplayID);
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

static void get_keyboard_state(unsigned int count, uint8_t* buffer) {
    memcpy(buffer, directinput_key_state, count);
}

static int amiga_MouseButton1 = 0;
static int amiga_MouseButton2 = 0;

static int get_and_handle_message(MSG_* msg) {
    struct IntuiMessage *imsg;
    int dinput_key;
    BOOL quit = FALSE;
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
                msg->message = WM_QUIT;
                quit = TRUE;
                break;
            case IDCMP_MOUSEMOVE:
            case IDCMP_MOUSEBUTTONS:
                handle_amiga_mouse_event(msg, amiga_window);
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
                break;
            default:
                break;
        }
    }
    return quit ? 1 : 0;
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
        printf("Creating HAM6 window and renderer\n");
        modeID = LORES_KEY | HAM_KEY;
    }
    else {
        printf("Creating AGA window and renderer\n");
        //modeID = LORES_KEY ;//| HIRESLACE_KEY;
        modeID = 0x00011000; // PAL AGA LORES
        width = 320;
        height = 256;
        if (gGraf_spec_index) {
            printf("Using high-res mode\n");
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
    
    printf("PAL Screen opened: width=%d, height=%d, depth=%d\n",
           amiga_screen->Width, amiga_screen->Height, amiga_screen->RastPort.BitMap->Depth);
    
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
    if (!ham_buffer) {
        printf("ERROR: Failed to allocate HAM buffer\n");
        CloseWindow(amiga_window);
        CloseScreen(amiga_screen);
        exit(1);
    }
    
    return amiga_window;
}

static void* create_window_and_renderer(char* title, int x, int y, int width, int height) {
    initializeAmigaRawKeyNums();

    ULONG modeID = 0;
    if (!is_aga_mode) {
        CyberGfxBase = OpenLibrary("cybergraphics.library", 37);
        if (!CyberGfxBase) {
            printf("ERROR: can't open cybergraphics.library V37.\n");
            exit(1);
        }
    }
    printf("Requested depth: %d\n", depth);
       
    /* Open HAM6 screen with optimal mode ID */
    if (is_ham_mode) {
        printf("Creating HAM6 window and renderer\n");
        modeID = LORES_KEY | HAM_KEY;

    }
    else if (is_aga_mode) {
        printf("Creating AGA window and renderer\n");
        //modeID = LORES_KEY ;//| HIRESLACE_KEY;
        modeID = 0x00011000; // PAL AGA LORES
        width = 320;
        height = 200;//56;
        if (gGraf_spec_index) {
            printf("Using high-res mode\n");
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

    BytesPerRow = ((amiga_screen->Width + 15) & ~15);

    printf(" Screen opened: width=%d, height=%d, depth=%4ld\n",
           amiga_screen->Width, amiga_screen->Height, GetBitMapAttr(amiga_screen->RastPort.BitMap,BMA_DEPTH));

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
    if (!rp || !src || !src->pixels) {
        printf("ERROR: `rp` or `src->pixels` is NULL!\n");
        return;
    }

    int screen_width = amiga_screen->Width;
    int screen_height = amiga_screen->Height;

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

    int changes_detected = 0;
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            int src_index = y * src->width + x;
            int dst_index = y * BytesPerRow + x;

            if (x < src->width && y < src->height) {
                //UBYTE new_pixel = src_pixels[src_index];  /* Direct index for 8-bit mode */
                UBYTE new_pixel = converted_palette[src_pixels[src_index]];
                if (temp_buffer[dst_index] != new_pixel) {
                    temp_buffer[dst_index] = new_pixel;
                    changes_detected = 1;
                }
            }
        }
    }

    if (changes_detected) {
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
    }

    current_buffer = 1 - current_buffer;
    //last_screen_src = src;

    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}
//#if 1

/* Find the best HAM6 representation for a pixel */
static UBYTE find_best_ham6_pixel(UBYTE r, UBYTE g, UBYTE b) {
    /* Scale down from 8-bit to 4-bit color */
    UBYTE r4 = r >> 4;
    UBYTE g4 = g >> 4;
    UBYTE b4 = b >> 4;
    
    /* Option 1: Use a color register */
    int best_reg = 0;
    int best_reg_error = 999999;
    
    for (int i = 0; i < 16; i++) {
        /* Get color from register */
        ULONG reg_color = converted_palette[i];
        UBYTE reg_r = (reg_color >> 16) & 0xFF;
        UBYTE reg_g = (reg_color >> 8) & 0xFF;
        UBYTE reg_b = reg_color & 0xFF;
        
        /* Convert 8-bit to 4-bit for comparison */
        reg_r >>= 4;
        reg_g >>= 4;
        reg_b >>= 4;
        
        /* Calculate error */
        int error = color_distance_squared(r4, g4, b4, reg_r, reg_g, reg_b);
        
        if (error < best_reg_error) {
            best_reg_error = error;
            best_reg = i;
        }
    }
    
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
                ham_buffers[render_buffer][dst_index] = find_best_ham6_pixel(r, g, b);
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
/* Present screen using HAM6 mode */
static void present_screen_ham6(br_pixelmap* src) {
    if (!rp || !src || !src->pixels || !ham_buffer) {
        printf("ERROR: Invalid parameters for HAM6 rendering\n");
      return;
    }

    /* Reset HAM statistics */
    ham_stats[0] = ham_stats[1] = ham_stats[2] = ham_stats[3] = 0;
    
    int screen_width = amiga_screen->Width;
    int screen_height = amiga_screen->Height;
    UBYTE *src_pixels = (UBYTE *)src->pixels;
    
    /* Reset current color at beginning of each frame */
    current_r = current_g = current_b = 0;
    
    /* Convert image to HAM6 format */
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
                ham_buffer[dst_index] = find_best_ham6_pixel(r, g, b);
            } else {
                /* Out of bounds - use black */
                ham_buffer[dst_index] = 0;
            }
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
#if USE_WPAL8
    /* Display the HAM6 image */
    for (int y = 0; y < screen_height; y++) {
        WritePixelLine8(rp, 0, y, screen_width, &ham_buffer[y * screen_width], NULL);
    }
#else
    struct BitMap ham6_bm = {
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
        0,             // scroffsx
        0,             // scroffsy
        ham_buffer,    // Zakładam, że ham_buffer zawiera dane chunky
        &ham6_bm
    );
#endif    
    last_screen_src = src;
    
    /* Apply FPS limiting */
    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

void set_palette_ham6_normal(PALETTEENTRY_ *pal) {
    // Ustaw bazowe 16 kolorów dla HAM6
    for (int i = 0; i < 16; i++) {
        // Konwersja z wartości 8-bit (0-255) na 4-bit (0-15)
        UBYTE r4 = (pal[i].peRed * 15) / 255;
        UBYTE g4 = (pal[i].peGreen * 15) / 255;
        UBYTE b4 = (pal[i].peBlue * 15) / 255;
        
        // Ustaw kolor w palecie Amigi
        SetRGB4(&amiga_screen->ViewPort, i, r4, g4, b4);
        
        // Ustaw ten sam kolor w converted_palette (format 24-bit)
        converted_palette[i] = (pal[i].peRed << 16) | (pal[i].peGreen << 8) | pal[i].peBlue;
    }
    
    // Opcjonalnie: ustaw pozostałe kolory (jeśli są używane poza HAM)
    for (int i = 16; i < 256; i++) {
        converted_palette[i] = (pal[i].peRed << 16) | (pal[i].peGreen << 8) | pal[i].peBlue;
    }
}
/* Initialize HAM6 palette */
static void set_palette_ham6_loading(PALETTEENTRY_* pal) {
  /* Ustawienie bazowej palety 16 kolorów z pliku IFF */

    /* Ustawienie bazowej palety 16 kolorów dla menu Carmageddon */
    /* Ustawienie bazowej palety 16 kolorów na podstawie podanych wartości */
    SetRGB4(&amiga_screen->ViewPort, 0, 0, 0, 0);      /* Czarny */
    SetRGB4(&amiga_screen->ViewPort, 1, 5, 0, 0);      /* Ciemny czerwony */
    SetRGB4(&amiga_screen->ViewPort, 2, 9, 0, 0);      /* Średni czerwony */
    SetRGB4(&amiga_screen->ViewPort, 3, 12, 0, 0);     /* Jasny czerwony */
    SetRGB4(&amiga_screen->ViewPort, 4, 15, 0, 0);     /* Czysty czerwony */
    SetRGB4(&amiga_screen->ViewPort, 5, 15, 4, 4);     /* Jasny czerwony z odrobiną zieleni i niebieskiego */
    SetRGB4(&amiga_screen->ViewPort, 6, 15, 7, 7);     /* Jaśniejszy czerwony z więcej zielonego i niebieskiego */
    SetRGB4(&amiga_screen->ViewPort, 7, 15, 11, 11);   /* Bardzo jasny czerwony */
    SetRGB4(&amiga_screen->ViewPort, 8, 3, 1, 0);      /* Ciemny brązowy */
    SetRGB4(&amiga_screen->ViewPort, 9, 6, 1, 0);      /* Brązowy */
    SetRGB4(&amiga_screen->ViewPort, 10, 9, 1, 0);     /* Jaśniejszy brązowy */
    SetRGB4(&amiga_screen->ViewPort, 11, 12, 2, 0);    /* Ciemny pomarańczowy */
    SetRGB4(&amiga_screen->ViewPort, 12, 15, 2, 0);    /* Pomarańczowy */
    SetRGB4(&amiga_screen->ViewPort, 13, 15, 6, 4);    /* Jaśniejszy pomarańczowy */
    SetRGB4(&amiga_screen->ViewPort, 14, 15, 9, 8);    /* Jasny łososiowy */
    SetRGB4(&amiga_screen->ViewPort, 15, 15, 12, 12);  /* Bardzo jasny różowy (prawie biały z odcieniem czerwieni) */

    /* Te same kolory w converted_palette */
    converted_palette[0] = 0x000000;   /* Czarny */
    converted_palette[1] = 0x550000;   /* Ciemny czerwony */
    converted_palette[2] = 0x990000;   /* Średni czerwony */
    converted_palette[3] = 0xCC0000;   /* Jasny czerwony */
    converted_palette[4] = 0xFF0000;   /* Czysty czerwony */
    converted_palette[5] = 0xFF4444;   /* Jasny czerwony z odrobiną zieleni i niebieskiego */
    converted_palette[6] = 0xFF7777;   /* Jaśniejszy czerwony z więcej zielonego i niebieskiego */
    converted_palette[7] = 0xFFBBBB;   /* Bardzo jasny czerwony */
    converted_palette[8] = 0x331100;   /* Ciemny brązowy */
    converted_palette[9] = 0x661100;   /* Brązowy */
    converted_palette[10] = 0x991100;  /* Jaśniejszy brązowy */
    converted_palette[11] = 0xCC2200;  /* Ciemny pomarańczowy */
    converted_palette[12] = 0xFF2200;  /* Pomarańczowy */
    converted_palette[13] = 0xFF6644;  /* Jaśniejszy pomarańczowy */
    converted_palette[14] = 0xFF9988;  /* Jasny łososiowy */
    converted_palette[15] = 0xFFCCCC;  /* Bardzo jasny różowy (prawie biały z odcieniem czerwieni) */
        

    /* Convert full palette for reference in HAM algorithm */
    for (int i = 16; i < 256; i++) {
        UBYTE r = pal[i].peRed;
        UBYTE g = pal[i].peGreen;
        UBYTE b = pal[i].peBlue;
        
        /* Convert to 24-bit color */
       converted_palette[i] = (r << 16) | (g << 8) | b;
    }
    #if 0
        /* Normal 8-bit palette setup */
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = (0xFF << 24) | (pal[i].peRed << 16) | (pal[i].peGreen << 8) | pal[i].peBlue;
    }
    #endif
    if (last_screen_src != NULL) {
        present_screen_ham6(last_screen_src);
    }
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

    printf("DEBUG: Setting 16-bit and 32-bit palettes\n");

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
        0, 0,
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
    if (toggle) {
        //SetPointer(amiga_window, (UBYTE*)~0, 0, 0, 0, 0);
    } else {
        SetPointer(amiga_window, (UBYTE*)~0, 0, 0, 0, 0);
    }
    return 0;
}

int show_error_message(void* window, char* text, char* caption) {
    printf("ERROR:%s\n", text);
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

void handle_amiga_mouse_event(struct IntuiMessage *msg, struct Window *win) {
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

#if defined(DETHRACE_FIX_BUGS)
    lX = (lX * 320) / render_width;
    lY = (lY * 200) / render_height;
#endif

    *pX = lX;
    *pY = lY;
    return 0;
}

void destroy_window(void* window) {
    if (ham_buffer) {
        FreeVec(ham_buffer);
        ham_buffer = NULL;
    }

    if (window)
        CloseWindow(window);
    if (amiga_screen)
        CloseScreen(amiga_screen);
    if (screen_buffers[0])
        FreeVec(screen_buffers[0]);
    if (screen_buffers[1])
        FreeVec(screen_buffers[1]);
    if (CyberGfxBase)
        CloseLibrary(CyberGfxBase);      
    if (TimerBase)
        CloseLibrary(TimerBase);
}


/* Update Harness_Platform_Init to include HAM6 support */
void Harness_Platform_Init(tHarness_platform* platform) {
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
    printf("Harness depth: %d\n", depth);
    platform->CreateWindowAndRenderer = create_window_and_renderer;

    if (depth == HAM6_DEPTH) {
        is_ham_mode = 1;
        platform->Renderer_SetPalette = set_palette_ham6;
        platform->Renderer_Present = present_screen_ham6;
        platform->CreateWindowAndRenderer = create_window_and_renderer_pal;
    } else if (depth == 8) {
        platform->Renderer_SetPalette = set_palette8;
        platform->Renderer_Present = present_screen8;
        if (is_aga_mode)
            platform->CreateWindowAndRenderer = create_window_and_renderer;
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
    platform->ShowCursor = show_cursor;
    platform->GetKeyboardState = get_keyboard_state;
    platform->GetMousePosition = get_mouse_position;
    platform->GetMouseButtons = get_mouse_buttons;
    platform->ShowErrorMessage = show_error_message;
    platform->SetWindowPos = set_window_pos;
    platform->DestroyWindow = destroy_window;
}