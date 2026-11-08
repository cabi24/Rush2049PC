/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Historical label only. Per-view fog/clip setup: when a weather/fog mode
 * gFogIntensity is active, view `index` (72-byte records at ZoidWinInfo) gets the
 * per-track defaults (near clip fogMin, far clip 1000, fog distance
 * fogDistance, global gViewableDistance from fog_viewable_distance, all [track gNumPlayers][mode]).
 * Then the nearest fog region of the current track (trackFog[gTrackNum],
 * 12-byte records {x, z, radius, near, far, fog}, terminated by radius 0)
 * that contains the camera position blends near/far/fog towards the region's
 * values by distance / radius. No arcade ancestor identified.
 *
 * Shaping quirks:
 *  - the view pointer is built through an integer cast
 *    ((s32) ZoidWinInfo + index * 72). Retail reloads gFogIntensity and gNumPlayers
 *    after every store to the view, which a plain &ZoidWinInfo[index] does not
 *    do (uopt then knows the stores cannot alias those globals; 228 words
 *    differ). The original was probably a pointer whose base uopt could not
 *    see; the exact spelling is not known.
 *  - typed struct fields, not byte-offset macros: with *(T *)((u8 *)p + off)
 *    accesses cfe orders the lerp operands differently (36 words, FP temps);
 *  - `nearest = region;` before `closest = dist;`.
 * -O3 only (at -O2 the position argument is homed: 99/231 words differ).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * (187/231 there because it was built as an internal group member; alone
 * the same source is 38 words off).
 */
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    s16 x;
    s16 z;
    s16 radius;
    s16 near_clip;
    s16 far_clip;
    s16 fog;
} Region; /* 0xC */

typedef struct {
    void *first; void *bounds; char pad0_8[0x3C - 0x8];
    f32 fog;        /* 0x3C */
    u16 near_clip;  /* 0x40 */
    u16 far_clip;   /* 0x42 */
    char pad44[4];
} View; /* 0x48 */
/* PC: a window is 80 bytes, its first two words are pointers (docs/port/POINTERS.md 10). */
#define VIEW_SIZE (s32)sizeof(View)

extern s8 gFogIntensity;
extern s8 gTrackNum;
extern s16 gNumPlayers;
extern s16 fogMin[][4];
extern s16 fogDistance[][4];
extern f32 fog_viewable_distance[][4];
extern f32 gViewableDistance;
extern Region *trackFog[];
extern View ZoidWinInfo[];

f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

void WorldFogDensity(s32 index, f32 *pos) {
    View *view;
    Region *region;
    Region *nearest;
    s16 dist;
    s16 closest;
    f32 dx;
    f32 dz;
    f32 ratio;

    if (gFogIntensity) {
        view = (View *) ((intptr_t) ZoidWinInfo + index * VIEW_SIZE);
        view->near_clip = fogMin[gNumPlayers][gFogIntensity];
        view->far_clip = 1000;
        view->fog = fogDistance[gNumPlayers][gFogIntensity];
        gViewableDistance = fog_viewable_distance[gNumPlayers][gFogIntensity];
        region = trackFog[gTrackNum];
        if (region) {
            nearest = 0;
            closest = 32767;
            while (region->radius) {
                dx = region->x - pos[0];
                dz = region->z - pos[2];
                dist = sqrtf(dx * dx + dz * dz);
                if (dist < region->radius && dist < closest) {
                    nearest = region;
                    closest = dist;
                }
                region++;
            }
            if (nearest) {
                if (nearest->radius <= view->fog) {
                    ratio = (f32) closest / nearest->radius;
                } else {
                    if (closest < view->fog - nearest->fog) {
                        ratio = closest / (view->fog - nearest->fog);
                    } else {
                        return;
                    }
                }
                view->near_clip = nearest->near_clip + ratio * (view->near_clip - nearest->near_clip);
                view->far_clip = nearest->far_clip + ratio * (view->far_clip - nearest->far_clip);
                if (view->near_clip > view->far_clip - 4) {
                    view->near_clip = view->far_clip - 4;
                }
                view->fog = nearest->fog + ratio * (view->fog - nearest->fog);
                gViewableDistance = nearest->fog + ratio * (gViewableDistance - nearest->fog);
            }
        }
    }
}
