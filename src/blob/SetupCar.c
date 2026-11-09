/* SetupCar.c: show a car with a body (MAT3 visuals.c, SetupCar()).
 *
 * Gives the car's frame the definition of its body (the car itself, or the burnt-out wreck after a crash), shows
 * or hides the hood, tires and sparks for the view, and sets the hulk flag. */
#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields SetupCar uses. */
typedef struct tCarData {
    u8 pad000[0xE8];
    u32 appearance;             /* +0x0E8 APPEAR_* */
    u8 pad0EC[0xF4 - 0xEC];
    int objnum;                 /* +0x0F4 the frame's scene object */
    u8 pad0F8[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 pad308[0x35D - 0x308];
    s8 view;                    /* +0x35D the camera view: 1 shows the hood, 0 and 1 no tires */
    u8 pad35E[0x380 - 0x35E]; void *player; u8 pad384[0x3B8 - 0x384];
} tCarData;

typedef struct OSThread OSThread;

extern void osStopThread(OSThread *);
extern void osStartThread(OSThread *);

/* Show car `slot` with body `type` (13: the burnt-out wreck). In a race (`for_game`), a car that was a hulk and now
 * gets a body back loses the hulk flag (APPEAR_HULK), unless a human drives it with the "drive the hulk" cheat
 * (gDriveHulk 1 keeps the flag and the smoke, 2 keeps the flag but not the smoke). The frame takes the body's
 * definition (PickCar), the hood its own (shown only in view 1), the tires theirs and the first tire the
 * rim texture (see CreateCar). In a race the sparks start hidden, and the tires are hidden for the wreck and in the
 * views 0 and 1; the wreck hides its hood too and sets the hulk flag. The model thread is stopped while the model's
 * appearance changes, since it writes the same word. */
void SetupCar(s16 slot, u8 type, int for_game)
{
 tCarData *car;
 CarParts *parts;
 int mask;
 u16 frame_def;
 s16 i;
 if(for_game && type!=13) {
  if(PC_CAR_FLAGS(slot)&APPEAR_HULK) {
   osStopThread(&model_thread);
   if(PC_LINK_OWNER(slot)!=6 || gDriveHulk[slot]==0)mask=~APPEAR_HULK;   /* owner 6: LINK_OWNER_HUMAN */
   else if(gDriveHulk[slot]==2)mask=~APPEAR_WRECK_SMOKE;
   else mask=-1;
   PC_MODEL_FLAGS(slot)&=mask;
   PC_CAR_FLAGS(slot)&=mask;
   osStartThread(&model_thread);
  }
 }
 car=PC_CAR(slot);
 frame_def=PickCar(slot,type);
 parts=&gCarParts[slot];
 ZOID_SetObjectDef(parts->frame,frame_def);
 car->objnum=parts->frame;
 ZOID_SetObjectDef(parts->hood,gObjList[(s16)(slot*3) + 1]);
 for(i=0;i<4;i++)ZOID_SetObjectDef(gCarParts[slot].tires[i],gObjList[OBJLIST_TIRE]);
 if(expansion_pak_present || gTrackNum>=6) {
  ZOID_SetTexture((s16)parts->tires[0],gWheelTextures[PC_MODEL_B00F(slot) + 3],-1);
 } else {
  ZOID_SetTexture((s16)parts->tires[0],gWheelTextures[16],-1);
 }
 MBOX_ShowObject(parts->frame,1,15);
 if(car->view!=1)MBOX_HideObject(parts->hood,1,15);
 if(for_game) {
  MBOX_HideObject(parts->lfSpark,1,15);
  MBOX_HideObject(parts->lrSpark,1,15);
  MBOX_HideObject(parts->rfSpark,1,15);
  MBOX_HideObject(parts->rrSpark,1,15);
  MBOX_HideObject(parts->bSpark,1,15);
  if(type==13 || car->view<2) {
   for(i=0;i<4;i++)MBOX_HideObject(gCarParts[slot].tires[i],1,15);
   if(type==13) {
    MBOX_HideObject(parts->hood,1,15);
    osStopThread(&model_thread);
    PC_MODEL_FLAGS(slot)|=APPEAR_HULK;
    car->appearance|=APPEAR_HULK;
    osStartThread(&model_thread);
   }
  }
 }
}
