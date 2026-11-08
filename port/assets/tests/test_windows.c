/* port/assets/tests/test_windows.c: the ZOID windows (ZoidWinInfo, 80 bytes a window on the PC) and the frame records
 * seen from frame_color_buffers through the game's own code (docs/port/POINTERS.md 10): ZOID_InitializeWindow
 * with the projection set-up (ZOID_SetWindow), the fog set-up and read-back (ZOID_SetFog, ZOID_GetFog), the
 * m2c accessors (ZOID_SetViewport, ZOID_GetWindow), the fog regions (WorldFogDensity, which strides by hand), and a
 * world-to-screen projection (ZOID_ProjectWindow, through ZOID_GetWindow) undone by the unprojection
 * (ZOID_WindowTo3D): each reads the window through its own view, so a view off the host layout breaks the round
 * trip. Windows 1 to 3 are used, so a 72-byte stride lands in the wrong one. Then the frame-buffer clear
 * (ClearScreen) of frame 1.
 * game: src/blob/groups/ZOID_InitializeWindow/candidate.c src/blob/ZOID_SetWindow.c src/blob/groups/ZOID_SetFog/candidate.c
 * game: src/blob/ZOID_SetViewport.c src/blob/ZOID_GetWindow.c src/blob/ZOID_GetFog.c src/blob/WorldFogDensity.c
 * game: src/blob/groups/ZOID_ProjectWindow/candidate.c src/blob/ZOID_WindowTo3D.c src/blob/ClearScreen.c
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

Window ZoidWinInfo[4];
/* The frame records (Segs, 2 x 208 bytes on the PC: the native OSScTask, then the display list, the colour
 * buffer at +0xC8); frame_color_buffers is frame 0's colour buffer, as gen_data places it. */
uint8_t frame_records[2 * 208] __attribute__((aligned(16)));
ALIAS(frame_color_buffers, frame_records, 0xC8);
struct { int16_t scale[4], translate[4]; } vp0;
float gViewableDistance;
int32_t screen_width = 320, screen_height = 240;
int8_t gFogIntensity, gTrackNum, gMirrorWaves, gUpsideDownTrack;
int16_t gNumPlayers;
int16_t fogMin[4][4], fogDistance[4][4];
float fog_viewable_distance[4][4];
typedef struct { int16_t x, z, radius, near_clip, far_clip, fog; } Region;
Region *trackFog[4];
volatile int8_t frame_record_index;
typedef struct { uint32_t w0; uintptr_t w1; } Gfx;
Gfx *frame_dl;

