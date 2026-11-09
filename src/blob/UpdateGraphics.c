/* UpdateGraphics (MAT3 gfx.c): the end of a frame's drawing. Nothing calls it on the PC (GameLoop's frame end is
 * UpdateFrame). */
#include "game/game.h"

/* Draws the blits (drawBlits), updates the environment objects (updateEnvirons), updates the cameras and ends the
 * scene's frame. */
void UpdateGraphics(s32 drawBlits, s32 updateEnvirons) {
    if (drawBlits != 0) {
        DrawBlits();
    }
    if (updateEnvirons != 0) {
        UpdateEnvirons();
    }
    UpdateCameras();
    ZOID_EndFrame(0);
}
