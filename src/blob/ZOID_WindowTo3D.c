#include "game/game.h"

typedef struct View {
    void *first; void *bounds; char pad0_8[0x1C - 0x8];
    f32 scale_x;   /* 28 */
    f32 scale_y;   /* 32 */
    f32 proj_x;    /* 36 */
    f32 proj_y;    /* 40 */
    f32 center_x;  /* 44 */
    f32 center_y;  /* 48 */
    char pad52[72 - 52];
} View;

typedef struct Camera {
    f32 mat[9];
    f32 pos[3];
} Camera;

VIEW(View[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

int pc_gfx_hud_dx(int x);
#define HUD_X(x) ((x) + pc_gfx_hud_dx(x))

void ZOID_WindowTo3D(int view, s16 *screen, Camera *cam, f32 depth, f32 *out)
{
    f32 world[3];
    f32 in[3];
    View *v = ZoidWinInfo;

    v += view;

    in[0] = ((HUD_X(screen[0]) - v->center_x) * depth) / (v->scale_x * v->proj_x);
    in[1] = ((v->center_y - screen[1]) * depth) / (v->scale_y * v->proj_y);
    in[2] = depth;
    if (gMirrorWaves) {
        in[0] = -in[0];
    }
    if (gUpsideDownTrack) {
        in[0] = -in[0];
        in[1] = -in[1];
    }
    WorldVector(in, world, cam);
    out[0] = world[0] + cam->pos[0];
    out[1] = world[1] + cam->pos[1];
    out[2] = world[2] + cam->pos[2];
}
