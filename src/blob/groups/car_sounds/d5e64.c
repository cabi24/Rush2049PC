#include "game/game.h"

typedef struct { float a; s32 pad[3]; float b; s32 c; } Ent;      /* 24 bytes */
typedef struct Slot { Ent e[5]; } Slot;                                 /* 120 bytes */

VIEW(Slot[], gBump);
#define gBump gBump_view

void InitBumps(s16 idx)
{
    s32 i;
    for (i = 0; i < 5; i++) {
        gBump[idx].e[i].c = 0;
        gBump[idx].e[i].a = 0.0f;
        gBump[idx].e[i].b = 0.0f;
    }
    AudStopEffect(gScrapes[idx]);
    gScrapes[idx] = -1;
    gScrapeState[idx] = 0;
    gScrapeTime[idx] = 0.0f;
    gBumpTime[idx] = 0.0f;
    gThumpTime[idx] = 0.0f;
}

typedef struct Player84 {Definitions64 *definition;State20 pair[3];State20 extra;} Player84;

typedef struct Vehicle2056 {u8 prefix[12];s8 category;u8 gap13[1587];s8 disabled;u8 gap1601[389];s16 player;u8 gap1992[4];s8 mode;u8 tail[59];} Vehicle2056;

VIEW(char[], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view

extern u32 AudEmitEffect(void *,void *,float,float,float,float,u32,u32,u32,u32);

void StartCarSounds(Vehicle2056 *vehicle)
{
 s32 player=vehicle->player;
 Player84 *state;
 int i;
 s32 object;
 object=(s32)&PC_CAR_SOUNDS_ON(player);
 state=PC_ENGINE_SOUND(player);
 if(!gGameSoundsEnabled || vehicle->disabled)return;
 if(&PC_CAR_SOUNDS_ON(player)!=0);
 state->definition=&gEngineSpecs[vehicle->category];
 for(i=0;i<2;i++) {
  InitSoundState(&state->pair[i]);
  object=state->definition->item[i].object;
  if(object==-1) continue;
  {
   if(vehicle->mode==2)state->pair[i].handle=AudPlayEffectVP(object,player,0,0,0.0f,0.0f,0.0f);
   else state->pair[i].handle=AudEmitEffect(unitmatrix_origin,unitmatrix_origin,400.0f,0.0f,1.0f,0.0f,object,player,0,128);
  }
 }
 InitSoundState(&state->extra);
 InitSoundState(PC_SCREECH(player));
 if(player<4) {
  InitSoundState(&gRoadNoises[player].values[0]);
  InitSoundState(&gRoadNoises[player].values[1]);
  InitSoundState(&gRoadNoises[player].values[2]);
  gScrapes[player]=-1;
  InitBumps(player);
  gHornSounds[player]=-1;
  gBogSounds[player]=-1;
 }
 PC_CAR_SOUNDS_ON(player)=1;
}

