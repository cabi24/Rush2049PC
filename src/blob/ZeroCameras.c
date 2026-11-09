/* ZeroCameras: every camera back to the origin (MAT3 camera.c). */
#include "game/game.h"

/* Resets the four cameras (ZeroCamera, by window bit) and updates them for drawing. */
void ZeroCameras(void)
{
    s32 i;
    for (i = 0; i < 4; i++) {
        ZeroCamera(i == 0 ? 1 : i == 1 ? 2 : i == 2 ? 4 : 8);
    }
    UpdateCameras();
}
