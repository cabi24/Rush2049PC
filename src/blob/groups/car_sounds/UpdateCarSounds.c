/* A car's sounds each frame (MAT3 audio.c's UpdateCarSounds, _UpdateEngineSound, _UpdateRoadSounds, _SetSoundVP and
 * the N64's tire screech): the engine's two layers and its random sound, the screech, the road noises, the
 * bumps (UpdateBumps.c), the horn and the bog. A human's car (drone_type 2) plays plain effects, a drone's 3D emitters. */
#include "game/game.h"

/* The model record's fields this file reads (MAT3 tModelData, the N64's layout, which MAT3 shares here:
 * docs/port/CARS.md). */
typedef struct ModelView {
    u8 other0[0x8];
    u8 body_type;           /* 0x8: picks the horn */
    u8 other9[0x61C - 0x9];
    u16 sviscode[4];        /* 0x61C: each tire's surface: 0 pavement, 1 dirt, 2 and 3 water */
    u8 other624[0x65C - 0x624];
    s16 bogState;           /* 0x65C: 1 start the bog sound, 3 stop it */
    u8 other65E[0x758 - 0x65E];
    s16 rwmph;              /* 0x758: the speed at the wheels (MAT3 reckon.rwmph) */
    u8 other75A[0x7C6 - 0x75A];
    s16 slot;               /* 0x7C6 */
    u8 other7C8[0x7CC - 0x7C8];
    s8 drone_type;          /* 0x7CC: 2 a human's car */
    u8 other7CD[0x7D4 - 0x7CD];
    u32 appearance;         /* 0x7D4: 0x10 no horn */
    u8 other7D8[0x7F8 - 0x7D8];
    f32 skidIntensity[4];   /* 0x7F8 */
} ModelView;

/* The engine update's view of the same record. */
typedef struct MODELDAT {
    u8 pad0[0x22C];
    f32 rwr[3];         /* 0x22C: the car's position */
    u8 pad238[0x404 - 0x238];
    f32 engtorque;      /* 0x404 */
    u8 pad408[0x7C6 - 0x408];
    s16 slot;           /* 0x7C6 */
    u8 pad7C8[4];
    s8 drone_type;      /* 0x7CC */
    u8 pad7CD[3];
    s16 rpm;            /* 0x7D0 */
} MODELDAT;

/* A human's car (tModelData.drone_type): its sounds are plain effects, a drone's are 3D emitters. */
#define DRONE_TYPE_HUMAN 2
/* gGameState's bit while the race is not running yet (the drones' engines are not capped, their random sound
 * stopped). */
#define GAME_STATE_NO_RACE 0x400000
/* tModelData.appearance's bit of a car without a horn. */
#define APPEARANCE_NO_HORN 0x10
/* The race type of a ghost race (gRace). */
#define RACE_GHOST 2

/* The effects (AudPlayEffect's fxid): the road noises, the screech, the bog, the first horn, the first of the
 * drones' three random engine sounds. */
#define SFX_DIRT 11
#define SFX_WATER 20
#define SFX_SPLASH 21
#define SFX_BOG 25
#define SFX_HORN_FIRST 26
#define SFX_SCREECH 36
#define SFX_ENGINE_RANDOM_FIRST 98


/* Sends sound its volume and pitch, each only when it has changed since last sent. */
void SetSoundVP(tSoundState *sound,f32 vol,f32 pitch)
{
    if (vol!=sound->vol) {
        sound->vol=vol;
        AudChangeEffectVol(sound->sound,vol);
    }
    if (pitch!=sound->pitch) {
        sound->pitch=pitch;
        AudChangeEffectPitch(sound->sound,pitch);
    }
}

/* The surface noise of a human's car: each tire adds its surface's share (more for the first tires on a surface,
 * by the wheel speed; on dirt at least the tire's skid), and only the loudest surface plays: dirt, or water (with
 * a splash when it starts loud), or pavement, which has no sound of its own. The others are stopped. (MAT3 plays
 * every surface with any share.) */
