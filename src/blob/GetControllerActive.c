#include "game/game.h"

VIEW(s8, gActiveControllers);
#define gActiveControllers gActiveControllers_view

/* lui 0x8015 base of gActiveControllers[arg0]; the symbol lives in an N64-layout arena on PC */
#define N64_HI_80150000 ((intptr_t) &gActiveControllers + 0x649C)

s8 GetControllerActive(s32 arg0) {
    s8 *active = (s8 *) &gActiveControllers;   /* s8[4]: each controller's */
    if (arg0 == -1) {
        if (active[0] == 0) {
            return 0;
        }
        if (active[1] == 0) {
            return 0;
        }
        if (active[2] == 0) {
            return 0;
        }
        if (active[3] == 0) {
            return 0;
        }
        return 1;
    }
    return M2C_FIELD((arg0 + N64_HI_80150000), s8 *, -0x649C);
}
