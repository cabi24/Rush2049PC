#include "game/game.h"

typedef struct Texture { u8 pad0[18]; u16 height; u8 pad20[16]; } Texture;
typedef struct MenuIndices { u8 pad0[26]; u16 first; } MenuIndices;
/* PC: gText's header has host pointers (port/assets/text.c), so its fields sit at host offsets */
typedef struct MenuData {
    void *pad0[3];
    MenuIndices *indices;
    char **text;
} MenuData;

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(MenuData, gText);
#define gText gText_view

Texture *MBOX_FindTexture_WORK(char *, s16 *, s8, s8, s32);

s32 animStatText(s32 i)
{
    s32 frame_slot0;
    s32 j;
    s16 index;
    s32 y;
    Texture *texture;
    s32 frame_slot1;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(12);
    TB_SetTextColor(1);
    for (i = 0; i < gNumPlayers; i++) {
        if (gCarLocked[i] == 1) {
            continue;
        }
        for (j = 0; j < 4; j++) {
            if (i < gNumPlayers && gNumPlayers == 1 && gRace != 2) {
                texture = MBOX_FindTexture_WORK(carscr_stat_text_texture_name, &index, 0, gNumModels - 1, 1);
                y = 48 + 2 * j * (texture->height / 8);
                TB_DrawText(10, y - TB_GetFontLineHeight(), gText.text[gText.indices->first + j]);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
