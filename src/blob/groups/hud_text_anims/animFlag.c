/* The HUD's finish flag on the track map (MAT3 hud.c animFlag): an animation function of a blit. */
#include "game/game.h"

/* A blit (MAT3 _tBlit_2), the PC's layout. */
typedef struct Blit {
    char *name;
    void *palette;
    void *texture;
    u16 texIndex;
    s16 x;
    s16 y;
    u16 z;
    s16 width;
    s16 height;
    u8 alpha;
    u8 flip;
    s8 hide;
    s8 init;
    s16 top, bot, left, right;
    s16 color;
    void *animFunc;
    u32 animID;         /* the frame shown */
    intptr_t animDTA;   /* milliseconds to the next frame */
} Blit;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* Sets blt's hidden flag to hide, redrawing it when that changes; returns the flag (MAT3 Hidden). */
s32 Hidden_9F54(Blit *blt, s32 hide)
{
    if (hide != blt->hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    return blt->hide;
}

/* On the track map (one to four windows, the map shown, a lap race but not battle, stunt or obstacle, gRace 4-6),
 * from the last lap on: places the flag at the finish line's place on the map (mirrored for a mirrored track). On
 * the last lap, unless practising (1) or in the demo, the laps left are drawn there instead; else the flag waves,
 * a frame each 100 ms, half translucent. Returns 1: the blit stays. */
s32 animFlag(Blit *blt)
{
    s32 x, y, width, height, w, h, xoff, yoff;

    if (Hidden_9F54(blt, gNumWindows > 4 || gNumLapsLeft > 1 || gRace == kBattle || gRace == kStunt ||
                         gRace == kObstacle || !gShowTrackMap))
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
    x = hud_mapPos[gNumWindows - 1].x + xoff - kMapSize / 2;
    y = hud_mapPos[gNumWindows - 1].y + yoff - kMapSize / 2;
    if (gMirrorWaves)
        x += (gCheckpoints.points[gCheckpoints.finishPt].pos[0] - gPathMin[0]) * w / width;
    else
        x += w - 1 - (gCheckpoints.points[gCheckpoints.finishPt].pos[0] - gPathMin[0]) * w / width;
    y += (gCheckpoints.points[gCheckpoints.finishPt].pos[2] - gPathMin[2]) * h / height;
    blt->x = x - blt->width / 2;
    blt->y = y - blt->width / 2;
    if (Hidden_9F54(blt, gRace == kPractice || gNumLapsLeft > 1 || (gGameState & GS_DEMO))) {
        if (gRace != kPractice && !(gGameState & GS_DEMO)) {
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
