#include "game/game.h"

void ZOID_SetScissor(u16 *out,u16 x,u16 y,u16 z,u16 w) {
 out[0]=x;
 out[1]=y;
 out[2]=z;
 out[3]=w;
}
