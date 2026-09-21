/*
 * Minimal MiniGL alpha-blending workaround diagnostic for AmigaOS.
 *
 * Expected result: one coloured person-shaped sprite on a blue background.
 * If a black square surrounds the sprite, the backend is also ignoring the
 * texture alpha used by GL_SRC_ALPHA blending.
 */
#include <clib/minigl_open_protos.h>
#include <proto/minigl.h>

#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define SPRITE_SIZE 64
#define ESC_RAWKEY 0x45

static FILE *log_file;
static GLubyte sprite_rgba[SPRITE_SIZE * SPRITE_SIZE * 4];
static GLubyte background_rgba[SPRITE_SIZE * SPRITE_SIZE * 4];

static void test_log(const char *text)
{
    if(text == NULL)
        text = "(null)";
    puts(text);
    if(log_file != NULL) {
        fputs(text, log_file);
        fputc('\n', log_file);
        fflush(log_file);
    }
}

static void put_pixel(int x, int y, GLubyte r, GLubyte g, GLubyte b)
{
    GLubyte *pixel;

    if(x < 0 || x >= SPRITE_SIZE || y < 0 || y >= SPRITE_SIZE)
        return;
    pixel = sprite_rgba + (y * SPRITE_SIZE + x) * 4;
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
    pixel[3] = 255;
}

static void fill_rect(int x0, int y0, int x1, int y1,
    GLubyte r, GLubyte g, GLubyte b)
{
    int x, y;
    for(y = y0; y <= y1; ++y)
        for(x = x0; x <= x1; ++x)
            put_pixel(x, y, r, g, b);
}

static void draw_thick_line(int x0, int y0, int x1, int y1,
    int radius, GLubyte r, GLubyte g, GLubyte b)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = dx < 0 ? -dx : dx;
    int abs_dy = dy < 0 ? -dy : dy;
    int i, ox, oy;

    if(abs_dy > steps)
        steps = abs_dy;
    if(steps == 0)
        steps = 1;
    for(i = 0; i <= steps; ++i) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        for(oy = -radius; oy <= radius; ++oy)
            for(ox = -radius; ox <= radius; ++ox)
                put_pixel(x + ox, y + oy, r, g, b);
    }
}

static void make_sprite(void)
{
    int x, y;

    /* Transparent texels deliberately retain black RGB, matching the visible
     * symptom in Carmageddon when alpha testing is ignored. */
    memset(sprite_rgba, 0, sizeof(sprite_rgba));

    /* Head. */
    for(y = 7; y <= 23; ++y) {
        for(x = 22; x <= 42; ++x) {
            int dx = x - 32;
            int dy = y - 15;
            if(dx * dx + dy * dy <= 9 * 9)
                put_pixel(x, y, 255, 204, 128);
        }
    }

    /* Shirt, arms, trousers and shoes. */
    fill_rect(23, 24, 41, 43, 240, 40, 24);
    draw_thick_line(24, 27, 13, 40, 2, 255, 204, 128);
    draw_thick_line(40, 27, 51, 38, 2, 255, 204, 128);
    draw_thick_line(28, 42, 23, 57, 3, 30, 220, 70);
    draw_thick_line(36, 42, 42, 57, 3, 30, 220, 70);
    fill_rect(17, 56, 26, 60, 255, 220, 30);
    fill_rect(39, 56, 48, 60, 255, 220, 30);
}

static void make_background(void)
{
    int x, y;

    for(y = 0; y < SPRITE_SIZE; ++y) {
        for(x = 0; x < SPRITE_SIZE; ++x) {
            GLubyte *pixel = background_rgba +
                (y * SPRITE_SIZE + x) * 4;
            int alternate = ((x >> 3) ^ (y >> 3)) & 1;

            pixel[0] = alternate ? 30 : 20;
            pixel[1] = alternate ? 110 : 70;
            pixel[2] = alternate ? 220 : 150;
            pixel[3] = 255;
        }
    }
}

