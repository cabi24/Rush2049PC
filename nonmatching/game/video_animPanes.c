#include "game/game.h"

VIEW(s8[][16], gControllerData);
#define gControllerData gControllerData_view
VIEW(s32[3], video_menuRect);
#define video_menuRect video_menuRect_view

s32 video_animPanes(void *item)
{
    s32 u0;
    s16 y;
    s32 h;
    s8 c;

    if (!video_panesCreated) {
        if (!(gGameState & 0x7C03FFFE)) {
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
