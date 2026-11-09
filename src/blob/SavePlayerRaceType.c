#include "game/game.h"

void PS_WriteFile(void **arg0, uintptr_t arg1, s32 arg2);

void SavePlayerRaceType(u8 ***arg0, s32 arg1) {
    u8 *p;

    p = *(u8 **) *(u8 ***) ((u8 *) *arg0 + PROFILE_OFS(0x2C));
    if (arg1 != p[0x47]) {
        p[0x47] = arg1;
        PS_WriteFile(*(void ***) ((u8 *) *arg0 + PROFILE_OFS(8)), (uintptr_t) (*(u8 **) *(u8 ***) ((u8 *) *arg0 + PROFILE_OFS(0x2C)) + 0x47), 1);
    }
}
