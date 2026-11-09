/* AnimateObjSwapGeneric: a track object that cycles through models, a visual's function (MAT3 visuals.c). */
#include "game/game.h"

#define TARGET_REMOVE_AT_END 0x1000   /* targetInfo flags: the cycle runs once, then the visual ends */
#define TARGET_DESTROY_AT_END 0x2000  /* the cycle runs once, then the object goes */
#define SWAP_FRAME_TIME 0.0625f       /* seconds a frame */

typedef struct Model {
    /* PC: id is the low half of the target's handle (an s32 at +0xC), at +0xC on a little-endian host, not +0xE:
     * the view reads the word (docs/port/POINTERS.md 10). */
    u8 pad0[0xC];
    s32 id;
    s16 type;
    u8 pad12[0x50 - 0x12];
    s16 cur;
    u8 pad52[0x58 - 0x52];
    s16 base;
    s16 count;
} Model;
/* PC: obj is a visual (tVisual, 0x20 bytes: next and func are host pointers), and its data is the target, a
 * level-object node, as a 32-bit window address (docs/port/CARS.md, POINTERS.md 10). */
typedef struct Obj {
    void *next;
    s16 frame;
    u8 pad6[6];
    unsigned int model;
    f32 timer;
} Obj;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Ent { unsigned int flags, w4; void *obj; f32 scale, w; u16 tex; s16 child, next, mode; void *light, *texture[4], *rect, *clip, *pal; unsigned int prim, env; } Ent;
/* PC: a target info record (targetInfo) is 0x40 bytes: its name, file, start function and visual function are
 * pointers. Every field is spelled out, whichever view the loader converts the ROM's records by
 * (docs/port/POINTERS.md 10). */
typedef struct Kind {
    char *name, *file;
    void *start, *visual;
    s16 kind;
    u16 flags;              /* 0x12 */
    s16 shape;
    signed char category, element;
    f32 value;
    s32 sound, w20, w24, w28;
    f32 f2C;
} Kind;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(Kind[], targetInfo);
#define targetInfo targetInfo_view

VIEW(Ent[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Every SWAP_FRAME_TIME seconds (not while paused) shows the next of the target's models (base .. base + count - 1
 * of gObjList) on its scene object; at the end of the cycle starts again, or ends the visual (and with
 * TARGET_DESTROY_AT_END removes the object). mode 0 ends the visual at once. */
void AnimateObjSwapGeneric(Obj *obj, s16 mode)
{
    Model *m;
    Kind *kind;
    s16 f;
    s16 id;
    u16 tex;

    if (mode == 0) {
apply:
        ReleaseEnviron(obj, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    m = ((Model *)PC_N64P(obj->model));
    obj->timer -= gFrameSeconds;
    if (obj->timer > 0.0f) {
        return;
    }
    obj->frame++;
    obj->timer = SWAP_FRAME_TIME;
    if (obj->frame >= m->count) {
        kind = &targetInfo[m->type];
        if (kind->flags & TARGET_REMOVE_AT_END) {
            goto apply;
        }
        if (kind->flags & TARGET_DESTROY_AT_END) {
            RemoveObjectTree(m->id, 0, 0);
            goto apply;
        }
        obj->frame = 0;
    }
    f = m->base + obj->frame;
    if (f != m->cur) {
        if (gObjList[f]) {}
        id = m->id;
        tex = gObjList[f];
        ObjInstanceList[id].tex = tex;
        m->cur = f;
    }
}
