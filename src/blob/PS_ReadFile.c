#include "game/game.h"

void PS_ReadFile(void **arg0) {
    PS_ReadPartialFile(arg0, 0, M2C_FIELD(*arg0, s32 *, PSF_OFS(0x40)));
}