void UpdateRoadSounds(ModelView *model)
{
    s32 i,numPave,numDirt,numWater;
    f32 paveVol,dirtVol,waterVol;
    f32 vol,scale;
    s16 slot;
    tRoadNoises *noises;
    slot=model->slot;
    numPave=0; numDirt=0; numWater=0;
    dirtVol=0.0f; paveVol=0.0f; waterVol=0.0f;
    for (i=0;i<4;i++) {
        if (numPave|numDirt|numWater|i) {}
        if (model->sviscode[i]==0) {
            vol=(f32)(model->rwmph>>2)/160.0f;
            if(vol<0.0f) vol=-vol;
            vol=vol<0.0f ? 0.0f : vol>1 ? 1 : vol;
            vol=1-(1-vol)*(1-vol);
            numPave++;
            vol/=numPave==1 ? 2.0f : numPave==2 ? 4.0f : 8.0f;
            paveVol+=vol;
        } else if(model->sviscode[i]==1) {
            vol=(f32)(model->rwmph>>2)/80.0f;
            if(vol!=0.0f) {
                if(vol<0.0f) vol=-vol;
                vol=vol<0.0f ? 0.0f : vol>1 ? 1 : vol;
                vol=1-(1-vol)*(1-vol);
            }
            if(vol<model->skidIntensity[i])vol=model->skidIntensity[i];
            numDirt++;
            vol/=numDirt==1 ? 2.0f : numDirt==2 ? 4.0f : 8.0f;
            dirtVol+=vol;
        } else if(model->sviscode[i]==2 || model->sviscode[i]==3) {
            vol=(f32)(model->rwmph>>2)/80.0f;
            numWater++;
            if(vol!=0.0f) {
                if(vol<0.0f)vol=-vol;
                vol=vol<0.0f ? 0.0f : vol>1 ? 1 : vol;
                vol=1-(1-vol)*(1-vol);
            }
            vol/=numWater==1 ? 2.0f : numWater==2 ? 4.0f : 8.0f;
            waterVol+=vol;
        }
    }
    if(paveVol<dirtVol && waterVol<dirtVol) {
        noises=&gRoadNoises[slot];
        if(noises->dirt.sound==-1) {
            AudStopEffect(noises->water.sound);
            InitSoundState(&noises->water);
            AudStopEffect(noises->pave.sound);
            InitSoundState(&noises->pave);
            noises->dirt.sound=AudPlayEffect(SFX_DIRT,model->slot,1,2);
        }
        SetSoundVP(&noises->dirt,dirtVol*0.7f,(dirtVol>0.5f ? dirtVol : 0.5f)-0.25f);
    } else if(paveVol<waterVol && dirtVol<waterVol) {
        noises=&gRoadNoises[slot];
        if(noises->water.sound==-1) {
            AudStopEffect(noises->dirt.sound);
            InitSoundState(&noises->dirt);
            AudStopEffect(noises->pave.sound);
            InitSoundState(&noises->pave);
            if(waterVol>0.33f)AudPlayEffect(SFX_SPLASH,model->slot,2,1);
            noises->water.sound=AudPlayEffect(SFX_WATER,model->slot,1,2);
        }
        SetSoundVP(&noises->water,waterVol,1.0f);
    } else {
        noises=&gRoadNoises[slot];
        if(noises->pave.sound==-1) {
            AudStopEffect(noises->dirt.sound);
            InitSoundState(&noises->dirt);
            AudStopEffect(noises->water.sound);
            InitSoundState(&noises->water);
        }
    }
}
/* PC: an input record (gPlayers) is 80 bytes, its pointer at +0x48 is 8 (docs/port/POINTERS.md 10). */
typedef struct PlayerPad {
    u8 pad0[4];
    s32 edges;          /* 0x4: the buttons pressed this frame */
    u8 pad8[56 - 8];
    s32 hornControl;    /* 0x38: the horn's button (MAT3 controls[10]) */
    u8 pad3C[80 - 60];
} PlayerPad;

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view

