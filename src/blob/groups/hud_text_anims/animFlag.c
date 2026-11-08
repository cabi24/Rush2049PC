/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_80109F54 (0x80109F54, 1504 B) = GC hud.c animFlag (MAT3 decomp/src/GameSource/src/hud.c animFlag,
 * GC 0x80021DE4; GC locals x, y, width, height, w, h). Checkered-flag blit drawn on the track map at the
 * finish checkpoint (gCheckpoints.points[finishPt]), with the laps-left digit printed over it on the last lap.
 * Globals: gNumWindows, gNumLapsLeft = gNumLapsLeft, gRace = gRace, gShowTrackMap = gShowTrackMap, gPathMax/Min =
 * gPathMax/gPathMin, kMapSize = kMapSize, mapPos = hud_mapPos, gMirrorWaves = gMirrorWaves, gCheckpoints =
 * gCheckpoints, gGameState = gGameState, gPauseState = gPauseState, gFrameSeconds = gFrameSeconds (volatile, as MAT3),
 * TB_SetJust pair = gJust; UpdateBlit = UpdateBlit, HAL_SetFont = HAL_SetFontInternal (s2 IPA
 * parameter), HAL_SetTextDepth = HAL_SetTextDepth, TB_SetTextColor = TB_SetTextColor, TB_Printf =
 * TB_Printf, SelectBlit = SelectBlit.
 * N64 differences from GC: no gGameState 0x400000 term in the first Hidden; the map-centre offsets are two more
 * named locals (xoff, yoff: frame 136 = two slots more than GC's six; computed before mapPos) without GC's outer
 * /2; the finish offset is not halved; blt->x/y subtract width/2 once.
 * Levers: operand order `(pos - min) * w / width`, `gFrameSeconds * 1000.0f`; TB_SetJust stores .h then .v.
 * Shaping (disclosed): `(0 + width)` / `(0 + height)` on the variable side of the two aspect multiplies (no code;
 * uopt puts a plain variable second in a commutative op, retail multiplies with the variable first). Helpers
 * gfx_lock/gfx_unlock/font_set/TB_SetJust/Hidden_9F54 are inlined, non-static, not on keep (stub rule).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 */typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

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

typedef struct Pair {
    s32 h, v;
} Pair;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;          /* gGameState */
extern s32 gRace;          /* gRace */
extern s16 gNumWindows;
extern s8 gNumLapsLeft;           /* gNumLapsLeft */
extern s8 gShowTrackMap;           /* gShowTrackMap */
extern s16 gPathMax[3];       /* gPathMax */
extern s16 gPathMin[3];       /* gPathMin */
extern s32 kMapSize;          /* kMapSize */
extern s32 hud_mapPos[4][2];    /* mapPos */
extern s8 gMirrorWaves;           /* gMirrorWaves */
extern Checkpoints gCheckpoints;  /* gCheckpoints */
extern s32 gPauseState;          /* gPauseState */
extern volatile f32 gFrameSeconds; /* gFrameSeconds */
extern Pair gJust;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(Blit *);
void HAL_SetTextDepth(f32);
void TB_SetTextColor(s32);
void TB_Printf(s16, s16, char *, ...);
void SelectBlit(Blit *blt, s32 index, s32 w, s32 h);

void gfx_lock_9F54(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_9F54(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_9F54(s32 font)
{
    s32 old;

    gfx_lock_9F54();
    old = HAL_SetFontInternal(font);
    gfx_unlock_9F54();
    return old;
}

void TB_SetJust_9F54(s32 h, s32 v)
{
    gJust.h = h;
    gJust.v = v;
}

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
                font_set_9F54(6);
            else
                font_set_9F54(7);
            HAL_SetTextDepth(0.0f);
            TB_SetJust_9F54(1, 1);
            TB_SetTextColor(0);
            TB_Printf(x + 1, y + 1, "%d", gNumLapsLeft);
            TB_SetTextColor(11);
            TB_Printf(x, y, "%d", gNumLapsLeft);
            TB_SetJust_9F54(0, 3);
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
