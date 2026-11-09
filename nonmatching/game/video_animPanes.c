#include "game/game.h"

void *InitPane(s32 x1, s32 y1, s32 x2, s32 y2, s32 style, intptr_t word40, s32 word44, s32 own);

s32 video_animPanes(void *item)
{
    s32 u0;
    s16 y;
    s32 h;
    s8 c;

    if (!video_panesCreated) {
        if (!(gGameState & 0x7C03FFFE)) {
            HAL_SetFont(13);
            c = gControllerData[playerInPause].attached;
            video_gControllerRemoved = c == 0;
            video_TitlePane();
            h = TB_GetFontLineHeight() * 2 + 10;
            y = h;
            InitPane(video_menuRect.x - 8, y - 4, video_menuRect.w + video_menuRect.x + 8, y + video_menuRect.h + 4,
                              176, 0, 0, 0);
        }
        InitPane(20, 136, 300, 204, 176, 0, 0, 0);
        video_panesCreated = 1;
    }
    return 1;
}
