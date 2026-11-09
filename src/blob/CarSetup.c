#include "game/game.h"

VIEW(u8, gNumActiveCars);
#define gNumActiveCars gNumActiveCars_view

/* On the PC carsetup_flag is pc_carsetup_flags, a flag per car slot with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 8); CarSetup steps from its first byte by the slot. */
#define carsetup_flag PC_CARSETUP_FLAG(0)

void CarSetup(void) {
    s16 temp_s0;
    s16 var_s2;
    s8 *temp_s1;
    void *temp_v0;

    var_s2 = 0;
    if ((s8) gNumActiveCars > 0) {
        do {
            temp_s0 = PC_MODEL_SLOT(var_s2);
            temp_s1 = (s32 *) ((u8 *) &carsetup_flag + temp_s0);
            temp_v0 = PC_MODEL(temp_s0);
            if (M2C_FIELD(temp_v0, s8 *, 0x7DD) != 0) {
                if (*temp_s1 == 0) {
                    SetupCar(temp_s0, M2C_FIELD(temp_v0, u8 *, 8), 1);
                }
                *temp_s1 = 1;
                UpdateVisuals(temp_s0);
            }
            var_s2 += 1;
        } while (var_s2 < (s8) gNumActiveCars);
    }
}

