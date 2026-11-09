/* are_sound_voices_idle: whether every sound effect has stopped (the N64's sound manager). */
#include "game/game.h"

/* A sound effect instance as this reads it: whether it plays. */
typedef struct SfxInstance {s8 active;u8 pad[23];} SfxInstance;

VIEW(SfxInstance *, gSfxInstances);
#define gSfxInstances gSfxInstances_view

/* 1 when none of the gNumInstances sound effect instances is playing, else 0. */
s32 are_sound_voices_idle(void) {s32 i;for(i=0;i<gNumInstances;i++){if(gSfxInstances[i].active)return 0;}return 1;}
