/* InitGraphics (MAT3 Init.c): the graphics' start-up, as a global function. Nothing calls it on the PC: InitSystem
 * uses its own copy (heap_release/initsystem.c's InitGraphics_Init). */
#include "game/game.h"

VIEW(u8 *, frame_dl);
#define frame_dl frame_dl_view

/* The video and frames (InitGfx), the scene objects, the blits, the cameras. */
void InitGraphics(void)
{
    InitGfx();
    frame_dl_start = frame_dl;
    ZOID_Reset();
    InitBlits();
    ZeroCameras();
}
