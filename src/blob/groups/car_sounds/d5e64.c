/* A car's sounds at its start (MAT3 audio.c's StartCarSounds and _InitBumps). */
#include "game/game.h"

/* Clears the bump state of the car in slot (its five parts' peaks), stops its scrape and resets its bump, scrape
 * and thump timers. */
void InitBumps(s16 slot)
{
    s32 i;
    for (i = 0; i < 5; i++) {
        gBump[slot].point[i].bump = 0;
        gBump[slot].point[i].peak = 0.0f;
        gBump[slot].point[i].time = 0.0f;
    }
    AudStopEffect(gScrapes[slot]);
    gScrapes[slot] = -1;
    gScrapeState[slot] = 0;
    gScrapeTime[slot] = 0.0f;
    gBumpTime[slot] = 0.0f;
    gThumpTime[slot] = 0.0f;
}

/* The engine record of a car (pc_engine_sounds: MAT3's specs, state[2], hum, rev): its car type's layers, their
 * two sounds, the hum (which the N64 never starts) and the drones' random engine sound. */
typedef struct EngineSounds {Definitions64 *specs;tSoundState state[2];tSoundState hum;tSoundState rev;} EngineSounds;

/* The model record's fields this file reads (MAT3 tModelData, the N64's layout). */
typedef struct Vehicle2056 {u8 prefix[12];s8 engine_type;u8 gap13[1587];s8 crashflag;u8 gap1601[389];s16 slot;u8 gap1992[4];s8 drone_type;u8 tail[59];} Vehicle2056;

/* A human's car (tModelData.drone_type): plain effects; a drone's sounds are 3D emitters. */
#define DRONE_TYPE_HUMAN 2

extern u32 AudEmitEffect(void *,void *,float,float,float,float,u32,u32,u32,u32);

/* Starts the sounds of the car of model vehicle, unless the game's sounds are off or the car has crashed: its
 * engine type's two layers (silent effects for a human's car, emitters for a drone), its other records empty, and
 * for the first four slots its road noises, scrape, bumps, horn and bog reset. Then marks its sounds on
 * (pc_car_sounds_on), which StopCarSounds clears. */
void StartCarSounds(Vehicle2056 *vehicle)
{
 s32 slot=vehicle->slot;
 EngineSounds *engine;
 int i;
 s32 fxid;
 fxid=(s32)&PC_CAR_SOUNDS_ON(slot);   /* a dead store and a test of nothing, left from the decompilation */
 engine=PC_ENGINE_SOUND(slot);
 if(!gGameSoundsEnabled || vehicle->crashflag)return;
 if(&PC_CAR_SOUNDS_ON(slot)!=0);
 engine->specs=&gEngineSpecs[vehicle->engine_type];
 for(i=0;i<2;i++) {
  InitSoundState(&engine->state[i]);
  fxid=engine->specs->item[i].id;
  if(fxid==-1) continue;
  {
   if(vehicle->drone_type==DRONE_TYPE_HUMAN)engine->state[i].sound=AudPlayEffectVP(fxid,slot,0,0,0.0f,0.0f,0.0f);
   else engine->state[i].sound=AudEmitEffect(unitmatrix_origin,unitmatrix_origin,400.0f,0.0f,1.0f,0.0f,fxid,slot,0,128);
  }
 }
 InitSoundState(&engine->rev);
 InitSoundState(PC_SCREECH(slot));
 if(slot<4) {
  InitSoundState(&gRoadNoises[slot].dirt);
  InitSoundState(&gRoadNoises[slot].water);
  InitSoundState(&gRoadNoises[slot].pave);
  gScrapes[slot]=-1;
  InitBumps(slot);
  gHornSounds[slot]=-1;
  gBogSounds[slot]=-1;
 }
 PC_CAR_SOUNDS_ON(slot)=1;
}
