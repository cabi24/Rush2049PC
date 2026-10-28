/* port/assets/tests/test_windows.c: the ZOID windows (D_8017A510, 80 bytes a window on the PC) and the frame records
 * seen from D_80156C5C through the game's own code (docs/port/POINTERS.md 10): ZOID_InitializeWindow (arb_rate_set)
 * with the projection set-up (exhaust_smoke_effect), the fog set-up and read-back (func_800A7480, func_800A7428), the
 * m2c accessors (func_800A5518, func_800A6078), the fog regions (func_800AB18C, which strides by hand), and a
 * world-to-screen projection (brake_light_update, through func_800A6078) undone by the unprojection
 * (car_lights_render): each reads the window through its own view, so a view off the host layout breaks the round
 * trip. Windows 1 to 3 are used, so a 72-byte stride lands in the wrong one. Then the frame-buffer clear
 * (func_800F7448) of frame 1.
 * game: src/blob/groups/window_init/candidate.c src/blob/exhaust_smoke_effect.c src/blob/groups/set_fog/candidate.c
 * game: src/blob/func_800A5518.c src/blob/func_800A6078.c src/blob/func_800A7428.c src/blob/func_800AB18C.c
 * game: src/blob/groups/project_window/candidate.c src/blob/car_lights_render.c src/blob/func_800F7448.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "support.h"

/* set_fog's Descriptor72, the layout every PC view of a window has. */
typedef struct {
    void *first, *bounds;
    int32_t orthographic;
    float horizontal, vertical, tan_horizontal, tan_vertical, inverse_horizontal, inverse_vertical;
    float width, height, near_plane, far_plane, aspect, zoom, fog;
    int16_t range_start;
    uint16_t range_end;
    uint8_t red, green, blue, alpha;
} Window;
_Static_assert(sizeof(Window) == 80 && offsetof(Window, range_start) == 0x48, "the window's host layout");

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

Window D_8017A510[4];
/* The frame records (Segs, 2 x 208 bytes on the PC: the native OSScTask, then the display list, the colour
 * buffer at +0xC8); D_80156C5C is frame 0's colour buffer, as gen_data places it. */
uint8_t D_80156BE0[2 * 208] __attribute__((aligned(16)));
ALIAS(D_80156C5C, D_80156BE0, 0xC8);
struct { int16_t scale[4], translate[4]; } D_8011EA30;
float D_80151AA0;
int32_t D_8002AFC0 = 320, D_8002AFC4 = 240;
int8_t D_8013F1D8, D_8014978C, D_80140A04, D_80151AD8;
int16_t D_8014A108;
int16_t D_8011E810[4][4], D_8011E7F0[4][4];
float D_8011E7A8[4][4];
typedef struct { int16_t x, z, radius, near_clip, far_clip, fog; } Region;
Region *D_8011E76C[4];
volatile int8_t D_8015F72D;
typedef struct { uint32_t w0; uintptr_t w1; } Gfx;
Gfx *D_801497C8;

