#include "game/game.h"


#define SLOT_DATA PROFILE_OFS(0x2C)  /* SaveSlot.data: the profile record's PC layout (pc_prelude.h) */

u8 GetPlayerCarTrans(void **arg0) {
    u8 *p;

    p = *M2C_FIELD(*arg0, u8 ***, SLOT_DATA);
    switch (p[0x47]) {
    case 6:
        return p[0x62];
    case 4:
        return p[0x72];
    case 5:
        return p[0x86];
    }
    return p[0x4E];
}
