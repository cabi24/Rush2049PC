#include "game/game.h"

s32 ZOID_AddObject();

/* This wrapper ends in a call to ZOID_AddObject (MBOX_NewObject), whose result stays in v0 through the void
 * return; every caller reads it. On the PC the wrapper returns it (docs/port/PROTOTYPES.md). */
#define PC_V0_S32 s32
#define PC_V0_RETURN return
PC_V0_S32 MBOX_NewObjectCam(s32 arg0, intptr_t arg1, s16 arg2, s32 arg3, s32 arg4)
{
  s16 x;
  x = arg3;
  PC_V0_RETURN ZOID_AddObject(arg0, arg1, x, ((arg2 << 8) ^ 0xF00) | arg4);
}
