#include "game/game.h"

/* PC: the entry's address is a 32-bit RDRAM-window address. Unsigned, so callers widen it to a pointer by zero
 * extension (docs/port/ASSETS.md section 4 #3). */
#define WAD_ADDR u32
WAD_ADDR MBGetFromWad(void *arg0, s32 arg1, s32 *arg2) {
    void *var_v1;

    var_v1 = NULL;
    if (arg0 != NULL) {
        var_v1 = MBFindInWad(arg0, arg1);
    }
    if (var_v1 == NULL) {
        if (arg2 != NULL) {
            *arg2 = 0;
        }
        return 0;
    }
    if (arg2 != NULL) {
        *arg2 = M2C_FIELD(var_v1, s32 *, 8);
    }
    return M2C_FIELD(var_v1, s32 *, 4);
}
