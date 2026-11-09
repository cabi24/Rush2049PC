#include "game/game.h"

VIEW(intptr_t, gEnvList);
#define gEnvList gEnvList_view

/* The list gEnvList links the environment visuals (gEnvirons, struct pc_visual on the PC: next and func are
 * host pointers, func at 0x18), so the walk holds them pointer-sized (docs/port/CASTS.md). */
void UpdateEnvirons(void) {
    M2C_UNK (*temp_v0)(intptr_t, M2C_UNK);
    intptr_t var_a0;
    intptr_t var_s0;

    var_a0 = gEnvList;
    if (gEnvList != 0) {
        var_s0 = M2C_FIELD(gEnvList, intptr_t *, 0);
    } else {
        var_s0 = 0;
    }
    if (var_a0 != 0) {
        do {
            temp_v0 = M2C_FIELD(var_a0, M2C_UNK (**)(intptr_t, M2C_UNK), VIS_OFS(0x14));
            if (temp_v0 != NULL) {
                temp_v0(var_a0, 1);
            }
            var_a0 = var_s0;
            if (var_s0 != 0) {
                var_s0 = M2C_FIELD(var_a0, intptr_t *, 0);
            }
        } while (var_a0 != 0);
    }
    AnimateSkids();
}
