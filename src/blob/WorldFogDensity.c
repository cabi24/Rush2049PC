/* WorldFogDensity.c: a window's fog for where its camera is (MAT3 world.c, WorldFogDensity()).
 *
 * With the track select's fog option on (gFogIntensity 1-3), each window's fog comes from tables by the player
 * count and the intensity; a track may also have fog regions (trackFog: circles on the ground where the fog is
 * thicker), and near one the window's fog blends toward the region's. The N64 regions and tables differ from
 * MAT3's (integer regions, the fog's far end fixed at 1000). */
#include "game/game.h"

/* A fog region: a circle of `radius` feet about (x, z) in which the fog moves toward these values at its centre.
 * A radius of 0 ends a track's list. */
typedef struct FogRegion {
    s16 x;
    s16 z;
    s16 radius;
    s16 fogmin;         /* where the fog starts (the window's fogmin) */
    s16 fogmax;         /* where it is full (fogmax) */
    s16 backclip;       /* the view's far end (backclip) */
} FogRegion; /* 0xC */

/* A window (MAT3 tWindowInfo), the fields used here. */
typedef struct tWindowInfo {
    void *first; void *bounds; char pad0_8[0x3C - 0x8];
    f32 backclip;       /* 0x3C the far clip distance, feet */
    u16 fogmin;         /* 0x40 the fog's start, in the RSP's fog depth units (0-1000) */
    u16 fogmax;         /* 0x42 the fog's full */
    char pad44[4];
} tWindowInfo; /* 0x48 */
/* PC: a window is 80 bytes, its first two words are pointers (docs/port/POINTERS.md 10). */
#define WININFO_SIZE (s32)sizeof(tWindowInfo)

f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

/* Set window winidx's fog for a camera at pos, when the fog option is on: the tables' values (fogMin,
 * fogDistance and fog_viewable_distance by the player count and the intensity; the fog full at 1000), then, inside
 * the nearest of the track's fog regions, each value moved toward the region's by how near the centre the camera
 * is (ratio: the distance over the radius, or over the gap between the window's and the region's back clip when
 * the radius is the larger; beyond that gap nothing changes). The fog's start stays at least 4 short of its full.
 * gViewableDistance (how far the scene walk draws) follows the back clip. */
void WorldFogDensity(s32 winidx, f32 *pos) {
    tWindowInfo *win;
    FogRegion *region;
    FogRegion *nearest;
    s16 dist;
    s16 closest;
    f32 dx;
    f32 dz;
    f32 ratio;

    if (gFogIntensity) {
        win = (tWindowInfo *) ((intptr_t) ZoidWinInfo + winidx * WININFO_SIZE);
        win->fogmin = fogMin[gNumPlayers][gFogIntensity];
        win->fogmax = 1000;
        win->backclip = fogDistance[gNumPlayers][gFogIntensity];
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
                if (nearest->radius <= win->backclip) {
                    ratio = (f32) closest / nearest->radius;
                } else {
                    if (closest < win->backclip - nearest->backclip) {
                        ratio = closest / (win->backclip - nearest->backclip);
                    } else {
                        return;
                    }
                }
                win->fogmin = nearest->fogmin + ratio * (win->fogmin - nearest->fogmin);
                win->fogmax = nearest->fogmax + ratio * (win->fogmax - nearest->fogmax);
                if (win->fogmin > win->fogmax - 4) {
                    win->fogmin = win->fogmax - 4;
                }
                win->backclip = nearest->backclip + ratio * (win->backclip - nearest->backclip);
                gViewableDistance = nearest->backclip + ratio * (gViewableDistance - nearest->backclip);
            }
        }
    }
}
