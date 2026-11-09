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

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);

void options_initScreen(void)
{
    s16 y;
    s32 u0;

    SaveGlobals();
    op_calcOptionMenuMaxSize();
    options_gSelectedOption = 0;
    while (!options_validOption[options_gSelectedOption]) {
        if (++options_gSelectedOption >= 14) options_gSelectedOption = 0;
    }
    options_gFirstOption = options_gSelectedOption;
    exitOptionScreen = 0;
    if (gGameState & 0x7C03FFFE) {
        createBG();
        op_initButtons();
        options_blit = NewMultiBlit(0, 0, &options_frontEndBlit, 1);
    } else {
        options_gControllerRemoved = !gControllerData[playerInPause].attached;
        HAL_SetFont(13);
        options_TitlePane();
        y = TB_GetFontLineHeight() * 2 + 10;
        HAL_SetFont(11);
        InitPane(40, y - 4, 280, y + (gOptionMenuSize * TB_GetFontLineHeight() + 4), 176, 0, 0, 0);
        options_blit = NewMultiBlit(0, 0, &options_pauseBlit, 1);
    }
    ClearRushControls();
    options_inited = 1;
}

