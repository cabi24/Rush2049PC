/* are_sound_voices_idle: whether every sound effect has stopped (the N64's sound manager). */
#include "game/game.h"
#include "game/audiomgr.h"

/* 1 when none of the gNumInstances sound effect instances (the sound server's, gSfxInstances) is in use, else 0. */
s32 are_sound_voices_idle(void) {s32 i;for(i=0;i<gNumInstances;i++){if(gSfxInstances[i].used)return 0;}return 1;}
