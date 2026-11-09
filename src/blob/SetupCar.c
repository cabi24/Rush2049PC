#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct Model952 {u8 prefix[232];u32 flags;u8 gap236[8];int model;u8 gap248[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 gap248_308[0x35D - 0x308];s8 selection;u8 tail[0x380 - 0x35E]; void *input; u8 tail_384[0x3B8 - 0x384];} Model952;

typedef struct Slot64 {int root;u8 gap4[12];int body;u8 gap20[8];int wheels[4],extras[5];} Slot64;

typedef struct OSThread OSThread;
VIEW(Slot64[], gCarParts);
#define gCarParts gCarParts_view

VIEW(OSThread, model_thread);
#define model_thread model_thread_view

/* The wheel textures (InitVisuals): the rims from 3, by rim type. */
typedef struct TexDef TexDef;
VIEW(TexDef *[24], gWheelTextures);
#define gWheelTextures gWheelTextures_view
extern void osStopThread(OSThread *);
extern void osStartThread(OSThread *);

extern void ZOID_SetTexture(s16,void *,int);

void SetupCar(s16 player,u8 mode,int apply)
{
 Model952 *model;
 Slot64 *slot;
 int mask;
 u16 color;
 s16 i;
 if(apply && mode!=13) {
  if(PC_CAR_FLAGS(player)&0x10) {
   osStopThread(&model_thread);
   if(PC_LINK_OWNER(player)!=6 || gDriveHulk[player]==0)mask=~0x10;
   else if(gDriveHulk[player]==2)mask=~0x60;
   else mask=-1;
   PC_MODEL_FLAGS(player)&=mask;
   PC_CAR_FLAGS(player)&=mask;
   osStartThread(&model_thread);
  }
 }
 model=PC_CAR(player);
 if(model){}
 color=PickCar(player,mode);
 slot=&gCarParts[player];
 ZOID_SetObjectDef(slot->root,color);
 model->model=slot->root;
 ZOID_SetObjectDef(slot->body,gObjList[(s16)(player*3) + 1]);
 for(i=0;i<4;i++)ZOID_SetObjectDef(gCarParts[player].wheels[i],gObjList[156]);
 if(expansion_pak_present || gTrackNum>=6) {
  ZOID_SetTexture((s16)slot->wheels[0],gWheelTextures[PC_MODEL_B00F(player) + 3],-1);
 } else {
  ZOID_SetTexture((s16)slot->wheels[0],gWheelTextures[16],-1);
 }
 MBOX_ShowObject(slot->root,1,15);
 if(model->selection!=1)MBOX_HideObject(slot->body,1,15);
 if(apply) {
  MBOX_HideObject(slot->extras[0],1,15);
  MBOX_HideObject(slot->extras[1],1,15);
  MBOX_HideObject(slot->extras[2],1,15);
  MBOX_HideObject(slot->extras[3],1,15);
  MBOX_HideObject(slot->extras[4],1,15);
  if(mode==13 || model->selection<2) {
   for(i=0;i<4;i++)MBOX_HideObject(gCarParts[player].wheels[i],1,15);
   if(mode==13) {
    MBOX_HideObject(slot->body,1,15);
    osStopThread(&model_thread);
    PC_MODEL_FLAGS(player)|=0x10;
    model->flags|=0x10;
    osStartThread(&model_thread);
   }
  }
 }
}
