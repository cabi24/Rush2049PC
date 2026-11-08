/*
 * animLapCount (0x80107EDC, 632 B): per-window lap counter text (HUD). Shown unless gGameState bit 3,
 * when bit 22 is set, the race state gRace is not 1/5 and gShowPlace is set; font 11 (one window) or
 * 12; for each window whose lap D_8014A250[i].lap (+2024, stride 2056) differs from gNumLaps and whose
 * car gGameCar[i].f239 != 1: drop-shadowed text (colour 0 at x+1,y+1 then colour 1) of the format at
 * StrTable header +0x338 with lap+1 and gNumLaps (TB_Printf is the formatted-text print).
 * Shaping (disclosed): gfx_lock/gfx_unlock/font_set are the family's inlined helpers (non-static so umerge
 * leaves no deleted-procedure stubs); the two `continue` guards give retail's s0..s3 order (nested ifs
 * colour the strength-reduced lap pointer first).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct TblHdr {
    char *pad0[0x338 / 4];
    char *text;
} TblHdr;

typedef struct StrTable {
    u8 pad0[4];
    TblHdr *hdr;
} StrTable;

typedef struct Pos {
    s32 x, y;
} Pos;

typedef struct Pair {
    s32 a, b;
} Pair;

typedef struct Lap {
    u8 pad0[2024];
    s8 lap;
    u8 pad7E9[2056 - 2025];
} Lap;

typedef struct Car {
    u8 pad0[239];
    s8 f239;
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x380 - 0x308]; void *input; u8 padF0_384[0x3B8 - 0x384];
} Car;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s32 gRace;
extern s8 gShowPlace;
extern Pair gJust;
extern StrTable gText;
extern s16 gNumWindows;
extern s16 gNumLaps;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Pos lapCountPos[][4];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void HAL_SetTextDepth(f32);
void TB_SetTextColor(s32);
void TB_Printf(s16, s16, char *, s32, s32);

void gfx_lock_EDC(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_EDC(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_EDC(s32 font)
{
    s32 old;

    gfx_lock_EDC();
    old = HAL_SetFontInternal(font);
    gfx_unlock_EDC();
    return old;
}

s32 animLapCount(void *item)
{
    s32 i;
    s32 x;
    s32 y;

    if ((gGameState & 8) || !(gGameState & 0x400000) || gRace == 1 || gRace == 5 || !gShowPlace) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    gJust.b = 3;
    gJust.a = 1;
    if (gNumWindows == 1) {
        font_set_EDC(11);
    } else {
        font_set_EDC(12);
    }
    for (i = 0; i < gNumWindows; i++) {
        if (gNumLaps == PC_MODEL_LAP(i)) {
            continue;
        }
        if (PC_CAR_FINISHED(i) == 1) {
            continue;
        }
        x = lapCountPos[gNumWindows - 1][i].x;
        y = lapCountPos[gNumWindows - 1][i].y;
        TB_SetTextColor(0);
        TB_Printf(x + 1, y + 1, gText.hdr->text, PC_MODEL_LAP(i) + 1, gNumLaps);
        TB_SetTextColor(1);
        TB_Printf(x, y, gText.hdr->text, PC_MODEL_LAP(i) + 1, gNumLaps);
    }
    gJust.b = 3;
    gJust.a = 0;
    HAL_SetTextDepth(-1.0f);
    return 1;
}
