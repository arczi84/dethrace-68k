/*
 * Standalone BRender -> 3dfx driver -> MiniGL perspective diagnostic.
 *
 * The application itself only supplies an ordinary textured BRender model.
 * BRender's original 3dfx renderer converts it to screen X/Y plus Glide's
 * oow=1/W, sow=U/W and tow=V/W convention.  The Amiga Glide shim then sends
 * that unchanged convention to minigl.library.
 */
#include <brender.h>

#include <clib/minigl_open_protos.h>
#include <proto/minigl.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <string.h>

#define TEST_WIDTH 640
#define TEST_HEIGHT 480
#define TEX_SIZE 64
#define ESC_RAWKEY 0x45
#define ONE_RAWKEY 0x01
#define TWO_RAWKEY 0x02
#define THREE_RAWKEY 0x03
#define FOUR_RAWKEY 0x04
#define FIVE_RAWKEY 0x05
#define SIX_RAWKEY 0x06
#define LEFT_RAWKEY 0x4f
#define RIGHT_RAWKEY 0x4e
#define UP_RAWKEY 0x4c
#define DOWN_RAWKEY 0x4d

static FILE *log_file;
static int perspective_mode = 1;
#ifdef BRENDER_DEPTH_STRESS
/* Confirmed PiStorm3D depth-glitch reproduction point. */
static br_scalar camera_distance = BR_SCALAR(-6.7);
#endif

void FXA_SetPerspectiveDiagnosticMode(int mode);

/* The shared MiniGL library owns a private CyberGraphX base, while the
 * BRender 3dfx shim uses the application's process-global base for its LFB
 * copy path.  Keep the two lifetimes independent, as the game does. */
struct Library *CyberGfxBase = NULL;

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

void BR_CALLBACK _BrBeginHook(void)
{
    struct br_device *BR_EXPORT BrDrv1SoftRendBegin(char *arguments);
    struct br_device *BR_EXPORT BrDrv13DFXAmigaBegin(char *arguments);

    /* SoftRend performs BRender's model/view/projection and clipping, then
     * feeds triangles to the primitive library owned by the 3dfx output.
     * It is a renderer front end, not the software pixel rasterizer. */
    BrDevAddStatic(NULL, BrDrv1SoftRendBegin, NULL);
    BrDevAddStatic(NULL, BrDrv13DFXAmigaBegin, NULL);
}

void BR_CALLBACK _BrEndHook(void)
{
}

