/* animStatText: the car select screen's stat bars' labels (MAT3 carscr.c animStatText). */
#include "game/game.h"

typedef struct Texture { u8 pad0[18]; u16 height; u8 pad20[16]; } Texture;
#define TEXT_STAT_NAMES 13          /* gText.tab[]: the four stats' names */

/* The labels' text blit callback: in a one-player race other than a ghost race, each of the four stats' names over
 * its bar (the bars are an eighth of the bars texture's height apart, twice that a stat). Not while the car is
 * locked. Always 1. */
s32 animStatText(s32 i)
{
    s32 j;
    s16 index;
    s32 y;
    Texture *texture;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(12);
    TB_SetTextColor(kTB_WhiteText);
    for (i = 0; i < gNumPlayers; i++) {
        if (gCarLocked[i] == 1) {
            continue;
        }
        for (j = 0; j < 4; j++) {
            if (i < gNumPlayers && gNumPlayers == 1 && gRace != kGhost) {
                texture = MBOX_FindTexture_WORK(carscr_stat_text_texture_name, &index, 0, gNumModels[0] - 1, 1);
                y = 48 + 2 * j * (texture->height / 8);
                TB_DrawText(10, y - TB_GetFontLineHeight(), gText.taba[gText.tab[TEXT_STAT_NAMES] + j]);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
