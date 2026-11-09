#include "game/game.h"

typedef struct Blit {
    /* 0x00 */ char *name;
    /* 0x04 */ void *palette;
    /* 0x08 */ void *texture;
    /* 0x0C */ u16 texIndex;
    /* 0x0E */ s16 x;
    /* 0x10 */ s16 y;
    /* 0x12 */ u16 z;
    /* 0x14 */ s16 width;
    /* 0x16 */ s16 height;
    /* 0x18 */ u8 alpha;
    /* 0x19 */ u8 flip;
    /* 0x1A */ s8 hide;
    /* 0x1B */ s8 init;
    /* 0x1C */ s16 top, bot, left, right;
    /* 0x24 */ s16 color;
    /* 0x28 */ void *animFunc;
    /* 0x2C */ u32 animID;
    /* 0x30 */ intptr_t animDTA;
} Blit;

typedef struct CheckPoint {
    f32 pos[3];
    f32 normal[3];
    s32 radius;
    s16 flags;
    s16 time[2];
    s16 pathPts[20];
    f32 distToNext;
} CheckPoint;

typedef struct Checkpoints {
    s16 startTime;
    s16 loopPt;
    s16 finishPt;
    s16 startPt;
    s16 numPoints;
    CheckPoint points[10];
} Checkpoints;

VIEW(s16[3], gPathMax);
#define gPathMax gPathMax_view
VIEW(s16[3], gPathMin);
#define gPathMin gPathMin_view

VIEW(s32[4][2], hud_mapPos);
#define hud_mapPos hud_mapPos_view

VIEW(Checkpoints, gCheckpoints);
#define gCheckpoints gCheckpoints_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

s32 Hidden_9F54(Blit *blt, s32 hide)
{
    if (hide != blt->hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    return blt->hide;
}

s32 animFlag(Blit *blt)
{
    s32 x, y, width, height, w, h, xoff, yoff;

    if (Hidden_9F54(blt, gNumWindows > 4 || gNumLapsLeft > 1 || gRace == 6 || gRace == 4 ||
                         gRace == 5 || !gShowTrackMap))
        return 1;
    width = gPathMax[0] - gPathMin[0];
    height = gPathMax[2] - gPathMin[2];
    if (width > height) {
        w = kMapSize - 8;
        h = (0 + height) * (kMapSize - 8) / width;
    } else {
        h = kMapSize - 8;
        w = (0 + width) * (kMapSize - 8) / height;
    }
    xoff = (kMapSize - 8 - w) / 2 + 4;
    yoff = (kMapSize - 8 - h) / 2 + 4;
    x = hud_mapPos[gNumWindows - 1][0] + xoff - kMapSize / 2;
    y = hud_mapPos[gNumWindows - 1][1] + yoff - kMapSize / 2;
    if (gMirrorWaves)
        x += (gCheckpoints.points[gCheckpoints.finishPt].pos[0] - gPathMin[0]) * w / width;
    else
        x += w - 1 - (gCheckpoints.points[gCheckpoints.finishPt].pos[0] - gPathMin[0]) * w / width;
    y += (gCheckpoints.points[gCheckpoints.finishPt].pos[2] - gPathMin[2]) * h / height;
    blt->x = x - blt->width / 2;
    blt->y = y - blt->width / 2;
    if (Hidden_9F54(blt, gRace == 1 || gNumLapsLeft > 1 || (gGameState & 8))) {
        if (gRace != 1 && !(gGameState & 8)) {
            if (gNumWindows < 4)
                HAL_SetFont(6);
            else
                HAL_SetFont(7);
            HAL_SetTextDepth(0.0f);
            TB_SetJust(1, 1);
            TB_SetTextColor(0);
            TB_Printf(x + 1, y + 1, "%d", gNumLapsLeft);
            TB_SetTextColor(11);
            TB_Printf(x, y, "%d", gNumLapsLeft);
            TB_SetJust(0, 3);
            HAL_SetTextDepth(-1.0f);
        }
        return 1;
    }
    if (gMirrorWaves)
        blt->flip = 1;
    blt->alpha = 128;
    if (!gPauseState)
        blt->animDTA -= gFrameSeconds * 1000.0f;
    if (blt->animDTA <= 0) {
        blt->animDTA = 100;
        blt->animID = (blt->animID + 1) % 5;
    }
    SelectBlit(blt, blt->animID, blt->width, blt->width);
    return 1;
}
