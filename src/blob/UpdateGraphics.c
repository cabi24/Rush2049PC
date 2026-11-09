#include "game/game.h"

void UpdateGraphics(s32 arg0, s32 arg1) {
    if (arg0 != 0) {
        DrawBlits();
    }
    if (arg1 != 0) {
        UpdateEnvirons();
    }
    UpdateCameras();
    ZOID_EndFrame(0);
}
