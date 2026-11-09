#include "game/game.h"

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct ModelObj {void *opaque0;u8 opaque4[2];s16 model,player;u16 opaque10;u32 flags,opaque16;intptr_t active;} ModelObj;
typedef struct Pose {f32 values[9];} Pose;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Model68 { u32 flags, opaque4; Pose *pose; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Model68;
/* PC: side[i] starts at the data word of car visual 16 + i (0x20 bytes on the PC); its func and the next
 * visual's next pointer are host pointers. */
typedef struct Side24 {u32 flags;u8 opaque4[4];void *func,*next;u8 opaque24[8];} Side24;
typedef struct Player952 {u8 opaque0[232];u32 flags;u8 opaque236[0x110 - 0xEC]; struct pc_visual visuals[16]; void *opaque236_290; u8 opaque236_294[0x29C - 0x294];Side24 side[2];u8 opaque716[0x2D4 - 0x2CC]; void *opaque716_2D4; struct pc_visual opaque716_2D8[2]; u8 opaque716_308[0x35C - 0x308];s8 shift,quality,opaque862,disabled;u8 opaque864[32];Pose *pose;u8 opaque900[52];} Player952;
VIEW(Model68[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(Pose, unitmatrix);
#define unitmatrix unitmatrix_view

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
