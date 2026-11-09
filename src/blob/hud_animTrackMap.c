/* hud_animTrackMap: the track map, drawn into its texture (MAT3 hud.c's animTrackMap). */
#include "game/game.h"

/* The map's texture's texels (tTexture.texBase: they are its image, in its WAD's IMAG chunk). */
#define TEXELS(tex) ((u8 *)(tex)->texBase)

#define MAP_FIRST_DRAW 0x10   /* animID: the map is yet to be placed and drawn */
#define MAP_ROAD 0xE          /* the map's colours (4-bit texels): the road, */
#define MAP_EDGE 0xD          /* and its border */

/* The track map: hidden with 5 windows or more or when the HUD hides it. The first time (animID's MAP_FIRST_DRAW)
 * it takes the medium texture with 4 windows, places itself at hud_mapPos, and draws the track into its texture
 * (4-bit texels, two a byte): every point of the path (gResPath) as a 5x5 square of road colour with a border colour
 * edge, fitted in the map less 4 pixels a side, mirrored with gMirrorWaves. Sets kMapSize for the dots. Returns 1. */
s32 hud_animTrackMap(tBlit *blt) {
    s32 dx;
    s32 dz;
    s32 w;
    s32 h;
    s32 offx;
    s32 offz;
    s32 i;
    s32 j;
    s32 x;
    s32 y;
    s32 px;
    s32 pz;
    s32 col;
    u8 *row;
    s32 hide;
    s32 t;

    hide = gNumWindows >= 5 || gShowTrackMap == 0;
    if (hide != blt->hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    if (blt->hide) {
        return 1;
    }
    if (!(blt->animID & MAP_FIRST_DRAW)) {
        return 1;
    }
    if (gNumWindows == 4) {
        RenameBlit(blt, "MAP_MD", 0);
    }
    kMapSize = blt->width;
    if (blt->animID & MAP_FIRST_DRAW) {
        blt->animID = blt->animID & 0xF;
        blt->x = hud_mapPos[gNumWindows - 1].x - kMapSize / 2;
        blt->y = hud_mapPos[gNumWindows - 1].y - kMapSize / 2;
        UpdateBlit(blt);
    }
    dx = gPathMax[0] - gPathMin[0];
    dz = gPathMax[2] - gPathMin[2];
    if (dz < dx) {
        t = kMapSize - 8;
        w = t;
        h = dz * t / dx;
    } else {
        t = kMapSize - 8;
        h = t;
        w = dx * t / dz;
    }
    offx = (kMapSize - w - 8) / 2 + 4;
    offz = (kMapSize - h - 8) / 2 + 4;
    for (pz = 0; pz < kMapSize; pz++) {
        for (px = 0; px < kMapSize / 2; px++) {
            TEXELS(blt->texture)[pz * kMapSize / 2 + px] = 0;
        }
    }
    for (i = 0; i < gResPath.numPts; i++) {
        if (i >= gResPath.numPts) {
            for (j = 0; j < gResPath.numAlt; j++) {
                if (&gResPathPtList[i] >= gResPath.alt[j].pts && &gResPathPtList[i] < gResPath.alt[j].pts + gResPath.alt[j].numPts) {
                    break;
                }
            }
            if (gResPath.alt[j].resurrect == 0) {
                i += gResPath.alt[j].numPts - 1;
                continue;
            }
        }
        px = (gResPath.pts[i][0] - gPathMin[0]) * w / dx + offx;
        pz = (gResPath.pts[i][2] - gPathMin[2]) * h / dz + offz;
        for (y = pz - 2; y <= pz + 2; y++) {
            for (x = px - 2; x <= px + 2; x++) {
                if (x >= 0 && x < kMapSize && y >= 0 && y < kMapSize) {
                    col = gMirrorWaves == 0 ? kMapSize - x - 1 : x;
                    row = TEXELS(blt->texture) + (kMapSize - y - 1) * kMapSize / 2 + col / 2;
                    if (x <= px - 2 || x >= px + 2 || y <= pz - 2 || y >= pz + 2) {
                        if (col & 1) {
                            if ((*row & 0xF) != MAP_ROAD) {
                                *row = (*row & 0xF0) | MAP_EDGE;
                            }
                        } else {
                            if ((*row & 0xF0) != MAP_ROAD << 4) {
                                *row = (*row & 0xF) | MAP_EDGE << 4;
                            }
                        }
                    } else if (col & 1) {
                        *row = (*row & 0xF0) | MAP_ROAD;
                    } else {
                        *row = (*row & 0xF) | MAP_ROAD << 4;
                    }
                }
            }
        }
    }
    return 1;
}
