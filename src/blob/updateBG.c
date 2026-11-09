/* updateBG: the front end's scrolling background (MAT3 front.c updateBG): four textured quads (backPolys) whose
 * texture coordinates drift each frame, wrapping by a whole texture so the numbers stay small. */
#include "game/game.h"
#define MotionFrame MotionFrameView   /* this file's own MotionFrame: include/game's MotionFrame is another type */

/* A vertex of a background quad: its texture coordinates (s, t, in 1/32 texels). */
typedef struct {
    s16 x;
    s16 y;
    char pad4[0x10];
} Point; /* 0x14 */

typedef struct PointList {
    s16 count;
    char pad2[0x12];
    Point pts[1];
} PointList;

typedef struct Dims {
    char pad0[0x10];
    u16 width;
    u16 height;
} Dims;

typedef struct {
    /* PC: the rectangle createBG reads (min/max x, y): the PC loader fills initialized data by its declarations, and
     * bytes here would stay big-endian (docs/port/ENDIAN.md). */
    f32 bounds[4];
    f32 dx;
    f32 dy;
    f32 xs[4];
    f32 ys[4];
} Motion; /* 0x38: one quad's drift (dx, dy textures a second) and its corners' coordinates */

/* gBackRects[windows - 1]: the four quads' motion for each split-screen layout. */
typedef struct {
    Motion slot[4];
} MotionFrame; /* 0xE0 */

VIEW(PointList *[4], backPolys);
#define backPolys backPolys_view
VIEW(Dims *, bg_texture);
#define bg_texture bg_texture_view

VIEW(MotionFrame[], gBackRects);
#define gBackRects gBackRects_view

/* Each frame, for each of the four quads that is up: move its corners' texture coordinates by its drift times the
 * frame's length; when all four corners are past one texture width (height) the same way, shift them all back by
 * one; keep the result in gBackRects (offsetsValid says it is there, for createBG to start from next time). */
void updateBG(void) {
    s32 slot;
    s32 i;
    PointList *list;
    f32 shift[2];

    for (slot = 0; slot < 4; slot++) {
        list = backPolys[slot];
        if (list != 0) {
            shift[0] = 0.0f;
            shift[1] = 0.0f;
            for (i = 0; i < list->count; i++) {
                list->pts[i].x += gBackRects[gNumWindows - 1].slot[slot].dx * gFrameSeconds * (bg_texture->width << 5);
                list->pts[i].y += gBackRects[gNumWindows - 1].slot[slot].dy * gFrameSeconds * (bg_texture->height << 5);
            }
            for (i = 0; i < list->count; i++) {
                if (list->pts[i].x >= bg_texture->width << 5) {
                    shift[0] -= 1.0f;
                }
                if (list->pts[i].x <= -bg_texture->width << 5) {
                    shift[0] += 1.0f;
                }
                if (list->pts[i].y >= bg_texture->height << 5) {
                    shift[1] -= 1.0f;
                }
                /* Original game: x is tested against -(width << 5) above, but y against +(height << 5) here
                 * (MAT3 front.c tests tc[1] <= -1.0f): nearly every vertex votes +1, so the y votes rarely add up
                 * to the -4 or +4 that wraps the background vertically. Kept as retail. */
                if (list->pts[i].y <= bg_texture->height << 5) {
                    shift[1] += 1.0f;
                }
            }
            if (shift[0] == -4.0f) {
                shift[0] = -bg_texture->width << 5;
            } else if (shift[0] == 4.0f) {
                shift[0] = bg_texture->width << 5;
            } else {
                shift[0] = 0;
            }
            if (shift[1] == -4.0f) {
                shift[1] = -bg_texture->height << 5;
            } else if (shift[1] == 4.0f) {
                shift[1] = bg_texture->height << 5;
            } else {
                shift[1] = 0;
            }
            for (i = 0; i < list->count; i++) {
                list->pts[i].x += shift[0];
                list->pts[i].y += shift[1];
                gBackRects[gNumWindows - 1].slot[slot].xs[i] = list->pts[i].x;
                gBackRects[gNumWindows - 1].slot[slot].ys[i] = list->pts[i].y;
            }
            offsetsValid[gNumWindows - 1] = 1;
        }
    }
}
