#include "game/game.h"

typedef struct Tire92 { float a, b; u8 rest[84]; } Tire92;
typedef struct Vehicle2056 {
 u8 gap0[1152];
 Tire92 tires[4];
 u8 gap1520[80];
 s8 state;
 u8 gap1601[123];
 int speed;
 u8 gap1728[16];
 s16 camera;
 u8 gap1746[74];
 s16 selected;
 u8 gap1822[18];
 u8 mode;
 u8 gap1841[149];
 s16 index;
 u8 gap1992[48];
 float wheels[4];
} Vehicle2056;
typedef struct Model952 {u8 prefix[4];float setup;u8 gap8[0x110 - 0x8]; struct pc_visual visuals[21]; u8 gap8_308[0x358 - 0x308];s8 clear,active;u8 gap858[10];float drift;u8 tail[0x380 - 0x368]; void *input; u8 tail_384[0x3B8 - 0x384];} Model952;

VIEW(volatile s16, modelrun);
#define modelrun modelrun_view

static void multiinit_n64(s16,s16);

void Init_MDrive(s16 action,s16 index)
{
 Model952 *model=PC_CAR(index);
 Vehicle2056 *vehicle=PC_MODEL(index);
 PC_LINK *config;
 if(gDeathMode && !model->active && action==2) {
  model->active=1;
  vehicle->state=0;
  CarReportsGameOver(index,model->setup);vehicle->selected=1;
 } else {
  if(model->active<0)model->active=0;
  vehicle->selected=0;
  multiinit_n64(action,index);
  config=&PC_LINKS[index];
  if(PC_LINKP_OWNER(config)==6) {
   if(action!=0) {if(vehicle->speed>=44.0f)
    vehicle->mode=2;
   else
    vehicle->mode=1;}
   model->drift=0.0f;
  }
  modelrun=1;
 }
}

static void multiinit_n64(s16 action,s16 index)
{
 Vehicle2056 *vehicle=PC_MODEL(index);
 int i;
 if(action==0) {
  if(gRace==2) vehicle->camera=0;
  else vehicle->camera=PC_LINK_PLACE(index);
  SetResurrectionData(0,vehicle,0);
 } else {
  if(gRace==2) vehicle->camera=PC_RESPAWN_BASE;
  else vehicle->camera=index+PC_RESPAWN_BASE;
 }
 Resurrect(vehicle);
 PC_CAR_B358(index)=0;
 for(i=0;i<4;i++) {
  vehicle->wheels[i]=0.0f;
  vehicle->tires[i].a=0.0f;
  vehicle->tires[i].b=0.0f;
 }
 if(gRace!=2 || vehicle->index==0)StartCarSounds(vehicle);
}
