/* ZOID_ProjectWindow/candidate.c: where a world point appears in a window (MAT3 zoidwindow.c, ZOID_ProjectWindow()).
 *
 * The front end's screens use it to place their labels by the 3D buttons, the HUD to put markers over cars. */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes): the
 * projection's numbers. */
typedef struct tWindowInfo {
    void *vp; void *s; u8 pad08[0x1C - 0x8];
    f32 wihvangtan;             /* 0.5 / tan(half the horizontal field of view) */
    f32 wivvangtan;             /* 0.5 / tan(half the vertical one) */
    f32 wwidth;                 /* the size, pixels */
    f32 wheight;
    f32 whcenter;               /* the centre, pixels */
    f32 wvcenter;
    u8 pad34[0x14];
} tWindowInfo;

/* Project world point point[3] through camera matrix `mat` (its rotation and position) into window `widx`: the
 * screen position into screen[2], pixels, clamped to the s16 range; the point in the camera's frame (x right, y up,
 * z ahead, feet) into body[3] if given. A point nearer than 2.5 feet ahead (MAT3: 0.01), behind the camera
 * included, is projected as if 2.5 feet ahead. */
void ZOID_ProjectWindow(s32 widx, f32 *point, tUVMatrix *mat, f32 *body, s16 *screen) {
    f32 rel[3];
    f32 scr[2];
    f32 invz;
    f32 bdy[3];
    tWindowInfo *win;

    rel[0] = point[0] - mat->pos[0];
    rel[1] = point[1] - mat->pos[1];
    rel[2] = point[2] - mat->pos[2];
    BodyVector(rel, bdy, (f32 *) mat);
    if (bdy[2] < 2.5f) {
        bdy[2] = 2.5f;
    }
    win = ZOID_GetWindow(widx);
    invz = 1.0f / bdy[2];
    scr[0] = bdy[0] * invz * win->wihvangtan * win->wwidth + win->whcenter;
    scr[1] = win->wvcenter - bdy[1] * invz * win->wivvangtan * win->wheight;
    if (scr[0] < -32768.0f) {
        screen[0] = -32768;
    } else if (scr[0] > 32767.0f) {
        screen[0] = 32767;
    } else {
        screen[0] = scr[0];
    }
    if (scr[1] < -32768.0f) {
        screen[1] = -32768;
    } else if (scr[1] > 32767.0f) {
        screen[1] = 32767;
    } else {
        screen[1] = scr[1];
    }
    if (body != 0) {
        body[0] = bdy[0];
        body[1] = bdy[1];
        body[2] = bdy[2];
    }
}
