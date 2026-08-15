#include <SDL.h>

#include "harness.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "sdl12_scancode_to_dinput.h"
#include <proto/intuition.h>
#include <inline/intuition.h>

#ifdef _WIN32
#include <windows.h>
#endif
SDL_Surface* screen;
uint32_t converted_palette[256];
br_pixelmap* last_screen_src;
int render_width, render_height, depth;

Uint32 last_frame_time;

uint8_t directinput_key_state[SDLK_LAST];

static void* create_window_and_renderer(char* title, int x, int y, int width, int height) {
    int video_flags = 0;
    render_width = width;
    render_height = height;

    if (depth == 8) {
        video_flags = SDL_HWPALETTE;
    }
    LOG_INFO("Using %d bit mode", depth);

    initializeSDL12KeyNums();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_PANIC("SDL_INIT_VIDEO error: %s", SDL_GetError());
    }

    if (harness_game_config.start_full_screen) {
        video_flags |= SDL_FULLSCREEN;
    }

    screen = SDL_SetVideoMode(width, height, depth, video_flags);
    SDL_WM_SetCaption("Carmageddon", NULL);

    if (screen == NULL) {
        LOG_PANIC("SDL_SetVideoMode failed (%s)", SDL_GetError());
    }

    return screen;
}

static int set_window_pos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    // SDL_SetWindowPosition(hWnd, x, y);
    if (nWidth == 320 && nHeight == 200) {
        nWidth = 640;
        nHeight = 400;
    }
   // SDL_UpdateRect(hWnd, x, y, nWidth, nHeight);
    return 0;
}

static void destroy_window(void* hWnd) {
    SDL_FreeSurface(screen);
    SDL_Quit();
    screen = NULL;
}

// Checks whether the `flag_check` is the only modifier applied.
// e.g. is_only_modifier(event.key.keysym.mod, KMOD_ALT) returns true when only the ALT key was pressed
static int is_only_key_modifier(int modifier_flags, int flag_check) {
    return (modifier_flags & flag_check) && (modifier_flags & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_META)) == (modifier_flags & flag_check);
}

static int get_and_handle_message(MSG_* msg) {
    SDL_Event event;
    int dinput_key;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_RETURN) {
                if (event.key.type == SDL_KEYDOWN) {
                    if ((event.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_META))) {
                        // Ignore keydown of RETURN when used together with some modifier
                        return 0;
                    }
                } else if (event.key.type == SDL_KEYUP) {
                    if (is_only_key_modifier(event.key.keysym.mod, KMOD_ALT)) {
                        SDL_WM_ToggleFullScreen(screen);
                    }
                }
            }

            // Map incoming SDL scancode to DirectInput DIK_* key code.
            // https://github.com/DanielGibson/Snippets/blob/master/sdl2_scancode_to_dinput.h
            dinput_key = sdl12KeyToDirectInputKeyNum[event.key.keysym.sym];
            if (dinput_key == 0) {
                LOG_WARN("unexpected key %s (%d)", SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.sym);
                return 0;
            }
            // DInput expects high bit to be set if key is down
            // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418261(v=vs.85)
            directinput_key_state[dinput_key] = (event.type == SDL_KEYDOWN ? 0x80 : 0);
            break;

        case SDL_QUIT:
            msg->message = WM_QUIT;
            return 1;
        }
    }
    return 0;
}

static void get_keyboard_state(unsigned int count, uint8_t* buffer) {
    memcpy(buffer, directinput_key_state, count);
}

static int get_mouse_buttons(int* pButton1, int* pButton2) {
    int state = SDL_GetMouseState(NULL, NULL);
    *pButton1 = state & SDL_BUTTON_LMASK;
    *pButton2 = state & SDL_BUTTON_RMASK;
    return 0;
}

static int get_mouse_position(int* pX, int* pY) {
    int lX, lY;

    SDL_GetMouseState(pX, pY);
    lX = *pX;
    lY = *pY;
//    SDL_RenderWindowToLogical(renderer, *pX, *pY, &lX, &lY);

#if defined(DETHRACE_FIX_BUGS)
    // In hires mode (640x480), the menus are still rendered at (320x240),
    // so prescale the cursor coordinates accordingly.
    lX *= 320;
    lX /= render_width;
    lY *= 200;
    lY /= render_height;
#endif
    *pX = (int)lX;
    *pY = (int)lY;
    return 0;
}

static void limit_fps(void) {
    Uint32 now = SDL_GetTicks();
    if (last_frame_time != 0) {
        unsigned int frame_time = now - last_frame_time;
        last_frame_time = now;
        if (frame_time < 100) {
            int sleep_time = (1000 / harness_game_config.fps) - frame_time;
            if (sleep_time > 5) {
                gHarness_platform.Sleep(sleep_time);
            }
        }
    }
    last_frame_time = SDL_GetTicks();
}
static unsigned int dim = 0;

