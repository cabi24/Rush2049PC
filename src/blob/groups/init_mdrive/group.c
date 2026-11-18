/* init_mdrive/group.c: a car's simulation started or restarted (MAT3 mdrive.c, Init_MDrive() and multiinit()).
 *
 * The race start (INIT_MODE_START for each car) and the model thread's respawn (INIT_MODE_CRASH, UpdateModel) call
 * Init_MDrive. */
#include "game/game.h"


static void multiinit_n64(s16,s16);

/* Start car `slot`'s simulation (`mode` INIT_MODE_START) or respawn it (INIT_MODE_CRASH). In death mode a car that
 * crashed for good is out instead (CAR_DYING): its race ends at its time so far and it keeps coasting. Else an
 * aborted car is alive again, the car is placed and reset (multiinit_n64), a human's respawn pulls away in second
 * gear if it comes back at 30 mph (44 ft/s) or more, else first, a human's wrong-way angle is cleared, and the model
 * thread runs again. */
void Init_MDrive(s16 mode,s16 slot)
{
 tCarData *car=&pc_cars[slot];
 tModelData *m=&pc_models[slot];
 PC_LINK *link;
 if(gDeathMode && !car->dead && mode==INIT_MODE_CRASH) {
  car->dead=CAR_DYING;
  m->crashflag=0;
  CarReportsGameOver(slot,car->modelTime);m->fastin.modelrun=1;
 } else {
  if(car->dead<CAR_ALIVE)car->dead=CAR_ALIVE;
  m->fastin.modelrun=0;
  multiinit_n64(mode,slot);
  link=&PC_LINKS[slot];
  if(PC_LINKP_OWNER(link)==6) {   /* LINK_OWNER_HUMAN */
   if(mode!=INIT_MODE_START) {if(m->resurrect.velocity>=44.0f)
    m->mainin.gear=2;
   else
    m->mainin.gear=1;}
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
 tModelData *m=&pc_models[slot];
 int i;
 if(mode==INIT_MODE_START) {
  if(gRace==kGhost) m->initin.polepos=0;
  else m->initin.polepos=PC_LINK_PLACE(slot);
  SetResurrectionData(INIT_MODE_START,m,0);
 } else {
  if(gRace==kGhost) m->initin.polepos=PC_RESPAWN_BASE;
  else m->initin.polepos=slot+PC_RESPAWN_BASE;
 }
 Resurrect(m);
 pc_cars[slot].crash_state=CRASH_NONE;   /* crash_state */
 for(i=0;i<4;i++) {
  m->skidIntensity[i]=0.0f;
  m->tires[i].sideforce=0.0f;
  m->tires[i].traction=0.0f;
 }
 if(gRace!=kGhost || m->slot==0)StartCarSounds(m);
}
