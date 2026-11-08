/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Xcpluscomm -Wab,-r4300_mul */
/* N64 game: AnimateWingFlame, 0x80091874, 636 bytes.
 * Arcade/MAT3 ancestor: visual.c AnimateWingFlame. Adaptation tier.
 * Shows a car wing flame, scales its matrix, and applies visibility/translucency.
 * The genuine ZOID_FindMatrix boundary is essential to IDO's inline allocation.
 * N64 uses a separate side index and appearance hulk, and a pre-mode slot read.
 * All seven locals are used. No padding, dead guards, or forced registers.
 * The complete genuine group is required for private visibility-call registers.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16;
typedef unsigned short u16; typedef int s32; typedef unsigned int u32; typedef float f32;
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct ModelObj {void *opaque0;u8 opaque4[2];s16 model,player;u16 opaque10;u32 flags,opaque16;intptr_t active;} ModelObj;
typedef struct Pose {f32 values[9];} Pose;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Model68 { u32 flags, opaque4; Pose *pose; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Model68;
/* PC: side[i] starts at the data word of car visual 16 + i (0x20 bytes on the PC); its func and the next
 * visual's next pointer are host pointers. */
typedef struct Side24 {u32 flags;u8 opaque4[4];void *func,*next;u8 opaque24[8];} Side24;
typedef struct Player952 {u8 opaque0[232];u32 flags;u8 opaque236[0x110 - 0xEC]; struct pc_visual visuals[16]; void *opaque236_290; u8 opaque236_294[0x29C - 0x294];Side24 side[2];u8 opaque716[0x2D4 - 0x2CC]; void *opaque716_2D4; struct pc_visual opaque716_2D8[2]; u8 opaque716_308[0x35C - 0x308];s8 shift,quality,opaque862,disabled;u8 opaque864[32];Pose *pose;u8 opaque900[52];} Player952;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Model68 ObjInstanceList[];
extern Pose unitmatrix;
extern s8 gInvisibleCar;
void MBOX_HideObject(s16,s32,s32);
void MBOX_ShowObject(s16,s32,s32);
void CopyUV(void *,void *);
s32 CheckVisibleAll(ModelObj *,s32);
s32 CheckVisible(ModelObj *,s32,s32);
Pose *ZOID_FindMatrix(s16 objnum);
/* MAT3 helper/body evidence with N64 separate side-index lifetime and
 * trace-supported native homes for the used inv and pose locals. */
void AnimateWingFlame(ModelObj *record,s16 mode) {
    s32 hulk;
    s32 inv;
    Pose *pose;
    Player952 *player;
    f32 scale;
    s32 index;
    s16 slot;
    slot=record->player;
    if(mode==0) {
        if(record->model>=0) MBOX_HideObject(record->model,1,15);
        record->active=0;
        record->model=-1;
        return;
    }
    index=(record->flags&1)?1:0;
    player=PC_CAR(slot);
    if(player->side[index].flags&0x10) {
        MBOX_ShowObject(record->model,0,15);
        pose=ZOID_FindMatrix(record->model);
        CopyUV(&unitmatrix,pose);
        scale=index ? 8.0f*player->pose->values[4] : -8.0f*player->pose->values[4];
        if(scale<2.0f) scale=2.0f;
        pose->values[4]=scale;
    } else MBOX_HideObject(record->model,1,15);
    hulk=(player->flags&0x10)!=0;
    {
        inv=gInvisibleCar || (player->flags&8);
        if(CheckVisibleAll(record,!hulk&&!player->disabled)) {
            CheckVisible(record,player->quality>=2,player->shift);
            if(inv) ObjInstanceList[record->model].flags^=0x80000000U;
            else ObjInstanceList[record->model].flags&=0x7FFFFFFFU;
        }
    }
}
