#include "game/game.h"

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Node {struct Node *next;u16 tag;s16 handle,index;u8 opaque10[10];intptr_t state;} Node;
/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first
 * on a little-endian host (docs/port/ENDIAN.md). */
typedef union Id {s32 handle;struct {s16 index,high;} part;} Id;
typedef struct Vec3 {f32 x,y,z;} Vec3;
typedef struct Effect {
    Id id;
    f32 matrix[3][3];
    Vec3 position;
    u8 alpha,phase,opaque54[2];
    f32 delay;
} Effect;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Resource { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 color; u8 opaque64[4]; } Resource;
typedef union Color {struct {u8 red,green,blue,alpha;} component;u32 word;} Color;

/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the blast rings are pc_blast_rings, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), seen here in this file's view. */
#define gBlastRingObjs (*(Effect (*)[PC_MAX_LINKS])PC_BLAST_RINGS)
VIEW(Resource[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

void AnimateNewBlastRing(Node *node,s16 update) {
    Vec3 position;
    s32 phase;
    Color color;
    if(gPauseState) return;
    if(!PC_MODEL_CRASHFLAG(node->index)) {
        if(gBlastRingObjs[node->index].id.handle!=-1) {
            RemoveObjectTree(gBlastRingObjs[node->index].id.handle,0,0);
            gBlastRingObjs[node->index].id.handle=-1;
        }
        goto release;
    }
    if(!update) {
release:
        ReleaseEnviron(node,1);
        return;
    }
    gBlastRingObjs[node->index].position.x=PC_CAR_POS_X(node->index);
    gBlastRingObjs[node->index].position.y=PC_CAR_POS_Y(node->index);
    gBlastRingObjs[node->index].position.z=PC_CAR_POS_Z(node->index);
    gBlastRingObjs[node->index].delay-=gFrameSeconds;
    if(gBlastRingObjs[node->index].delay<=0.0f) {
        gBlastRingObjs[node->index].matrix[0][0]+=1.5f;
        gBlastRingObjs[node->index].matrix[1][1]+=1.5f;
        gBlastRingObjs[node->index].matrix[2][2]+=1.5f;
        phase=gBlastRingObjs[node->index].phase;
        if(phase>=16) {
            if(gBlastRingObjs[node->index].id.handle!=-1) {
                RemoveObjectTree(gBlastRingObjs[node->index].id.handle,0,0);
                gBlastRingObjs[node->index].id.handle=-1;
            }
            goto release;
        }
        if(phase>=7) {
            color.component.red=255;
            color.component.green=255;
            color.component.blue=255;
            color.component.alpha=gBlastRingObjs[node->index].alpha;
            phase=gBlastRingObjs[node->index].id.part.index;
            ObjInstanceList[phase].color=color.word;
            if(gBlastRingObjs[node->index].alpha>=25) {
                gBlastRingObjs[node->index].alpha-=24;
            }
        }
        gBlastRingObjs[node->index].phase++;
        gBlastRingObjs[node->index].delay=0.0333333f;
    }
    return;
}
