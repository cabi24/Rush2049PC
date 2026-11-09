#include "game/game.h"

typedef struct Phys {
    f32 W[3];
    f32 V[3];
} Phys;
/* PC: a target (a level-object node) keeps its N64 layout; its state, the debris' physics, is a heap record of its
 * category, held as a 32-bit window address (docs/port/POINTERS.md 10). */
enum n64p_Phys { n64p_Phys_ };
enum n64p_Model { n64p_Model_ };
typedef struct Model {
    u8 pad0[0xC];
    s32 id;                 /* the target's handle, whose low half is the model id (N64 +0xE): +0xC here */
    s16 type;
    u8 pad12[2];
    f32 UV[3][3];
    f32 RWR[3];
    u8 pad44[0x6C - 0x44];
    enum n64p_Phys phys;
} Model;
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Obj {
    void *next;
    u8 pad4[0xC - 4];
    enum n64p_Model model;
    f32 timer;
    void (*fn)();
} Obj;
#define N64P(x) _Generic((x), enum n64p_Phys: (Phys *)PC_N64P(x), enum n64p_Model: (Model *)PC_N64P(x), \
    default: (x))
#define N64W(x) PC_N64W(x)
/* PC: a target info record (targetInfo) is 0x40 bytes: its name, file, start function and visual function are
 * pointers. Every field is spelled out, whichever view the loader converts the ROM's records by
 * (docs/port/POINTERS.md 10). */
typedef struct Kind {
    char *name, *file;
    void *start, *visual;
    s16 kind;
    u16 flags;              /* 0x12 */
    s16 shape;
    s8 category, element;
    f32 value;
    s32 sound, w20, w24, w28;
    f32 f2C;
} Kind;

VIEW(f32[3], velrot_gravity);
#define velrot_gravity velrot_gravity_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(Kind[], targetInfo);
#define targetInfo targetInfo_view
VIEW(u8[], gTargetList);
#define gTargetList gTargetList_view

static void debris_remove(Obj *obj, Model *m, Kind *k) {
    if (k->flags & 0x2000) {
        RemoveObjectTree(m->id, 0, 0);
    }
    FreeStatLLE(gTargetList, m);
    ReleaseEnviron(obj, 1);
}

void AnimateVelRotGeneric(Obj *obj, s16 mode) {
    Phys *p;
    Model *m;
    f32 a[6];
    f32 temp[3];
    f32 *v;
    s16 i;

    if (mode == 0) {
        ReleaseEnviron(obj, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    m = N64P(obj->model);
    p = N64P(m->phys);
    v = p->V;
    for (i = 0; i < 3; i++) {
        v[i] += velrot_gravity[i] * 0.15f;
        m->RWR[i] += v[i] + velrot_gravity[i] * 0.15f;
    }
    temp[0] = p->W[0] * gFrameSeconds;
    temp[1] = p->W[1] * gFrameSeconds;
    temp[2] = p->W[2] * gFrameSeconds;
    RotateUV(temp, m->UV);
    obj->timer -= gFrameSeconds;
    if (obj->timer <= 0.0f) {
        debris_remove(obj, m, &targetInfo[m->type]);
    }
}
