#include "game/game.h"

VIEW(s8[][16], gControllerData);
#define gControllerData gControllerData_view
VIEW(s32[3], video_menuRect);
#define video_menuRect video_menuRect_view

void *InitPane(s32 x1, s32 y1, s32 x2, s32 y2, s32 style, intptr_t word40, s32 word44, s32 own);

s32 video_animPanes(void *item)
{
    s32 u0;
    s16 y;
    s32 h;
    s8 c;

    if (!video_panesCreated) {
        if (!(gGameState & GS_FRONTEND_MASK)) {
            HAL_SetFont(13);
            c = gControllerData[playerInPause][0];
            video_gControllerRemoved = c == 0;
            video_TitlePane();
            h = TB_GetFontLineHeight() * 2 + 10;
            y = h;
            InitPane(video_menuRect[0] - 8, y - 4, video_menuRect[1] + video_menuRect[0] + 8, y + video_menuRect[2] + 4,
                              176, 0, 0, 0);
        }
        InitPane(20, 136, 300, 204, 176, 0, 0, 0);
        video_panesCreated = 1;
    }
    return 1;
}
