/* options_initScreen: the options screen's set-up (MAT3 optionscr.c initScreen). */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;

typedef struct { s8 attached; u8 pad[15]; } ContData;

VIEW(const MultiBlit, options_frontEndBlit);
#define options_frontEndBlit options_frontEndBlit_view
VIEW(const MultiBlit, options_pauseBlit);
#define options_pauseBlit options_pauseBlit_view

VIEW(s32[14], options_validOption);
#define options_validOption options_validOption_view

VIEW(ContData[], gControllerData);
#define gControllerData gControllerData_view

#define OPTIONS_ROWS 14
#define PANE_STYLE 176              /* InitPane's style for the screen's boxes */

/* Set the screen up: keep the options as they are now (SaveGlobals), size the list, select the first offered row.
 * In the front end: the background, the 3D buttons and the text blit. In the pause menu: whether the pausing
 * player's controller is out, the title box and a box for the list, and the pause layout's text blit. The pads'
 * presses so far are dropped. */
void options_initScreen(void)
{
    s16 y;

    SaveGlobals();
    op_calcOptionMenuMaxSize();
    options_gSelectedOption = 0;
    while (!options_validOption[options_gSelectedOption]) {
        if (++options_gSelectedOption >= OPTIONS_ROWS) options_gSelectedOption = 0;
    }
    options_gFirstOption = options_gSelectedOption;
    exitOptionScreen = 0;
    if (gGameState & GS_FRONTEND_MASK) {
        createBG();
        op_initButtons();
        options_blit = NewMultiBlit(0, 0, &options_frontEndBlit, 1);
    } else {
        options_gControllerRemoved = !gControllerData[playerInPause].attached;
        HAL_SetFont(13);
        options_TitlePane();
        y = TB_GetFontLineHeight() * 2 + 10;
        HAL_SetFont(11);
        InitPane(40, y - 4, 280, y + (gOptionMenuSize * TB_GetFontLineHeight() + 4), PANE_STYLE, 0, 0, 0);
        options_blit = NewMultiBlit(0, 0, &options_pauseBlit, 1);
    }
    ClearRushControls();
    options_inited = 1;
}

