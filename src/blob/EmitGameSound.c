#include "game/game.h"

extern int AudEmitEffect(const f32 *,intptr_t,f32,f32,f32,f32,int,int,int,u8);
int EmitGameSound(const f32 *first,intptr_t second,f32 x,f32 y,f32 z,f32 scalar,int index,int mode,int flags,u8 final) {
    if (!gGameSoundsEnabled) return -1;
    if (index==-1) return -1;
    return AudEmitEffect(first,second,x,y,z,scalar,index,mode,flags,final);
}