static void present_screen32(br_pixelmap* src) {
    // fastest way to convert 8 bit indexed to 32 bit
    uint8_t* src_pixels = src->pixels;
    uint32_t* dest_pixels;
    if (dim == 0)
        dim = src->height * src->width;
    //uint16_t* dest_pixels;
   // SDL_LockSurface(screen);
    dest_pixels = screen->pixels;

   for (unsigned int i = 0; i < dim; i++) {
        *dest_pixels = converted_palette[*src_pixels];
        dest_pixels++;
        src_pixels++;
    }
    /*for (int i = 0; i < dim; i += 4) {
    dest_pixels[0] = converted_palette[src_pixels[0]];
    dest_pixels[1] = converted_palette[src_pixels[1]];
    dest_pixels[2] = converted_palette[src_pixels[2]];
    dest_pixels[3] = converted_palette[src_pixels[3]];
    dest_pixels += 4;
    src_pixels += 4;
}*/
   // SDL_UnlockSurface(screen);

    SDL_Flip(screen);

    last_screen_src = src;

    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}
static void set_palette32(PALETTEENTRY_* pal) {

    for (int i = 0; i < 256; i++) {
            converted_palette[i] = (0xff << 24 | pal[i].peRed << 16 | pal[i].peGreen << 8 | pal[i].peBlue);
        }

    if (last_screen_src != NULL) {
        present_screen32(last_screen_src);
    }
}

static void present_screen16(br_pixelmap* src) {
    // fastest way to convert 8 bit indexed to 16 bit
    uint8_t* src_pixels = src->pixels;
//    uint32_t* dest_pixels;
    uint16_t* dest_pixels;
    //SDL_LockSurface(screen);
    dest_pixels = screen->pixels;
    if (dim == 0)
        dim = src->height * src->width;
    
    for (unsigned int i = 0; i < dim; i++) {
        *dest_pixels = converted_palette[*src_pixels];
        dest_pixels++;
        src_pixels++;
    }
    //SDL_UnlockSurface(screen);

    SDL_Flip(screen);

    last_screen_src = src;

    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}
static void set_palette16(PALETTEENTRY_* pal) {
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = ((pal[i].peRed >> 3 << 11) | (pal[i].peGreen >> 2 << 5) | pal[i].peBlue >> 3);
    }

    if (last_screen_src != NULL) {
        present_screen16(last_screen_src);
    }
}

static void present_screen8(br_pixelmap* src) {
    //uint8_t* src_pixels = src->pixels;
    uint8_t* dest_pixels;
    //SDL_LockSurface(screen);
    dest_pixels = (uint8_t*)screen->pixels;

    memcpy(dest_pixels, src->pixels/*src_pixels*/, src->height * src->width);
    //CopyMem(src->pixels, dest_pixels, src->height * src->width);
    //SDL_UnlockSurface(screen);

    SDL_Flip(screen);

    last_screen_src = src;

    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

static void set_palette8(PALETTEENTRY_* pal) {
    SDL_Color colors[256];
    for (int i = 0; i < 256; i++) {
        colors[i].r = pal[i].peRed;
        colors[i].g = pal[i].peGreen;
        colors[i].b = pal[i].peBlue;
    }
    SDL_SetPalette(screen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);

    if (last_screen_src != NULL) {
        present_screen8(last_screen_src);
    }
}

int show_error_message(void* window, char* text, char* caption) {
    //fprintf(stderr, "%s", text);
#ifdef _WIN32
    MessageBoxA(NULL, text, caption, MB_ICONERROR);
#endif
//    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption, text, window);

    struct EasyStruct es = {
        sizeof(struct EasyStruct), 0,
        (unsigned char*)caption,
          (unsigned char*)text,
            (unsigned char*)"Abort",
    }; 
    EasyRequest(NULL, &es, NULL);

    return 0;
}

void Harness_Platform_Init(tHarness_platform* platform) {
    platform->ProcessWindowMessages = get_and_handle_message;
    platform->Sleep = SDL_Delay;
    platform->GetTicks = SDL_GetTicks;
    platform->CreateWindowAndRenderer = create_window_and_renderer;
    platform->ShowCursor = SDL_ShowCursor;
    platform->SetWindowPos = set_window_pos;
    platform->DestroyWindow = destroy_window;
    platform->GetKeyboardState = get_keyboard_state;
    platform->GetMousePosition = get_mouse_position;
    platform->GetMouseButtons = get_mouse_buttons;
    platform->ShowErrorMessage = show_error_message;
    depth = harness_game_config.bpp;
    if (depth > 31)
        depth = 32;
    else if (depth > 15 && depth < 32)
        depth = 16;
     else
        depth = 8;

    if (depth == 8) {
        platform->Renderer_SetPalette = set_palette8;
        platform->Renderer_Present = present_screen8;

    }
    else if (depth == 16) {
        platform->Renderer_SetPalette = set_palette16;
        platform->Renderer_Present = present_screen16;
    }
    else {
        platform->Renderer_SetPalette = set_palette32;
        platform->Renderer_Present = present_screen32;
    }

}