VIEW(f32[3], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view
extern PlayerPad gPlayers[];

s32 AudEmitEffect(f32 *,intptr_t,f32,f32,f32,f32,s32,s32,s32,u8);

void AudUpdateEmitter(s32,f32 *,f32 *,f32,f32);

/* The engine record of a car (pc_engine_sounds: MAT3's specs, state[2], hum, rev): its car type's two layers,
 * their sounds, an unused one and the drones' random engine sound. */
typedef struct {
    Definition32 *specs;
    tSoundState rec[4];
} SndSet; /* 0x58 on the PC */

/* The 3D form of the screech's update: moves the emitter of sound to pos with volume vol and pitch. */
void update_skid_emitter(tSoundState *sound, f32 *pos, f32 vol, f32 pitch)
{
    AudUpdateEmitter(sound->sound,pos,unitmatrix_origin,vol,pitch);
}

/* The engine's two layers of the car of model m: each layer's pitch follows the rpm from its base rpm (0 to 2),
 * its volume the rpm through its low, mid and high points, both scaled by the torque. A human's car sends them
 * as effect changes (its pitch lowered more the more players share the screen); a drone's moves its emitters to the
 * car, with its rpm capped at 890 once the race runs, and now and then (one in five tries, after the countdown's
 * start) plays one of three random engine sounds there. */
void UpdateEngineSound(MODELDAT *m) {
    f32 rpm;
    f32 torque;
    f32 unused[2];
    s32 slot;
    s32 i;
    SndSet *set;
    Definition32 *spec;
    f32 pitch;
    f32 vol;
    f32 *pos;
    f32 *origin;

    slot = m->slot;
    if (gGameSoundsEnabled == 0) {
        return;
    }
    if (m->rpm < 0) {
        rpm = -m->rpm;
    } else {
        rpm = m->rpm;
    }
    torque = m->engtorque;
    if (!(gGameState & GAME_STATE_NO_RACE) && m->drone_type != DRONE_TYPE_HUMAN && !(rpm < 890.0f)) {
        rpm = 890.0f;
    }
    set = PC_ENGINE_SOUND(slot);
    origin = unitmatrix_origin;
    for (i = 0; i < 2; i++) {
        if (set->rec[i].sound == -1) {
            return;
        }
        spec = &set->specs[i];
        pitch = ((f32) spec->basePitchRpm / (f32) spec->pitchRpmRange) * (rpm / spec->basePitchRpm - 1.0f) + 1.0f;
        pitch = pitch < 0.0f ? 0.0f : pitch > 2.0f ? 2.0f : pitch;
        if (rpm < spec->lowVolRpm) {
            vol = spec->lowVol;
        } else if (rpm < spec->midVolRpm) {
            vol = (rpm - spec->lowVolRpm) / (spec->midVolRpm - spec->lowVolRpm);
            vol = (spec->midVol - spec->lowVol) * vol + spec->lowVol;
        } else if (rpm < spec->hiVolRpm) {
            vol = (rpm - spec->midVolRpm) / (spec->hiVolRpm - spec->midVolRpm);
            vol = (spec->hiVol - spec->midVol) * vol + spec->midVol;
        } else {
            if (rpm < spec->midVolRpm) {}
            vol = spec->hiVol;
        }
        vol *= 0.85f + ((torque + 200.0f) / 900.0f) * 0.15f;
        if (m->drone_type == DRONE_TYPE_HUMAN) {
            vol *= 0.8f - 0.05f * (gRace == RACE_GHOST ? 1 : gNumPlayers);
            if (pitch != set->rec[i].pitch) {
                set->rec[i].pitch = pitch;
                AudChangeEffectPitch(set->rec[i].sound, pitch);
            }
            if (vol != set->rec[i].vol) {
                set->rec[i].vol = vol;
                AudChangeEffectVol(set->rec[i].sound, vol);
            }
        } else {
            vol *= 0.75f;
            pos = m->rwr;
            if (gGameState & GAME_STATE_NO_RACE) {
                if (set->rec[3].sound != -1) {
                    StopSound(&set->rec[3], 1);
                }
            } else if (0.75f < gCountDown) {
                if (set->rec[3].sound == -1 || AudEmitterPlaying(set->rec[3].sound) == 0) {
                    if ((s32) Random(5.0f) == 0) {
                        set->rec[3].sound = AudEmitEffect(origin, (s32) origin, 400.0f, 0.0f, 1.0f, 0.0f,
                            (s32) (Random(3.0f) + SFX_ENGINE_RANDOM_FIRST), slot, 0, 128);
                        AudUpdateEmitter(set->rec[3].sound, pos, origin, 0.8f, -2.0f);
                    }
                }
            }
            if (pitch != set->rec[i].pitch) {
                set->rec[i].pitch = pitch;
            } else {
                pitch = -2.0f;
            }
            AudUpdateEmitter(set->rec[i].sound, pos, origin, vol, pitch);
        }
    }
}

/* The model record as UpdateCarSounds reads it for the screech's emitter. */
#define MODEL_RWR(model) ((f32 *)((u8 *)(model) + 0x22C))

/* Each frame for each car: updates its sounds on every other frame (the cars alternate, pc_engine_phase): the
 * engine, and for a human's car the tire screech (from the tires' skids: volume up to 0.6 each, pitch from 0.5),
 * the road noises, the bumps and scrapes, the horn (its button, at most every 0.1 s) and the bog. */
void UpdateCarSounds(ModelView *model)
{
    s32 original_slot=model->slot;
    s32 slot;
    tSoundState *screech;
    f32 pitch;
    f32 weighted,value;
    s32 i;
    s32 d1,d2,d3;
    f32 vol;
    if (!gGameSoundsEnabled) return;
    if (PC_ENGINE_PHASE(original_slot)) {
        PC_ENGINE_PHASE(original_slot)=0;
        return;
    }
    PC_ENGINE_PHASE(original_slot)=1;
    if (!PC_CAR_SOUNDS_ON(original_slot)) return;
    UpdateEngineSound((MODELDAT *)model);
    slot=model->slot;
    if (model->drone_type==DRONE_TYPE_HUMAN) {
        pitch=0.5f;
        for(i=0,vol=value=0.0f;i!=4;i++) {
            if(model->sviscode[i]) continue;
            weighted=model->skidIntensity[i]*sPitchRange[i];
            value=weighted-sPitchRange[i]/2.0f+1;
            if(pitch<value) pitch=value;
            value=weighted*0.6f;
            if(vol<value) vol=value;
        }
        screech=PC_SCREECH(slot);
        if(screech->sound==-1 && vol>0.0f) {
            if(model->drone_type==DRONE_TYPE_HUMAN) {
                screech->sound=AudPlayEffect(SFX_SCREECH,model->slot,2,2);
            } else {
                screech->sound=AudEmitEffect(MODEL_RWR(model),(s32)unitmatrix_origin,400.0f,0.0f,1.0f,0.0f,SFX_SCREECH,slot,0,130);
            }
        }
        if(screech->sound!=-1) {
            if(vol<=0.0f) {
                if(model->drone_type==DRONE_TYPE_HUMAN) AudStopEffect(screech->sound);
                else AudStopEmitter(screech->sound);
                InitSoundState(screech);
            } else if(model->drone_type==DRONE_TYPE_HUMAN) {
                if (vol!=PC_SCREECH_LEVEL(slot)) {
                    screech->vol=vol;
                    AudChangeEffectVol(screech->sound,vol);
                }
                if (pitch!=screech->pitch) {
                    screech->pitch=pitch;
                    AudChangeEffectPitch(screech->sound,pitch);
                }
            } else {
                if(pitch!=PC_SCREECH_STYLE(slot)) screech->pitch=pitch;
                else pitch=-2.0f;
                update_skid_emitter(screech,MODEL_RWR(model),vol,pitch);
            }
        }
    }
    if(model->drone_type!=DRONE_TYPE_HUMAN)return;
    UpdateRoadSounds(model);
    UpdateBumps(model);
    if(gSeconds<sLastHornTime[original_slot])sLastHornTime[original_slot]=0.0f;
    if(!(model->appearance&APPEARANCE_NO_HORN) &&
       (gPlayers[original_slot].edges&gPlayers[original_slot].hornControl) &&
       gSeconds-sLastHornTime[original_slot]>0.1f) {
        sLastHornTime[original_slot]=gSeconds;
        gHornSounds[original_slot]=AudPlayEffect(gCarHorn[original_slot + 1][model->body_type]+SFX_HORN_FIRST,original_slot,1,4);
    }
    if(model->bogState==1) {
        gBogSounds[original_slot]=AudPlayEffect(SFX_BOG,original_slot,2,1);
        model->bogState=2;
    } else if(model->bogState==3) {
        AudStopEffect(gBogSounds[original_slot]);
        gBogSounds[original_slot]=-1;
        model->bogState=0;
    }
}
