/* TB_GetFlashTextCycle: where the flashing colour is in its cycle (MAT3 TB_text.c). */
#include "game/game.h"

/* The flashing text's mix, 0 to 1 and back twice a second (gColorCycle, TB_TextOncePerFrame). */
f32 TB_GetFlashTextCycle(void) {
    return gColorCycle;
}
