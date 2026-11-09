/* ZOID_WindowTo3D.c: the world point under a window position (MAT3 zoidwindow.c, ZOID_WindowTo3D()).
 *
 * The inverse of ZOID_ProjectWindow at a given depth: the HUD and the front end use it to put 3D objects (the
 * arrows, the menu cars) at screen positions. */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes): the
 * projection's numbers. */
typedef struct tWindowInfo {
    void *vp; void *s; char pad0_8[0x1C - 0x8];
    f32 wihvangtan;             /* 0.5 / tan(half the horizontal field of view) */
    f32 wivvangtan;             /* 0.5 / tan(half the vertical one) */
    f32 wwidth;                 /* the size, pixels */
    f32 wheight;
    f32 whcenter;               /* the centre, pixels */
    f32 wvcenter;
    char pad52[72 - 52];
} tWindowInfo;

VIEW(tWindowInfo[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* PC: the HUD's horizontal offset for a wider screen (port/gfx), so a 3D object placed at a HUD position lands
 * where the HUD is drawn. 0 at the original width. */
int pc_gfx_hud_dx(int x);
#define HUD_X(x) ((x) + pc_gfx_hud_dx(x))

/* The world point at depth `zdist` (feet ahead of the camera) under screen position screen[2] (pixels) of window
 * `widx`, seen through camera matrix `mat`, into point[3]. A mirrored track (gMirrorWaves) flips x, an upside-down
 * one (gUpsideDownTrack) x and y, as the draw does. */
void ZOID_WindowTo3D(int widx, s16 *screen, tUVMatrix *mat, f32 zdist, f32 *point)
{
    f32 world[3];
    f32 bvec[3];
    tWindowInfo *win = ZoidWinInfo;

    win += widx;

    bvec[0] = ((HUD_X(screen[0]) - win->whcenter) * zdist) / (win->wihvangtan * win->wwidth);
    bvec[1] = ((win->wvcenter - screen[1]) * zdist) / (win->wivvangtan * win->wheight);
    bvec[2] = zdist;
    if (gMirrorWaves) {
        bvec[0] = -bvec[0];
    }
    if (gUpsideDownTrack) {
        bvec[0] = -bvec[0];
        bvec[1] = -bvec[1];
    }
    WorldVector(bvec, world, (f32 *) mat);
    point[0] = world[0] + mat->pos[0];
    point[1] = world[1] + mat->pos[1];
    point[2] = world[2] + mat->pos[2];
}
