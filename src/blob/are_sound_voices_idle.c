#include "game/game.h"

typedef struct Record {s8 active;u8 pad[23];} Record;

VIEW(Record *, gSfxInstances);
#define gSfxInstances gSfxInstances_view
s32 are_sound_voices_idle(void) {s32 i;for(i=0;i<gNumInstances;i++){if(gSfxInstances[i].active)return 0;}return 1;}
