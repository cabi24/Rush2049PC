#include "game/game.h"


#define SLOT_DATA PROFILE_OFS(0x2C)  /* SaveSlot.data: the profile record's PC layout (pc_prelude.h) */

u8 GetPlayerRaceType(void **arg0) {
    return M2C_FIELD(*M2C_FIELD(*arg0, void ***, SLOT_DATA), u8 *, 0x47);
}