static uint16_t g_fill;
void func_800A5560(uint16_t color) { g_fill = color; }
float func_800A557C(float x) { return tanf(x); }
float func_8008C720(float x) { return atanf(x); }
static void *g_v2p;
uint32_t osVirtualToPhysical(void *p) { g_v2p = p; return (uint32_t)(uintptr_t)p & 0x1FFFFFFF; }
int pc_gfx_hud_dx(int x) { (void)x; return 0; }   /* port/gfx's race HUD at the screen's edges: no move here */
/* View-space rotation (row vector times matrix) and its inverse; the camera matrices here are rotations. */
void func_800A61B0(float *v, float *out, float *m)
{
    for (int i = 0; i < 3; i++)
        out[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
}
void func_8009E820(float *in, float *out, float *m)
{
    for (int j = 0; j < 3; j++)
        out[j] = in[0] * m[j] + in[1] * m[3 + j] + in[2] * m[6 + j];
}

Window *arb_rate_set(int index, void *first, void *bounds, float horizontal, float vertical, float width, float height,
                     float near_plane, float far_plane);
Window *func_800A7480(int16_t range_start, int16_t range_end, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha,
                      uint32_t index);
void func_800A5518(int32_t index, void *first, void *bounds);
void *func_800A6078(int32_t index);
void func_800A7428(uint16_t *a, uint16_t *b, uint8_t *c, uint8_t *d, uint8_t *e, uint8_t *f, int32_t index);
void func_800AB18C(int32_t index, float *pos);
void brake_light_update(int32_t index, float *pos, float *view, float *vout, int16_t *sout);
void car_lights_render(int view, int16_t *screen, float *cam, float depth, float *out);
void func_800F7448(uint16_t color);

static int near(float a, float b) { return fabsf(a - b) <= 1e-3f * (1.0f + fabsf(b)); }

static void test_init(void)
{
    static int a, b;
    Window *w;
    uint16_t lo, hi;
    uint8_t r, g, bl, al;

    memset(D_8017A510, 0xA5, sizeof D_8017A510);
    w = arb_rate_set(2, &a, &b, 90.0f, 0.0f, 160.0f, 120.0f, 80.0f, 60.0f);
    CHECK(w == &D_8017A510[2] && w->first == &a && w->bounds == &b);
    CHECK(w->zoom == 2.5f && w->fog == 2000.0f && w->range_start == 1000 && w->range_end == 999);
    CHECK(w->red == 0 && w->green == 0 && w->blue == 0 && w->alpha == 255 && g_fill == 1);
    CHECK(w->width == 160.0f && w->height == 120.0f && w->near_plane == 80.0f && w->far_plane == 60.0f);
    CHECK(w->orthographic == 0 && near(w->aspect, 160.0f / 120.0f) && near(w->horizontal, 1.5707964f));
    CHECK(near(w->tan_horizontal, 1.0f) && near(w->inverse_horizontal, 0.5f));
    CHECK(near(w->tan_vertical, 0.75f) && near(w->inverse_vertical, 0.5f / 0.75f));
    CHECK(D_8017A510[1].range_start == (int16_t)0xA5A5 && D_8017A510[3].first == (void *)(uintptr_t)0xA5A5A5A5A5A5A5A5u);
    CHECK(D_8011EA30.scale[0] == 320 && D_8011EA30.translate[1] == 120);

    /* the fog range and colour through two views */
    w = func_800A7480(-5, 777, 10, 20, 30, 40, 3);
    CHECK(w == &D_8017A510[3] && D_8017A510[3].range_start == -5 && D_8017A510[3].range_end == 777);
    func_800A7428(&lo, &hi, &r, &g, &bl, &al, 3);
    CHECK(lo == 0xFFFB && hi == 777 && r == 10 && g == 20 && bl == 30 && al == 40);
    func_800A7428(&lo, &hi, &r, &g, &bl, &al, 2);
    CHECK(lo == 1000 && hi == 999 && al == 255);

    /* the m2c accessors */
    func_800A5518(1, &b, &a);
    CHECK(D_8017A510[1].first == &b && D_8017A510[1].bounds == &a && D_8017A510[2].first == &a);
    CHECK(func_800A6078(3) == &D_8017A510[3]);
}

/* The fog regions: window 2 gets the track's defaults, blended towards the nearest region's. */
static void test_fog_regions(void)
{
    static Region regions[2] = {{0, 0, 100, 10, 500, 40}, {0, 0, 0, 0, 0, 0}};
    float pos[3] = {50.0f, 0.0f, 0.0f};

    D_8013F1D8 = 1, D_8014A108 = 2, D_8014978C = 0;
    D_8011E810[2][1] = 30, D_8011E7F0[2][1] = 200, D_8011E7A8[2][1] = 1500.0f;
    D_8011E76C[0] = regions;
    D_8017A510[1].fog = 7.0f, D_8017A510[3].fog = 9.0f;
    func_800AB18C(2, pos);
    /* the defaults (near 30, far 1000, fog 200, global 1500) halfway to the region's (10, 500, 40) */
    CHECK(D_8017A510[2].range_start == 20 && D_8017A510[2].range_end == 750 && D_8017A510[2].fog == 120.0f);
    CHECK(D_80151AA0 == 770.0f);
    CHECK(D_8017A510[1].fog == 7.0f && D_8017A510[3].fog == 9.0f && D_8017A510[3].range_end == 777);
    D_8013F1D8 = 0;
}

/* Project a point into window 2 and back. */
static void test_round_trip(void)
{
    float cam[12] = {0.6f, 0.0f, -0.8f, 0.0f, 1.0f, 0.0f, 0.8f, 0.0f, 0.6f, 10.0f, 2.0f, -3.0f};
    float pos[3] = {14.0f, 5.0f, 20.0f}, vout[3], back[3];
    int16_t s[2];

    brake_light_update(2, pos, cam, vout, s);
    CHECK(vout[2] > 2.5f && s[0] != 0 && s[1] != 0);
    car_lights_render(2, s, cam, vout[2], back);
    CHECK(fabsf(back[0] - pos[0]) < 0.5f && fabsf(back[1] - pos[1]) < 0.5f && fabsf(back[2] - pos[2]) < 0.5f);
}

/* Clear frame 1's colour buffer: its address is read 208 bytes after frame 0's. */
static void test_frame_clear(void)
{
    static Gfx dl[8];
    void *buf0 = (void *)(uintptr_t)0x80300000u, *buf1 = (void *)(uintptr_t)0x80340000u;

    memcpy(D_80156BE0 + 0xC8, &buf0, sizeof buf0);
    memcpy(D_80156BE0 + 208 + 0xC8, &buf1, sizeof buf1);
    D_801497C8 = dl;
    D_8015F72D = 1;
    func_800F7448(0x1234);
    CHECK(D_801497C8 == dl + 5 && g_v2p == buf1 && dl[1].w1 == 0x00340000u && dl[2].w1 == 0x12341234u);
}

int main(void)
{
    test_init();
    test_fog_regions();
    test_round_trip();
    test_frame_clear();
    return TEST_DONE();
}
