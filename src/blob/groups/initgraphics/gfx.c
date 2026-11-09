#include "game/game.h"

VIEW(u8 *, frame_dl);
#define frame_dl frame_dl_view

void InitGraphics(void)
{
    InitGfx();
    frame_dl_start = frame_dl;
    ZOID_Reset();            /* ZOID_Init */
    InitBlits();
    ZeroCameras();    /* InitCameras */
}
