/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* AnimateNewBlastRing: per-frame update of a growing smoke/dust puff
 * attached to a scene node (name is a historical label). node->index selects
 * the car (model, 2056-byte records), the player position source
 * (gGameCar, 952-byte records) and the puff record (gBlastRingObjs, 60 bytes).
 *  - paused (gPauseState) -> nothing;
 *  - car disabled -> kill the puff's sound/effect handle (RemoveObjectTree
 *    (handle,0,0), handle = -1) and release the node (ReleaseEnviron);
 *  - update == 0 -> release the node;
 *  - otherwise the puff follows the player position; every 1/30 s (delay
 *    counts down by the volatile frame time gFrameSeconds) its scale matrix
 *    diagonal grows by 1.5; after phase 16 it is killed and released; from
 *    phase 7 its colour (255,255,255,alpha) is written to the resource record
 *    ObjInstanceList[id.index] and alpha fades by 24 while >= 25.
 * Shaping: every access is written gBlastRingObjs[node->index].field (no Effect
 * pointer local): uopt keeps the address as one expression web in v0 and
 * re-forms it after the alpha store (the shift-form x*60 on that edge).
 * `phase` is one variable reused for the resource index (v1). The frame needs
 * 12 bytes of unreferenced local storage declared first (here an unused
 * `Vec3 position`; any 12-byte aggregate gives the same frame). gFrameSeconds is volatile
 * (lui/addiu/lwc1 0(reg)). Own .rodata: 0.0333333f (0x3D088880 at 0x801239CC).
 * No arcade ancestor identified.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;
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
typedef struct Car {u8 opaque0[1600];s8 enabled;u8 opaque1601[455];} Car;
typedef struct Player {u8 opaque0[8];Vec3 position;u8 opaque20[0x110 - 0x14]; struct pc_visual visuals[21]; u8 opaque20_308[0x380 - 0x308]; void *input; u8 opaque20_384[0x3B8 - 0x384];} Player;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Resource { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 color; u8 opaque64[4]; } Resource;
typedef union Color {struct {u8 red,green,blue,alpha;} component;u32 word;} Color;
extern s32 gPauseState;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the blast rings are pc_blast_rings, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), seen here in this file's view. */
#define gBlastRingObjs (*(Effect (*)[PC_MAX_LINKS])PC_BLAST_RINGS)
extern Resource ObjInstanceList[];
extern volatile f32 gFrameSeconds;
extern void RemoveObjectTree(s16,s32,s32);
extern void ReleaseEnviron(Node *,s32);
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
