#include "game/game.h"

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    void *name, *image, *info;
    u16 texIndex;
    s16 x, y;
    u16 z;
    s16 width, height;
    u8 alpha, flip;
    s8 hidden;
} Blit;

typedef struct LangInfo {
    u16 lang;
} LangInfo;

/* PC: gText's header has host pointers (port/assets/text.c), so its fields sit at host offsets */
typedef struct StrTable {
    void *pad0[3];
    LangInfo *info;
    char **strings;
} StrTable;

VIEW(StrTable, gText);
#define gText gText_view

s32 animCreditText(Blit *blt)
{
    s32 x;
    s32 hide;
    s32 y;
    s16 sx;
    s32 i;

    hide = !(gGameState & 8) || !gShowCredits;
    if (blt->hidden != hide) {
        blt->hidden = hide;
        UpdateBlit(blt);
    }
    if (blt->hidden) {
        return 1;
    }
    switch (gCorner) {
    case 0:
        x = 30;
        y = 30;
        break;
    case 1:
        x = 290 - gCreditWidth;
        y = 30;
        break;
    case 2:
        x = 30;
        y = 155 - gNumCreditNames * 10;
        break;
    default:
        x = 290 - gCreditWidth;
        y = 155 - gNumCreditNames * 10;
        break;
    }
    HAL_SetFont(11);
    TB_SetTextColor(14);
    sx = x;
    TB_DrawText(sx, y, gText.strings[gText.info->lang + gCredit]);
    TB_SetTextColor(1);
    i = 0;
    y += 15;
    for (; i < gNumCreditNames; i++) {
        TB_DrawText(sx, y, cStrCreditNames[gCredit].names[i]);
        y += 10;
    }
    return 1;
}
