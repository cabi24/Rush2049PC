/* EmitGameSound: a 3D sound of the race, if the game's sounds are on (MAT3 audio.c). */
#include "game/game.h"

/* Starts effect fxid as a sound source at pos (AudEmitEffect: dir unused, fadeDistance, fadeCompression, maxVol
 * and minVol, subId, replace, priority) and returns its fx tag handle; -1 when the game's sounds are off
 * (EnableGameSounds) or fxid is -1. */
int EmitGameSound(const f32 *pos,void *dir,f32 fadeDistance,f32 fadeCompression,f32 maxVol,f32 minVol,int fxid,int subId,int replace,u8 priority) {
    if (!gGameSoundsEnabled) return -1;
    if (fxid==-1) return -1;
    return AudEmitEffect(pos,dir,fadeDistance,fadeCompression,maxVol,minVol,fxid,subId,replace,priority);
}
