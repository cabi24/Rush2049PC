#include "game/game.h"

VIEW(s8[], gPaks);
#define gPaks gPaks_view
void PS_SetRumblePercent(s32 index,u32 value) {gPaks[0x89 + index*772]=value;}