static int handle_events(struct Window *window, br_actor *camera, int *redraw)
{
    struct IntuiMessage *message;
    int running = 1;

    if(window == NULL || window->UserPort == NULL)
        return running;

    while((message = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        ULONG event_class = message->Class;
        UWORD code = message->Code;
        ReplyMsg((struct Message *)message);

        if(event_class == IDCMP_CLOSEWINDOW ||
            (event_class == IDCMP_RAWKEY && (code & 0x7f) == ESC_RAWKEY))
            running = 0;
        else if(event_class == IDCMP_REFRESHWINDOW) {
            BeginRefresh(window);
            EndRefresh(window, TRUE);
            *redraw = 1;
        }
        else if(event_class == IDCMP_RAWKEY && (code & 0x80) == 0) {
            int new_mode = 0;
            if(code == ONE_RAWKEY) new_mode = 1;
            if(code == TWO_RAWKEY) new_mode = 2;
            if(code == THREE_RAWKEY) new_mode = 3;
            if(code == FOUR_RAWKEY) new_mode = 4;
            if(new_mode != 0 && new_mode != perspective_mode) {
                perspective_mode = new_mode;
                FXA_SetPerspectiveDiagnosticMode(perspective_mode);
                *redraw = 1;
                if(perspective_mode == 1)
                    test_log("Mode 1: Glide texture Q (sow, tow, oow)");
                else if(perspective_mode == 2)
                    test_log("Mode 2: affine U/V control");
                else if(perspective_mode == 3)
                    test_log("Mode 3: reconstructed vertex W");
                else
                    test_log("Mode 4: reconstructed eye XYZ via glFrustum");
            }
#ifdef BRENDER_DEPTH_STRESS
            else if(code == LEFT_RAWKEY || code == DOWN_RAWKEY ||
                code == RIGHT_RAWKEY || code == UP_RAWKEY ||
                code == FIVE_RAWKEY || code == SIX_RAWKEY) {
                char line[96];
                long camera_tenths;
                int camera_negative;

                if(code == FIVE_RAWKEY)
                    camera_distance = BR_SCALAR(-6.7);
                else if(code == SIX_RAWKEY)
                    camera_distance = BR_SCALAR(-6.8);
                else if(code == UP_RAWKEY)
                    camera_distance -= BR_SCALAR(0.1);
                else if(code == DOWN_RAWKEY)
                    camera_distance += BR_SCALAR(0.1);
                else if(code == LEFT_RAWKEY)
                    camera_distance -= BR_SCALAR(2.0);
                else
                    camera_distance += BR_SCALAR(2.0);

                /* Permit travelling through the complete scene.  The old
                 * lower clamp at Z=3 stopped the camera before the first
                 * buildings and prevented testing the problematic distance
                 * ranges from inside the corridor. */
                if(camera_distance < BR_SCALAR(-75.0))
                    camera_distance = BR_SCALAR(-75.0);
                if(camera_distance > BR_SCALAR(80.0))
                    camera_distance = BR_SCALAR(80.0);

                BrMatrix34Translate(&camera->t.t.mat, BR_SCALAR(0),
                    BR_SCALAR(0), camera_distance);
                /* The libnix/noixemul printf used by this Amiga build does
                 * not include floating-point formatting.  Convert to tenths
                 * explicitly so the log remains useful without pulling in a
                 * larger printf implementation. */
                camera_negative = camera_distance < BR_SCALAR(0.0);
                camera_tenths = (long)((camera_negative ? -camera_distance :
                    camera_distance) * BR_SCALAR(10.0) + BR_SCALAR(0.5));
                sprintf(line, "Camera Z: %s%ld.%ld",
                    camera_negative ? "-" : "", camera_tenths / 10,
                    camera_tenths % 10);
                test_log(line);
                *redraw = 1;
            }
#else
            (void)camera;
#endif
        }
    }

    return running;
}

#ifdef BRENDER_DEPTH_STRESS
static br_actor *add_box(br_actor *parent, br_model *model,
    br_material *material, br_scalar x, br_scalar y, br_scalar z,
    br_scalar sx, br_scalar sy, br_scalar sz, br_angle yaw)
{
    br_actor *actor = BrActorAdd(parent, BrActorAllocate(BR_ACTOR_MODEL, NULL));

    if(actor == NULL)
        return NULL;
    actor->model = model;
    actor->material = material;
    actor->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Scale(&actor->t.t.mat, sx, sy, sz);
    if(yaw != 0)
        BrMatrix34PreRotateY(&actor->t.t.mat, yaw);
    actor->t.t.mat.m[3][0] = x;
    actor->t.t.mat.m[3][1] = y;
    actor->t.t.mat.m[3][2] = z;
    return actor;
}

static int make_depth_scene(br_actor *world,
    br_model *row1_model, br_material *row1_material,
    br_model *row2_model, br_material *row2_material,
    br_model *row3_model, br_material *row3_material,
    br_model *row4_model, br_material *row4_material,
    br_model *road_model, br_material *road_material)
{
    /* A small city canyon, deliberately using long triangles and a very wide
     * depth range.  The geometry is added in a mixed near/far order, like a
     * real BRender world.  Correct Z buffering must never allow a distant box
     * to appear through a nearer one while the camera distance is changed. */
    static const struct {
        br_scalar x, y, z;
        br_scalar sx, sy, sz;
        int yaw_degrees;
    } boxes[] = {
        { 0.0,-2.15,-19.0, 4.2,0.10,30.0,  0}, /* long road */
        {-4.5, 0.0, -2.0, 2.1,4.5, 4.0,  0}, /* near left */
        { 4.4, 0.2, -4.0, 2.0,4.8, 5.5,  0}, /* near right */
        {-4.8, 0.7,-13.0, 2.3,5.2, 5.0,  0},
        { 4.7, 1.0,-16.0, 2.2,5.5, 5.0,  0},
        {-5.2, 1.4,-27.0, 2.8,6.0, 6.5,  0},
        { 5.0, 1.7,-30.0, 2.6,6.5, 7.0,  0},
        { 0.0, 3.7,-18.0, 3.6,0.35,0.45,  0}, /* bridge */
        {-1.7, 0.3,-39.0, 2.0,7.5, 2.0,  0}, /* far towers */
        { 2.3, 1.0,-45.0, 2.5,8.2, 2.5,  0},
        {-1.5,-1.92,-16.0, 0.08,0.08, 8.0,  0}, /* road separators */
        { 1.5,-1.92,-34.0, 0.08,0.08, 8.0,  0},
        { 0.0, 0.2,-58.0, 7.5,7.5, 1.0,  0}  /* distant backdrop */
    };
    unsigned int i;

    for(i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i) {
        br_model *model = row1_model;
        br_material *material = row1_material;

        if(i == 0 || i == 10 || i == 11) {
            model = road_model;
            material = road_material;
        }
        else if((i >= 3 && i <= 4) || i == 7) {
            model = row2_model;
            material = row2_material;
        }
        else if(i >= 5 && i <= 6) {
            model = row3_model;
            material = row3_material;
        }
        else if((i >= 8 && i <= 9) || i == 12) {
            model = row4_model;
            material = row4_material;
        }

        if(add_box(world, model, material,
            boxes[i].x, boxes[i].y, boxes[i].z,
            boxes[i].sx, boxes[i].sy, boxes[i].sz,
            BrDegreeToAngle(BR_SCALAR(boxes[i].yaw_degrees))) == NULL)
            return 0;
    }
    return 1;
}
#endif

static br_pixelmap *make_checker_texture(const char *identifier, int variant)
{
    static const br_uint_32 quadrant_colours[6][4] = {
        {0xf800, 0x07e0, 0x001f, 0xffe0}, /* original */
        {0xf800, 0xf800, 0xffe0, 0xffe0}, /* row 1: red/yellow */
        {0x07ff, 0xf81f, 0x07ff, 0xf81f}, /* row 2: cyan/magenta */
        {0x07e0, 0x07e0, 0x001f, 0x001f}, /* row 3: green/blue */
        {0xfd20, 0xfd20, 0xffff, 0xffff}, /* row 4: orange/white */
        {0x4208, 0x8410, 0x4208, 0x8410}  /* road: grey */
    };
    br_pixelmap *map;
    int x, y;

    map = BrPixelmapAllocate(BR_PMT_RGB_565, TEX_SIZE, TEX_SIZE, NULL, 0);
    if(map == NULL)
        return NULL;

    if(variant < 0 || variant > 5)
        variant = 0;
    map->identifier = BrResStrDup(map, identifier);

    for(y = 0; y < TEX_SIZE; ++y) {
        for(x = 0; x < TEX_SIZE; ++x) {
            int quadrant = (x >= TEX_SIZE / 2) + 2 * (y >= TEX_SIZE / 2);
            br_uint_32 colour = quadrant_colours[variant][quadrant];

            /* Uneven grid spacing and a white diagonal make U/V swaps,
             * affine interpolation and triangle seams immediately visible. */
            if(x == 0 || y == 0 || x == TEX_SIZE - 1 || y == TEX_SIZE - 1)
                colour = 0xffff;
            else if((x % 8) == 0 || (y % 8) == 0)
                colour = 0x0000;
            else if(x == y || x + y == TEX_SIZE - 1)
                colour = 0xffff;

            BrPixelmapPixelSet(map, x, y, colour);
        }
    }

    return map;
}

static void set_vertex(br_vertex *vertex,
    br_scalar x, br_scalar y, br_scalar z, br_scalar u, br_scalar v)
{
    BrVector3Set(&vertex->p, x, y, z);
    BrVector2Set(&vertex->map, u, v);
}

static br_model *make_cube(br_material *material)
{
    static const br_scalar positions[6][4][3] = {
        /* front, back, left, right, top, bottom */
        {{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}},
        {{ 1,-1,-1}, {-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}},
        {{-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}},
        {{ 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1}},
        {{-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, {-1, 1,-1}},
        {{-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1}}
    };
    static const br_scalar uv[4][2] = {
        {0, 1}, {1, 1}, {1, 0}, {0, 0}
    };
    br_model *model;
    int face, corner;

    model = BrModelAllocate("perspective_cube", 24, 12);
    if(model == NULL)
        return NULL;

    model->flags |= BR_MODF_DONT_WELD | BR_MODF_FACES_ONLY;

    for(face = 0; face < 6; ++face) {
        int base = face * 4;
        int triangle = face * 2;

        for(corner = 0; corner < 4; ++corner) {
            set_vertex(&model->vertices[base + corner],
                positions[face][corner][0], positions[face][corner][1],
                positions[face][corner][2], uv[corner][0], uv[corner][1]);
        }

        model->faces[triangle + 0].vertices[0] = base + 0;
        model->faces[triangle + 0].vertices[1] = base + 1;
        model->faces[triangle + 0].vertices[2] = base + 2;
        model->faces[triangle + 0].material = material;

        model->faces[triangle + 1].vertices[0] = base + 0;
        model->faces[triangle + 1].vertices[1] = base + 2;
        model->faces[triangle + 1].vertices[2] = base + 3;
        model->faces[triangle + 1].material = material;
    }

    return model;
}

int main(void)
{
    br_pixelmap *screen = NULL;
    br_pixelmap *colour_buffer = NULL;
    br_pixelmap *depth_buffer = NULL;
    br_pixelmap *texture = NULL;
    br_material *material = NULL;
    br_model *cube_model = NULL;
#ifdef BRENDER_DEPTH_STRESS
    br_pixelmap *far_texture = NULL;
    br_pixelmap *row3_texture = NULL;
    br_pixelmap *row4_texture = NULL;
    br_pixelmap *road_texture = NULL;
    br_material *far_material = NULL;
    br_material *row3_material = NULL;
    br_material *row4_material = NULL;
    br_material *road_material = NULL;
    br_model *far_model = NULL;
    br_model *row3_model = NULL;
    br_model *row4_model = NULL;
    br_model *road_model = NULL;
#endif
    br_actor *world = NULL;
    br_actor *camera = NULL;
    br_actor *cube = NULL;
    br_camera *camera_data;
    struct Window *window = NULL;
    int brender_started = 0;
    int zb_started = 0;
    int minigl_open = 0;
    int cybergraphics_open = 0;
    int context_open = 0;
    unsigned long frames = 0;
    int running = 1;
    int redraw = 1;
    int result = 1;

#ifdef BRENDER_DEPTH_STRESS
    log_file = fopen("brdepth.log", "w");
    test_log("BRender/Carmageddon depth stress test: starting");
#else
    log_file = fopen("brender_perspective_test.log", "w");
    test_log("BRender perspective test: starting");
#endif

    if(!MiniGLOpen()) {
        test_log("ERROR: unable to open minigl.library");
        goto cleanup;
    }
    minigl_open = 1;

    CyberGfxBase = OpenLibrary("cybergraphics.library", 37);
    if(CyberGfxBase == NULL) {
        test_log("ERROR: unable to open cybergraphics.library V37");
        goto cleanup;
    }
    cybergraphics_open = 1;

    mglChooseWindowMode(GL_TRUE);
    mglChooseNumberOfBuffers(2);
    mglChoosePixelDepth(16);
    mglChooseVertexBufferSize(4096);
    if(!mglCreateContext(0, 0, TEST_WIDTH, TEST_HEIGHT)) {
        test_log("ERROR: unable to create MiniGL context");
        goto cleanup;
    }
    context_open = 1;
    mglEnableSync(GL_FALSE);

    window = (struct Window *)mglGetWindowHandle();
    if(window == NULL) {
        test_log("ERROR: MiniGL returned no Intuition window");
        goto cleanup;
    }
    ModifyIDCMP(window, IDCMP_RAWKEY | IDCMP_CLOSEWINDOW |
        IDCMP_REFRESHWINDOW);
#ifdef BRENDER_DEPTH_STRESS
    SetWindowTitles(window,
        "BRender depth: arrows=move  5=-6.8  6=-6.7",
        (CONST_STRPTR)-1);
#else
    SetWindowTitles(window,
        "BRender perspective: 1=Q  2=affine  3=vertex4  4=frustum",
        (CONST_STRPTR)-1);
#endif

    test_log((const char *)glGetString(GL_VENDOR));
    test_log((const char *)glGetString(GL_RENDERER));
    test_log((const char *)glGetString(GL_VERSION));

    BrBegin();
    brender_started = 1;

    if(BrDevBeginVar(&screen, "3dfx_amiga",
        BRT_WIDTH_I32, TEST_WIDTH,
        BRT_HEIGHT_I32, TEST_HEIGHT,
        BR_NULL_TOKEN) != BRE_OK || screen == NULL) {
        test_log("ERROR: unable to start BRender 3dfx_amiga device");
        goto cleanup;
    }

    colour_buffer = BrPixelmapMatch(screen, BR_PMMATCH_OFFSCREEN);
    depth_buffer = BrPixelmapMatch(colour_buffer, BR_PMMATCH_DEPTH_16);
    if(colour_buffer == NULL || depth_buffer == NULL) {
        test_log("ERROR: unable to allocate BRender buffers");
        goto cleanup;
    }
    colour_buffer->origin_x = depth_buffer->origin_x = TEST_WIDTH / 2;
    colour_buffer->origin_y = depth_buffer->origin_y = TEST_HEIGHT / 2;

    BrZbBegin(colour_buffer->type, depth_buffer->type);
    zb_started = 1;

#ifdef BRENDER_DEPTH_STRESS
    texture = make_checker_texture("near_red_yellow", 1);
#else
    texture = make_checker_texture("perspective_grid", 0);
#endif
    if(texture == NULL) {
        test_log("ERROR: unable to create checker texture");
        goto cleanup;
    }
    BrMapAdd(texture);

#ifdef BRENDER_DEPTH_STRESS
    far_texture = make_checker_texture("far_cyan_magenta", 2);
    row3_texture = make_checker_texture("row3_green_blue", 3);
    row4_texture = make_checker_texture("row4_orange_white", 4);
    road_texture = make_checker_texture("road_grey", 5);
    if(far_texture == NULL || row3_texture == NULL ||
        row4_texture == NULL || road_texture == NULL) {
        test_log("ERROR: unable to create diagnostic textures");
        goto cleanup;
    }
    BrMapAdd(far_texture);
    BrMapAdd(row3_texture);
    BrMapAdd(row4_texture);
    BrMapAdd(road_texture);
#endif

    material = BrMaterialAllocate("perspective_material");
    if(material == NULL) {
        test_log("ERROR: unable to create material");
        goto cleanup;
    }
    material->colour_map = texture;
    material->flags &= ~(BR_MATF_LIGHT | BR_MATF_PRELIT | BR_MATF_SMOOTH);
    material->flags |= BR_MATF_PERSPECTIVE | BR_MATF_TWO_SIDED |
        BR_MATF_MAP_INTERPOLATION;
    BrMaterialAdd(material);

#ifdef BRENDER_DEPTH_STRESS
    far_material = BrMaterialAllocate("far_cyan_magenta");
    row3_material = BrMaterialAllocate("row3_green_blue");
    row4_material = BrMaterialAllocate("row4_orange_white");
    road_material = BrMaterialAllocate("road_grey");
    if(far_material == NULL || row3_material == NULL ||
        row4_material == NULL || road_material == NULL) {
        test_log("ERROR: unable to create diagnostic materials");
        goto cleanup;
    }
    far_material->colour_map = far_texture;
    row3_material->colour_map = row3_texture;
    row4_material->colour_map = row4_texture;
    road_material->colour_map = road_texture;
    far_material->flags = material->flags;
    row3_material->flags = material->flags;
    row4_material->flags = material->flags;
    road_material->flags = material->flags;
    BrMaterialAdd(far_material);
    BrMaterialAdd(row3_material);
    BrMaterialAdd(row4_material);
    BrMaterialAdd(road_material);
#endif

    cube_model = make_cube(material);
    if(cube_model == NULL) {
        test_log("ERROR: unable to create cube model");
        goto cleanup;
    }
    BrModelAdd(cube_model);

#ifdef BRENDER_DEPTH_STRESS
    far_model = make_cube(far_material);
    row3_model = make_cube(row3_material);
    row4_model = make_cube(row4_material);
    road_model = make_cube(road_material);
    if(far_model == NULL || row3_model == NULL ||
        row4_model == NULL || road_model == NULL) {
        test_log("ERROR: unable to create diagnostic models");
        goto cleanup;
    }
    BrModelAdd(far_model);
    BrModelAdd(row3_model);
    BrModelAdd(row4_model);
    BrModelAdd(road_model);
#endif

    world = BrActorAllocate(BR_ACTOR_NONE, NULL);
    camera = BrActorAdd(world, BrActorAllocate(BR_ACTOR_CAMERA, NULL));
    camera->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Translate(&camera->t.t.mat, BR_SCALAR(0), BR_SCALAR(0),
#ifdef BRENDER_DEPTH_STRESS
        camera_distance);
#else
        BR_SCALAR(5.5));
#endif
    camera_data = (br_camera *)camera->type_data;
    camera_data->type = BR_CAMERA_PERSPECTIVE_FOV;
    camera_data->field_of_view = BrDegreeToAngle(BR_SCALAR(55));
    camera_data->aspect = BR_DIV(BR_SCALAR(TEST_WIDTH), BR_SCALAR(TEST_HEIGHT));
    camera_data->hither_z = BR_SCALAR(0.1);
    camera_data->yon_z = BR_SCALAR(200.0);

#ifdef BRENDER_DEPTH_STRESS
    if(!make_depth_scene(world,
        cube_model, material, far_model, far_material,
        row3_model, row3_material, row4_model, row4_material,
        road_model, road_material)) {
        test_log("ERROR: unable to create depth stress scene");
        goto cleanup;
    }
#else
    cube = BrActorAdd(world, BrActorAllocate(BR_ACTOR_MODEL, NULL));
    cube->model = cube_model;
    cube->material = material;
    cube->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Identity(&cube->t.t.mat);
    BrMatrix34PreRotateY(&cube->t.t.mat, BrDegreeToAngle(BR_SCALAR(32.0)));
    BrMatrix34PreRotateX(&cube->t.t.mat, BrDegreeToAngle(BR_SCALAR(-19.0)));
#endif

    FXA_SetPerspectiveDiagnosticMode(1);
    test_log("Mode 1: Glide texture Q (sow, tow, oow)");
#ifdef BRENDER_DEPTH_STRESS
    test_log("Depth stress scene: distant geometry must never cover nearer boxes");
    test_log("Arrow keys change camera distance; 5 resets it; 1 is Carmageddon Q");
#else
    test_log("Keys 1/2/3/4 select Q, affine, vertex4, or glFrustum XYZ");
#endif
    test_log("ESC or close gadget exits");
    result = 0;
    while(running) {
        if(redraw) {
            BrPixelmapFill(colour_buffer, 0x0010);
            BrPixelmapFill(depth_buffer, 0xffffffffUL);
            BrZbSceneRender(world, camera, colour_buffer, depth_buffer);
            BrPixelmapDoubleBuffer(screen, colour_buffer);
            ++frames;
            redraw = 0;
        }

        /* The scene is static.  Sleep until the user changes a diagnostic
         * mode instead of rendering the same frame at unlimited speed. */
        WaitPort(window->UserPort);
        running = handle_events(window, camera, &redraw);
    }

cleanup:
    if(log_file != NULL) {
        fprintf(log_file, "Frames rendered: %lu\n", frames);
        fflush(log_file);
    }
    if(zb_started)
        BrZbEnd();
    if(brender_started)
        BrEnd();
    if(context_open)
        mglDeleteContext();
    if(cybergraphics_open) {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
    if(minigl_open)
        MiniGLClose();
#ifdef BRENDER_DEPTH_STRESS
    test_log(result == 0 ? "BRender depth stress test: finished" :
        "BRender depth stress test: failed");
#else
    test_log(result == 0 ? "BRender perspective test: finished" :
        "BRender perspective test: failed");
#endif
    if(log_file != NULL)
        fclose(log_file);
    return result;
}
