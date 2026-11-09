/* AnimateWingFlame.c (visuals): a car's wing flame each frame (MAT3 visuals.c, AnimateWingFlame()). */
#include "game/game.h"

/* A car visual (tVisual) on the PC: 0x20 bytes, next and func host pointers (docs/port/CARS.md 3). */
typedef struct tVisual {void *next;s16 index,objnum,slot;u16 pad;u32 data,timeStamp;intptr_t func;} tVisual;
/* A scene node's matrix, its rotation part. */
typedef struct tUVMatrix {f32 uvs[3][3];} tUVMatrix;
/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields AnimateWingFlame reads. */
typedef struct tCarData {u8 pad0[0xE8];u32 appearance;u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308];s8 camera,view,saveview,carMine;u8 pad360[0x380 - 0x360];tPadState *player;u8 pad384[0x3B8 - 0x384];} tCarData;
VIEW(tUVMatrix, unitmatrix);
#define unitmatrix unitmatrix_view

/* The flame of car v->slot's wing on side v->data (0 left, 1 right; VIS_WING_FLAMES + side), each frame: while the
 * wing is out (its visual's data bit 0x10, set by AnimateWing) the flame shows, stretched along its Y axis by the
 * stick: 8 times its x (the player's analog[0], negated for the left wing), 2 at least.
 * Not shown for a hulk or a car shown as a mine; drawn only in the car's own window's views 2 and up; it blinks
 * with the invisible cheat or a respawn. op 0 stops it: the flame is hidden. */
void AnimateWingFlame(tVisual *v,s16 op) {
    s32 hulk;
    s32 inv;
    tUVMatrix *mat;
    tCarData *car;
    f32 scale;
    s32 side;
    s16 slot;
    slot=v->slot;
    if(op==0) {
        if(v->objnum>=0) MBOX_HideObject(v->objnum,1,15);
        v->func=0;
        v->objnum=-1;
        return;
    }
    side=(v->data&1)?1:0;
    car=PC_CAR(slot);
    if(car->visuals[VIS_WINGS + side].data&0x10) {
        MBOX_ShowObject(v->objnum,0,15);
        mat=ZOID_FindMatrix(v->objnum);
        CopyUV(&unitmatrix,mat);
        scale=side ? 8.0f*car->player->analog[0] : -8.0f*car->player->analog[0];
        if(scale<2.0f) scale=2.0f;
        mat->uvs[1][1]=scale;
    } else MBOX_HideObject(v->objnum,1,15);
    hulk=(car->appearance&APPEAR_HULK)!=0;
    {
        inv=gInvisibleCar || (car->appearance&APPEAR_INVISIBLE);
        if(CheckVisibleAll(v,!hulk&&!car->carMine)) {
            CheckVisible(v,car->view>=2,car->camera);
            if(inv) ObjInstanceList[v->objnum].flags^=0x80000000U;
            else ObjInstanceList[v->objnum].flags&=0x7FFFFFFFU;
        }
    }
}
