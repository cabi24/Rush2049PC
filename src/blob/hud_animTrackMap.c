#include "game/game.h"

/* PC: TexDef is a WAD's TXHD record, which keeps its N64 layout (port/assets/wad.c): data is a 32-bit window
 * address of the texels. */
#define TXHD_DATA u32 data
#define TEXELS(tex) ((u8 *)PC_N64P((tex)->data))
typedef struct TexDef {
    /* 0x00 */ char name[16];
    /* 0x10 */ u16 width, height;
    /* 0x14 */ u8 pad14[4];
    /* 0x18 */ TXHD_DATA;
} TexDef;

typedef struct Blit {
    /* 0x00 */ char *name;
    /* 0x04 */ void *image;
    /* 0x08 */ TexDef *info;
    /* 0x0C */ s16 texIndex;
    /* 0x0E */ s16 X;
    /* 0x10 */ s16 Y;
    /* 0x12 */ s16 pad12;
    /* 0x14 */ s16 Width;
    /* 0x16 */ s16 Height;
    /* 0x18 */ u8 Alpha;
    /* 0x19 */ u8 pad19;
    /* 0x1A */ s8 Hide;
    /* 0x1C */ s16 Top;
    /* 0x1E */ s16 Bot;
    /* 0x20 */ s16 Left;
    /* 0x22 */ s16 Right;
    /* 0x24 */ s32 pad24;
    /* 0x28 */ intptr_t AnimDTA;
    /* 0x2C */ u32 AnimID;
} Blit;

typedef struct PathPoint {
    s16 pos[3];
} PathPoint;

typedef struct PathSet {
    /* 0x0 */ u8 type;
    /* 0x1 */ u8 pad1[9];
    /* 0xA */ u16 count;
    /* 0xC */ PathPoint *points;
} PathSet; /* 0x10 */

typedef struct PathGraph {
    /* 0x0 */ u16 count;
    /* 0x4 */ PathPoint *points;
    /* 0x8 */ u8 numSets;
    /* 0xC */ PathSet *sets;
} PathGraph;

typedef struct {
    s32 x;
    s32 y;
} Pos;

VIEW(Pos[], hud_mapPos);
#define hud_mapPos hud_mapPos_view
VIEW(s16[3], gPathMax);
#define gPathMax gPathMax_view
VIEW(s16[3], gPathMin);
#define gPathMin gPathMin_view
VIEW(PathGraph, gResPath);
#define gResPath gResPath_view
VIEW(PathPoint *, gResPathPtList);
#define gResPathPtList gResPathPtList_view

s32 hud_animTrackMap(Blit *blt) {
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
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    if (blt->Hide) {
        return 1;
    }
    if (!(blt->AnimID & 0x10)) {
        return 1;
    }
    if (gNumWindows == 4) {
        RenameBlit(blt, "MAP_MD", 0);
    }
    kMapSize = blt->Width;
    if (blt->AnimID & 0x10) {
        blt->AnimID = blt->AnimID & 0xF;
        blt->X = hud_mapPos[gNumWindows - 1].x - kMapSize / 2;
        blt->Y = hud_mapPos[gNumWindows - 1].y - kMapSize / 2;
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
            TEXELS(blt->info)[pz * kMapSize / 2 + px] = 0;
        }
    }
    for (i = 0; i < gResPath.count; i++) {
        if (i >= gResPath.count) {
            for (j = 0; j < gResPath.numSets; j++) {
                if (&gResPathPtList[i] >= gResPath.sets[j].points && &gResPathPtList[i] < gResPath.sets[j].points + gResPath.sets[j].count) {
                    break;
                }
            }
            if (gResPath.sets[j].type == 0) {
                i += gResPath.sets[j].count - 1;
                continue;
            }
        }
        px = (gResPath.points[i].pos[0] - gPathMin[0]) * w / dx + offx;
        pz = (gResPath.points[i].pos[2] - gPathMin[2]) * h / dz + offz;
        for (y = pz - 2; y <= pz + 2; y++) {
            for (x = px - 2; x <= px + 2; x++) {
                if (x >= 0 && x < kMapSize && y >= 0 && y < kMapSize) {
                    col = gMirrorWaves == 0 ? kMapSize - x - 1 : x;
                    row = TEXELS(blt->info) + (kMapSize - y - 1) * kMapSize / 2 + col / 2;
                    if (x <= px - 2 || x >= px + 2 || y <= pz - 2 || y >= pz + 2) {
                        if (col & 1) {
                            if ((*row & 0xF) != 0xE) {
                                *row = (*row & 0xF0) | 0xD;
                            }
                        } else {
                            if ((*row & 0xF0) != 0xE0) {
                                *row = (*row & 0xF) | 0xD0;
                            }
                        }
                    } else if (col & 1) {
                        *row = (*row & 0xF0) | 0xE;
                    } else {
                        *row = (*row & 0xF) | 0xE0;
                    }
                }
            }
        }
    }
    return 1;
}
