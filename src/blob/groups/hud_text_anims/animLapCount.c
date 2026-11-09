#include "game/game.h"

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

VIEW(Pair, gJust);
#define gJust gJust_view
VIEW(StrTable, gText);
#define gText gText_view

VIEW(Pos[][4], lapCountPos);
#define lapCountPos lapCountPos_view

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
        HAL_SetFont(11);
    } else {
        HAL_SetFont(12);
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
