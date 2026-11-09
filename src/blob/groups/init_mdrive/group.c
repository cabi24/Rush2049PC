/* init_mdrive/group.c: a car's simulation started or restarted (MAT3 mdrive.c, Init_MDrive() and multiinit()).
 *
 * The race start (INIT_MODE_START for each car) and the model thread's respawn (INIT_MODE_CRASH, UpdateModel) call
 * Init_MDrive. */
#include "game/game.h"

/* A tire in the model record (MAT3 anon_tradius_springK_5c) seen from its sideforce field (+0x50), the 92-byte
 * stride kept. */
typedef struct TireForces { float sideforce, traction; u8 rest[84]; } TireForces;
/* The model record (tModelData), the fields Init_MDrive and multiinit read and write. */
typedef struct tModelData {
 u8 pad0[0x480];
 TireForces tires[4];           /* +0x480 tires[i].sideforce, .traction */
 u8 pad5F0[0x640 - 0x5F0];
 s8 crashflag;                  /* +0x640 */
 u8 pad641[0x6BC - 0x641];
 int resurrect_velocity;        /* +0x6BC resurrect.velocity: the respawn's speed, feet a second */
 u8 pad6C0[0x6D0 - 0x6C0];
 s16 polepos;                   /* +0x6D0 initin.polepos: the start place (PC_RESPAWN_BASE and up: a respawn) */
 u8 pad6D2[0x71C - 0x6D2];
 s16 fastin_modelrun;           /* +0x71C fastin.modelrun: the model thread steps this car */
 u8 pad71E[0x730 - 0x71E];
 u8 gear;                       /* +0x730 mainin.gear */
 u8 pad731[0x7C6 - 0x731];
 s16 slot;                      /* +0x7C6 */
 u8 pad7C8[0x7F8 - 0x7C8];
 float skidIntensity[4];        /* +0x7F8 each tire's skid sound level */
} tModelData;
/* The car record (tCarData), the fields Init_MDrive reads and writes. */
typedef struct tCarData {u8 pad0[4];float modelTime;u8 pad8[0x110 - 0x8]; struct pc_visual visuals[21]; u8 pad308[0x358 - 0x308];s8 crash_state,dead;u8 pad35A[10];float strayAngle;u8 tail[0x380 - 0x368]; void *player; u8 tail_384[0x3B8 - 0x384];} tCarData;

VIEW(volatile s16, modelrun);
#define modelrun modelrun_view

static void multiinit_n64(s16,s16);

/* Start car `slot`'s simulation (`mode` INIT_MODE_START) or respawn it (INIT_MODE_CRASH). In death mode a car that
 * crashed for good is out instead (CAR_DYING): its race ends at its time so far and it keeps coasting. Else an
 * aborted car is alive again, the car is placed and reset (multiinit_n64), a human's respawn pulls away in second
 * gear if it comes back at 30 mph (44 ft/s) or more, else first, a human's wrong-way angle is cleared, and the model
 * thread runs again. */
void Init_MDrive(s16 mode,s16 slot)
{
 tCarData *car=PC_CAR(slot);
 tModelData *m=PC_MODEL(slot);
 PC_LINK *link;
 if(gDeathMode && !car->dead && mode==INIT_MODE_CRASH) {
  car->dead=CAR_DYING;
  m->crashflag=0;
  CarReportsGameOver(slot,car->modelTime);m->fastin_modelrun=1;
 } else {
  if(car->dead<CAR_ALIVE)car->dead=CAR_ALIVE;
  m->fastin_modelrun=0;
  multiinit_n64(mode,slot);
  link=&PC_LINKS[slot];
  if(PC_LINKP_OWNER(link)==6) {   /* LINK_OWNER_HUMAN */
   if(mode!=INIT_MODE_START) {if(m->resurrect_velocity>=44.0f)
    m->gear=2;
   else
    m->gear=1;}
   car->strayAngle=0.0f;
  }
  modelrun=1;
 }
}

/* The common part (MAT3 multiinit): car `slot`'s start place, its grid place at the race start (0 for every car of a
 * ghost race) with its respawn point the start, or a respawn's place past the grid (PC_RESPAWN_BASE, plus the slot
 * but in a ghost race); the car set on it (Resurrect), no crash, no skid sound, no tire forces, and its engine
 * sounds started (in a ghost race only car 0's). */
static void multiinit_n64(s16 mode,s16 slot)
{
 tModelData *m=PC_MODEL(slot);
 int i;
 if(mode==INIT_MODE_START) {
  if(gRace==2) m->polepos=0;   /* kGhost */
  else m->polepos=PC_LINK_PLACE(slot);
  SetResurrectionData(INIT_MODE_START,m,0);
 } else {
  if(gRace==2) m->polepos=PC_RESPAWN_BASE;
  else m->polepos=slot+PC_RESPAWN_BASE;
 }
 Resurrect(m);
 PC_CAR_B358(slot)=CRASH_NONE;   /* crash_state */
 for(i=0;i<4;i++) {
  m->skidIntensity[i]=0.0f;
  m->tires[i].sideforce=0.0f;
  m->tires[i].traction=0.0f;
 }
 if(gRace!=2 || m->slot==0)StartCarSounds(m);
}
