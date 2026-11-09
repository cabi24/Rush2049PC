/* AnimateNewBlastRing.c: widen and fade a blast's shock ring (MAT3 visuals.c, AnimateNewBlastRing()).
 *
 * StartNewBlastRing (groups/blast) starts the ring around an exploding car; this, its visual's animator, runs once a
 * frame until the ring is gone. The ring's state is the car's pc_blast_rings entry. */
#include "game/game.h"

/* A visual (MAT3 tVisual). PC: 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct tVisual {struct tVisual *next;u16 index;s16 objnum,slot;u8 pad0E[10];intptr_t func;} tVisual;
/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first
 * on a little-endian host (docs/port/ENDIAN.md). */
typedef union Id {s32 handle;struct {s16 index,high;} part;} Id;
typedef struct Vec3 {f32 x,y,z;} Vec3;
/* A car's shock ring (MAT3 tBlastRingStruct; PC: struct pc_blast_ring). */
typedef struct tBlastRingStruct {
    Id ringIdx;             /* its scene object, -1 none */
    f32 uvs[3][3];          /* its matrix: its scene node points here */
    Vec3 pos;
    u8 ralpha,ralphaCnt,unused[2];  /* its alpha; the steps it has taken */
    f32 incTime;            /* seconds to its next step */
} tBlastRingStruct;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInstance { u32 flags, culling; void *mat; f32 scale, w; u16 index; s16 child, next, numLights; void *lights, *texture[4], *tile, *tileSize, *palette; u32 color; u8 envcolor[4]; } ObjInstance;
typedef union tRGBA {struct {u8 red,green,blue,alpha;} c;u32 rgba;} tRGBA;

/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the blast rings are pc_blast_rings, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), seen here in this file's view. */
#define gBlastRingObjs (*(tBlastRingStruct (*)[PC_MAX_LINKS])PC_BLAST_RINGS)
VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* The ring's animator (a visual's func): `op` 0 removes it. While its car is crashed the ring stays on the car
 * and, every 1/30 s, its scale grows by 1.5 (from 1); from its 8th step it fades (alpha down 24 a step) and after
 * its 16th it is removed. It ends at once when the car is no longer crashed. Nothing moves while the game is
 * paused. */
void AnimateNewBlastRing(tVisual *v,s16 op) {
    Vec3 pos;           /* unused: kept, as it shapes the function's stack frame */
    s32 phase;          /* the step count, then (below) the ring's scene node */
    tRGBA clr;
    if(gPauseState) return;
    if(!PC_MODEL_CRASHFLAG(v->slot)) {
        if(gBlastRingObjs[v->slot].ringIdx.handle!=-1) {
            RemoveObjectTree(gBlastRingObjs[v->slot].ringIdx.handle,0,0);
            gBlastRingObjs[v->slot].ringIdx.handle=-1;
        }
        goto release;
    }
    if(!op) {
release:
        ReleaseEnviron(v,1);
        return;
    }
    gBlastRingObjs[v->slot].pos.x=PC_CAR_POS_X(v->slot);
    gBlastRingObjs[v->slot].pos.y=PC_CAR_POS_Y(v->slot);
    gBlastRingObjs[v->slot].pos.z=PC_CAR_POS_Z(v->slot);
    gBlastRingObjs[v->slot].incTime-=gFrameSeconds;
    if(gBlastRingObjs[v->slot].incTime<=0.0f) {
        gBlastRingObjs[v->slot].uvs[0][0]+=1.5f;
        gBlastRingObjs[v->slot].uvs[1][1]+=1.5f;
        gBlastRingObjs[v->slot].uvs[2][2]+=1.5f;
        phase=gBlastRingObjs[v->slot].ralphaCnt;
        if(phase>=16) {
            if(gBlastRingObjs[v->slot].ringIdx.handle!=-1) {
                RemoveObjectTree(gBlastRingObjs[v->slot].ringIdx.handle,0,0);
                gBlastRingObjs[v->slot].ringIdx.handle=-1;
            }
            goto release;
        }
        if(phase>=7) {
            /* ZOID_SetColor, inlined */
            clr.c.red=255;
            clr.c.green=255;
            clr.c.blue=255;
            clr.c.alpha=gBlastRingObjs[v->slot].ralpha;
            phase=gBlastRingObjs[v->slot].ringIdx.part.index;
            ObjInstanceList[phase].color=clr.rgba;
            if(gBlastRingObjs[v->slot].ralpha>=25) {
                gBlastRingObjs[v->slot].ralpha-=24;
            }
        }
        gBlastRingObjs[v->slot].ralphaCnt++;
        gBlastRingObjs[v->slot].incTime=0.0333333f;
    }
    return;
}
