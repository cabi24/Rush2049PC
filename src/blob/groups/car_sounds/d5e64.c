/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef struct Entity {
    u8 pad0[12]; s32 id; s32 state; u8 pad14[6];
    u8 msgCount; u8 pad1B[9]; f32 x,y,z,w; u8 pad34[16];
} Entity;
typedef struct Msg {
    s16 id; s8 type,used;
    union { struct { f32 x,y,z,w; } value; Entity *target; } payload;
    Entity *ent;
} Msg;
extern Entity *gFxTags;
extern s32 gTagMask;
extern Msg audiomgr_gCmds[128];
extern s32 audio_lock_queue, audio_command_queue;
s32 osRecvMesg(void *,void *,s32);
s32 osSendMesg(void *,void *,s32);
Entity *ConvertTag(s32 h);
Msg *GetCmd(void);
void AudChangeEffect(s32 h,f32 x,f32 y,f32 z,f32 w);
void AudChangeEffectVol(s32 h,f32 opacity);
void AudChangeEffectPitch(s32 h,f32 value);
void AudStopEffect(s32 h);

void change_effect_lr_pan(s32 h,f32 value);
void change_effect_fb_pan(s32 h,f32 value);


typedef struct { float a; s32 pad[3]; float b; s32 c; } Ent;      /* 24 bytes */
typedef struct { Ent e[5]; } Slot;                                 /* 120 bytes */

extern Slot gBump[];
extern s16 gScrapeState[];
extern s32 gScrapes[];
extern float gScrapeTime[];
extern float gBumpTime[];
extern float gThumpTime[];
extern void AudStopEffect(s32 h);

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


typedef unsigned int u32;
typedef struct State20 {s32 handle;float a,b,c,d;} State20;
/* PC: the engine sound table gEngineSpecs (ROM data) with the fields UpdateEngineSound reads (mode.c's EngTbl): the loader
 * swaps each element by this declaration, and as rest[28] the rev points and levels stayed big-endian, which made
 * every engine's level 0 (docs/port/AUDIO.md 9). */
typedef struct Definition32 {s32 object;unsigned short r0,r1,b0,b1,b2;u8 padE[2];f32 p0,p1,p2;u8 pad1C[4];} Definition32;
typedef struct Definitions64 {Definition32 item[2];} Definitions64;
typedef struct Player84 {Definitions64 *definition;State20 pair[3];State20 extra;} Player84;
typedef struct States60 {State20 values[3];} States60;
typedef struct Vehicle2056 {u8 prefix[12];s8 category;u8 gap13[1587];s8 disabled;u8 gap1601[389];s16 player;u8 gap1992[4];s8 mode;u8 tail[59];} Vehicle2056;
/* On the PC the cars' sound records are PC data, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 7). */
extern s8 gGameSoundsEnabled;
extern Definitions64 gEngineSpecs[];
extern States60 gRoadNoises[];
extern s32 gHornSounds[],gBogSounds[];
extern char unitmatrix_origin[];
extern u32 AudPlayEffectVP(u32,u32,u32,u8,float,float,float);
extern u32 AudEmitEffect(void *,void *,float,float,float,float,u32,u32,u32,u32);
void InitSoundState(State20 *state);
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

