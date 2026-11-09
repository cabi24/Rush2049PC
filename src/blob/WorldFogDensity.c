#include "game/game.h"

typedef struct Region {
    s16 x;
    s16 z;
    s16 radius;
    s16 near_clip;
    s16 far_clip;
    s16 fog;
} Region; /* 0xC */

typedef struct View {
    void *first; void *bounds; char pad0_8[0x3C - 0x8];
    f32 fog;        /* 0x3C */
    u16 near_clip;  /* 0x40 */
    u16 far_clip;   /* 0x42 */
    char pad44[4];
} View; /* 0x48 */
/* PC: a window is 80 bytes, its first two words are pointers (docs/port/POINTERS.md 10). */
#define VIEW_SIZE (s32)sizeof(View)

VIEW(Region *[], trackFog);
#define trackFog trackFog_view
VIEW(View[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

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
