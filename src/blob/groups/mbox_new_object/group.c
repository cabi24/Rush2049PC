#include "game/game.h"

s32 ZOID_AddObject(u32 value, uintptr_t data, s16 index, u32 flags);

/* This wrapper ends in a call to ZOID_AddObject (MBOX_NewObject), whose result stays in v0 through the void
 * return; every caller reads it. On the PC the wrapper returns it (docs/port/PROTOTYPES.md). */
#define PC_V0_S32 s32
#define PC_V0_RETURN return
PC_V0_S32 MBOX_NewObject(u32 value, uintptr_t data, s32 index, u32 flags)
{
    PC_V0_RETURN ZOID_AddObject(value, data, (s16) index, flags);
}