static uint16_t g_fill;
void ZOID_SetBGColor(uint16_t color) { g_fill = color; }
float tan_cody_waite(float x) { return tanf(x); }
float fatan(float x) { return atanf(x); }
static void *g_v2p;
uint32_t osVirtualToPhysical(void *p) { g_v2p = p; return N64_TO_PHYS((uint32_t)(uintptr_t)p); }
int pc_gfx_hud_dx(int x) { (void)x; return 0; }   /* port/gfx's race HUD at the screen's edges: no move here */
/* View-space rotation (row vector times matrix) and its inverse; the camera matrices here are rotations. */
void BodyVector(float *v, float *out, float *m)
{
    for (int i = 0; i < 3; i++)
        out[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
}
void WorldVector(float *in, float *out, float *m)
{
    for (int j = 0; j < 3; j++)
        out[j] = in[0] * m[j] + in[1] * m[3 + j] + in[2] * m[6 + j];
}

Window *ZOID_InitializeWindow(int index, void *first, void *bounds, float horizontal, float vertical, float width, float height,
                     float near_plane, float far_plane);
Window *ZOID_SetFog(int16_t range_start, int16_t range_end, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha,
                      uint32_t index);
void ZOID_SetViewport(int32_t index, void *first, void *bounds);
void *ZOID_GetWindow(int32_t index);
void ZOID_GetFog(uint16_t *a, uint16_t *b, uint8_t *c, uint8_t *d, uint8_t *e, uint8_t *f, int32_t index);
void WorldFogDensity(int32_t index, float *pos);
void ZOID_ProjectWindow(int32_t index, float *pos, float *view, float *vout, int16_t *sout);
void ZOID_WindowTo3D(int view, int16_t *screen, float *cam, float depth, float *out);
void ClearScreen(uint16_t color);

static int near(float a, float b) { return fabsf(a - b) <= 1e-3f * (1.0f + fabsf(b)); }

static void test_init(void)
{
    static int a, b;
    Window *w;
    uint16_t lo, hi;
    uint8_t r, g, bl, al;

    memset(ZoidWinInfo, 0xA5, sizeof ZoidWinInfo);
    w = ZOID_InitializeWindow(2, &a, &b, 90.0f, 0.0f, 160.0f, 120.0f, 80.0f, 60.0f);
    CHECK(w == &ZoidWinInfo[2] && w->first == &a && w->bounds == &b);
    CHECK(w->zoom == 2.5f && w->fog == 2000.0f && w->range_start == 1000 && w->range_end == 999);
    CHECK(w->red == 0 && w->green == 0 && w->blue == 0 && w->alpha == 255 && g_fill == 1);
    CHECK(w->width == 160.0f && w->height == 120.0f && w->near_plane == 80.0f && w->far_plane == 60.0f);
    CHECK(w->orthographic == 0 && near(w->aspect, 160.0f / 120.0f) && near(w->horizontal, 1.5707964f));
    CHECK(near(w->tan_horizontal, 1.0f) && near(w->inverse_horizontal, 0.5f));
    CHECK(near(w->tan_vertical, 0.75f) && near(w->inverse_vertical, 0.5f / 0.75f));
    CHECK(ZoidWinInfo[1].range_start == (int16_t)0xA5A5 && ZoidWinInfo[3].first == (void *)(uintptr_t)0xA5A5A5A5A5A5A5A5u);
    CHECK(vp0.scale[0] == 320 && vp0.translate[1] == 120);

    /* the fog range and colour through two views */
    w = ZOID_SetFog(-5, 777, 10, 20, 30, 40, 3);
    CHECK(w == &ZoidWinInfo[3] && ZoidWinInfo[3].range_start == -5 && ZoidWinInfo[3].range_end == 777);
    ZOID_GetFog(&lo, &hi, &r, &g, &bl, &al, 3);
    CHECK(lo == 0xFFFB && hi == 777 && r == 10 && g == 20 && bl == 30 && al == 40);
    ZOID_GetFog(&lo, &hi, &r, &g, &bl, &al, 2);
    CHECK(lo == 1000 && hi == 999 && al == 255);

    /* the m2c accessors */
    ZOID_SetViewport(1, &b, &a);
    CHECK(ZoidWinInfo[1].first == &b && ZoidWinInfo[1].bounds == &a && ZoidWinInfo[2].first == &a);
    CHECK(ZOID_GetWindow(3) == &ZoidWinInfo[3]);
}

/* The fog regions: window 2 gets the track's defaults, blended towards the nearest region's. */
static void test_fog_regions(void)
{
    static Region regions[2] = {{0, 0, 100, 10, 500, 40}, {0, 0, 0, 0, 0, 0}};
    float pos[3] = {50.0f, 0.0f, 0.0f};

    gFogIntensity = 1, gNumPlayers = 2, gTrackNum = 0;
    fogMin[2][1] = 30, fogDistance[2][1] = 200, fog_viewable_distance[2][1] = 1500.0f;
    trackFog[0] = regions;
    ZoidWinInfo[1].fog = 7.0f, ZoidWinInfo[3].fog = 9.0f;
    WorldFogDensity(2, pos);
    /* the defaults (near 30, far 1000, fog 200, global 1500) halfway to the region's (10, 500, 40) */
    CHECK(ZoidWinInfo[2].range_start == 20 && ZoidWinInfo[2].range_end == 750 && ZoidWinInfo[2].fog == 120.0f);
    CHECK(gViewableDistance == 770.0f);
    CHECK(ZoidWinInfo[1].fog == 7.0f && ZoidWinInfo[3].fog == 9.0f && ZoidWinInfo[3].range_end == 777);
    gFogIntensity = 0;
}

/* Project a point into window 2 and back. */
static void test_round_trip(void)
{
    float cam[12] = {0.6f, 0.0f, -0.8f, 0.0f, 1.0f, 0.0f, 0.8f, 0.0f, 0.6f, 10.0f, 2.0f, -3.0f};
    float pos[3] = {14.0f, 5.0f, 20.0f}, vout[3], back[3];
    int16_t s[2];

    ZOID_ProjectWindow(2, pos, cam, vout, s);
    CHECK(vout[2] > 2.5f && s[0] != 0 && s[1] != 0);
    ZOID_WindowTo3D(2, s, cam, vout[2], back);
    CHECK(fabsf(back[0] - pos[0]) < 0.5f && fabsf(back[1] - pos[1]) < 0.5f && fabsf(back[2] - pos[2]) < 0.5f);
}

/* Clear frame 1's colour buffer: its address is read 208 bytes after frame 0's. */
static void test_frame_clear(void)
{
    static Gfx dl[8];
    void *buf0 = (void *)(uintptr_t)TEST_SCRATCH, *buf1 = (void *)(uintptr_t)(TEST_SCRATCH + 0x40000u);

    memcpy(frame_records + 0xC8, &buf0, sizeof buf0);
    memcpy(frame_records + 208 + 0xC8, &buf1, sizeof buf1);
    frame_dl = dl;
    frame_record_index = 1;
    ClearScreen(0x1234);
    CHECK(frame_dl == dl + 5 && g_v2p == buf1 && dl[1].w1 == 0x00340000u && dl[2].w1 == 0x12341234u);
}

int main(void)
{
    test_init();
    test_fog_regions();
    test_round_trip();
    test_frame_clear();
    return TEST_DONE();
}
