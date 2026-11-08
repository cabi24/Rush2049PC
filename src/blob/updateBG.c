/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Historical label only. Scrolls up to four point lists (backPolys[slot],
 * a 20-byte header with an s16 count followed by 20-byte points whose first
 * two s16 are x and y) by the current frame's per-slot velocity
 * gBackRects[gNumWindows - 1].slot[slot].dx/dy * gFrameSeconds * (dimension << 5),
 * then wraps: every point votes -1 / +1 when it is beyond +/- (dimension << 5);
 * a unanimous vote (exactly -4 or +4, i.e. four points) shifts the whole list
 * by one dimension. The resulting x/y are recorded as floats in the frame
 * record (xs[i], ys[i]) and offsetsValid[gNumWindows - 1] is set to 1.
 * No arcade ancestor identified.
 *
 * Retail facts kept as they are: the second y test is `y <= +(height << 5)`
 * (not a negated bound), and the negations are `(-dim) << 5`.
 * Shaping: the two "no shift" stores are the int literal 0 while the
 * initialisers are 0.0f (retail keeps two separate zero registers, f0 and
 * f16). Also MATCH at -O2 with the same source. Prior attempt:
 * (235/254; raw s16 list and byte-offset macro).
 */
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned char u8;
typedef float f32;

typedef struct {
    s16 x;
    s16 y;
    char pad4[0x10];
} Point; /* 0x14 */

typedef struct {
    s16 count;
    char pad2[0x12];
    Point pts[1];
} PointList;

typedef struct {
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
} Motion; /* 0x38 */

typedef struct {
    Motion slot[4];
} MotionFrame; /* 0xE0 */

extern PointList *backPolys[4];
extern Dims *bg_texture;
extern s32 offsetsValid[];
extern s16 gNumWindows;
extern f32 gFrameSeconds;
extern MotionFrame gBackRects[];

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
