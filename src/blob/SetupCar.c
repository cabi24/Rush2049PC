/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SetupCar @ 0x800B200C, 980 bytes (from 31/245 best.c). Names are historical labels.
 * SetupCar(player, mode, apply): car model/resource refresh for one player. When apply is set and
 * mode != 13 and the car's model is visible (flags & 0x10), under the model_thread thread stop/start
 * (osPfsChecker_full / osStartThread labels) clear the hulk/visibility bits of the vehicle record
 * (D_8014A250, 0x808 bytes) and the model (gGameCar, 952 bytes) with a mask picked from the slot mode
 * (D_80153E88[player].mode) and gDriveHulk[player]. Then set the object (+20, u16) of the car's root, body
 * and four wheel resources (ObjInstanceList, 68 bytes) from PickCar(player, mode), gObjList_hood[player*3]
 * and gObjList_tire; load the wheel model set (vehicle type, or 13 in attract/when gTrackNum >= 6); show the
 * root, the body unless selection == 1, and (apply) the extras and wheels; mode 13 also restores the
 * visibility bits.
 * Shaping (measured in the whole-program unit with --block PickCar):
 * - resource_set_object is the inlined static setter found (also in AnimateCoin): each inlined
 *    call adds 8 bytes of named locals;
 *  - declaration order model, slot, mask, color, i: the two 2-byte locals (u16 color, s16 i) are adjacent,
 *    so the named area is 40, not 48 (the 8-byte "frame 96 vs 88" residual was alignment padding);
 *  - the first block uses gGameCar[player] directly (no `model` there): the spilled address is then a
 *    uopt temp whose base ugen knows, so as1 may schedule the model load above the vehicle store;
 * - if (model) {} after the second model assignment (compiled-out check) ends the block;
 *  - the expansion_pak_present if/else on separate lines from its calls (as1 tie-break by .loc: lui of gTrackNum
 *    before the branch, the 2056 multiply's addu in the delay slot).
 */
typedef unsigned char u8;typedef signed char s8;typedef short s16;typedef unsigned short u16;typedef unsigned int u32;
typedef struct Model952 {u8 prefix[232];u32 flags;u8 gap236[8];int model;u8 gap248[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 gap248_308[0x35D - 0x308];s8 selection;u8 tail[0x380 - 0x35E]; void *input; u8 tail_384[0x3B8 - 0x384];} Model952;
typedef struct Vehicle2056 {u8 prefix[15];s8 type;u8 gap16[1988];u32 flags;u8 tail[48];} Vehicle2056;
typedef struct Slot64 {int root;u8 gap4[12];int body;u8 gap20[8];int wheels[4],extras[5];} Slot64;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Resource68 { u32 flags, w4; void *obj; float scale, w; u16 object; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Resource68;
typedef struct Config8 {u8 prefix[7];u8 mode;} Config8;
typedef struct OSThread OSThread;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Slot64 gCarParts[];
extern Resource68 ObjInstanceList[];
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern OSThread model_thread;
extern s8 gDriveHulk[],expansion_pak_present,gTrackNum;
extern u16 gObjList_hood[],gObjList_tire;
extern void *gWheelTextures_rims[];
extern void osStopThread(OSThread *);
extern void osStartThread(OSThread *);
extern u16 PickCar(s16,u8);
extern void ZOID_SetTexture(s16,void *,int);
extern void MBOX_ShowObject(int,int,int);
extern void MBOX_HideObject(int,int,int);
void ZOID_SetObjectDef(s16 index, u16 object);

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
 ZOID_SetObjectDef(slot->body,gObjList_hood[(s16)(player*3)]);
 for(i=0;i<4;i++)ZOID_SetObjectDef(gCarParts[player].wheels[i],gObjList_tire);
 if(expansion_pak_present || gTrackNum>=6) {
  ZOID_SetTexture((s16)slot->wheels[0],gWheelTextures_rims[PC_MODEL_B00F(player)],-1);
 } else {
  ZOID_SetTexture((s16)slot->wheels[0],gWheelTextures_rims[13],-1);
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