static void draw_test_frame(GLuint sprite_texture, GLuint background_texture)
{
    const GLfloat left = 192.0f;
    const GLfloat top = 112.0f;
    const GLfloat right = 448.0f;
    const GLfloat bottom = 368.0f;

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Use an opaque texture for the reference background.  This avoids
     * depending on clear colour or untextured-colour handling, while making
     * failed transparency unmistakable as a black rectangle over the grid. */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, background_texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.5f);
    glTexCoord2f(10.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, 0.0f, 0.5f);
    glTexCoord2f(10.0f, 7.5f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, 0.5f);
    glTexCoord2f(0.0f, 7.5f);
    glVertex3f(0.0f, WINDOW_HEIGHT, 0.5f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, sprite_texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    /* PiStorm3D workaround under test: use the uploaded binary texture alpha
     * in the blend unit instead of its currently broken GL_ALPHA_TEST path. */
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(left, top, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(right, top, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(right, bottom, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(left, bottom, 0.0f);
    glEnd();

    glFlush();
    mglSwitchDisplay();
}

static void wait_until_exit(struct Window *window)
{
    int running = 1;

    while(running) {
        struct IntuiMessage *message;
        WaitPort(window->UserPort);
        while((message = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
            ULONG event_class = message->Class;
            UWORD code = message->Code;
            ReplyMsg((struct Message *)message);
            if(event_class == IDCMP_CLOSEWINDOW ||
                (event_class == IDCMP_RAWKEY &&
                    (code & 0x7f) == ESC_RAWKEY))
                running = 0;
        }
    }
}

int main(void)
{
    struct Window *window = NULL;
    GLuint textures[2] = { 0, 0 };
    int minigl_open = 0;
    int context_open = 0;
    int result = 1;

    log_file = fopen("minigl_alpha_sprite_test.log", "w");
    test_log("MiniGL alpha sprite test: starting");

    if(!MiniGLOpen()) {
        test_log("ERROR: unable to open minigl.library");
        goto cleanup;
    }
    minigl_open = 1;

    mglChooseWindowMode(GL_TRUE);
    mglChooseNumberOfBuffers(2);
    /* Match Carmageddon's working MiniGL context exactly.  The previous
     * diagnostic requested 32 bits, so it was not testing the same path as
     * the game on classic MiniGL/Warp3D. */
    mglChoosePixelDepth(16);
    /* Match Carmageddon's working MiniGL setup. */
    mglChooseVertexBufferSize(4096);
    if(!mglCreateContext(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        test_log("ERROR: unable to create MiniGL context");
        goto cleanup;
    }
    context_open = 1;
    mglEnableSync(GL_FALSE);

    window = (struct Window *)mglGetWindowHandle();
    if(window != NULL && window->UserPort != NULL) {
        ModifyIDCMP(window, IDCMP_RAWKEY | IDCMP_CLOSEWINDOW);
        SetWindowTitles(window, "MiniGL RGBA alpha-blend workaround",
            (CONST_STRPTR)-1);
    } else {
        window = NULL;
        test_log("MiniGL returned no Intuition window handle; using console input");
    }

    test_log((const char *)glGetString(GL_VENDOR));
    test_log((const char *)glGetString(GL_RENDERER));
    test_log((const char *)glGetString(GL_VERSION));

    make_background();
    make_sprite();
    glGenTextures(2, textures);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        SPRITE_SIZE, SPRITE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        background_rgba);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        SPRITE_SIZE, SPRITE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, sprite_rgba);

    test_log("Blend workaround: expected person over blue grid; no black rectangle");
    draw_test_frame(textures[1], textures[0]);
    result = 0;
    if(window != NULL) {
        wait_until_exit(window);
    } else {
        test_log("Press ENTER in this console to exit");
        (void)getchar();
    }

cleanup:
    if(textures[0] != 0 && context_open)
        glDeleteTextures(2, textures);
    if(context_open)
        mglDeleteContext();
    if(minigl_open)
        MiniGLClose();
    test_log(result == 0 ? "MiniGL alpha sprite test: finished" :
        "MiniGL alpha sprite test: failed");
    if(log_file != NULL)
        fclose(log_file);
    return result;
}
