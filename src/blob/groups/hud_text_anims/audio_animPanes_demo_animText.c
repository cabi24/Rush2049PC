/* The audio screen's and the demo's text animators (MAT3 audioscr.c's animPanes; demo.c's animText): blit
 * animation functions, run each frame through their blits. */
#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

typedef struct { s8 flag; s8 pad[15]; } Slot16;
VIEW(Slot16[], gControllerData);
#define gControllerData gControllerData_view

VIEW(s32[3], audio_menuRect);
#define audio_menuRect audio_menuRect_view

typedef struct { u32 first, second; } Pair;
VIEW(Pair, gJust);
#define gJust gJust_view

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
s32 InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);

/* The menu box's left, width and height (audio_menuRect, audio_calcMenuRect's). */
typedef struct { s32 x, w, h; } MenuRect;

/* The audio screen's panes, made once: in the pause screen the title pane (with "controller removed" if the
 * pausing player's controller is out) and the menu's backing pane around audio_menuRect. blit is unused; returns 1
 * (keep animating). The unused locals x, unused1 and unused2 are left from the N64 build's stack frame. */
s32 audio_animPanes(s32 blit)
{
    s16 x;
    s16 y;
    s32 unused1;
    s32 unused2;

    if (!audio_panesCreated) {
        if (!(gGameState & GS_FRONTEND_MASK)) {
            HAL_SetFont(13);
            audio_gControllerRemoved = !gControllerData[playerInPause].flag;
            audio_TitlePane();
            y = 10 + 2 * TB_GetFontLineHeight();
            HAL_SetFont(10);
            InitPane(((MenuRect *)audio_menuRect)->x - 8, y - 4,
                              ((MenuRect *)audio_menuRect)->x + ((MenuRect *)audio_menuRect)->w + 8,
                              y + ((MenuRect *)audio_menuRect)->h + 4, 176, 0, 0, 0);
        }
        audio_panesCreated = 1;
    }
    return 1;
}

/* The attract-mode demo's text each frame: the alternate message and a line of the text bank, or the usual line,
 * in scroll boxes at the screen's bottom (and top). blit is unused; returns 1. */
s32 demo_animText(s32 blit)
{
    char buf[40];   /* unused: left from the N64 build's stack frame */

    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    if (attract_alt_text_flag == 1U) {   /* 1U shares the u32 1 of the inlined TB_SetJust(1, 1) stores */
        HAL_SetFont(13);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 10, 320, 110, -1, 0, demo_alt_message);
        TB_DrawTextScrollBox(160, 190, 320, 110, -1, 0, gText.single[33]);
    } else {
        HAL_SetFont(11);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 185, 320, 110, -1, 0, gText.single[32]);
        TB_SetTextColor(1);
    }
    gJust.second = 3;
    gJust.first = 0;
    HAL_SetTextDepth(-1.0f);
    return 1;
}
