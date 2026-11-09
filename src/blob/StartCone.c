#include "game/game.h"

typedef struct Motion {
    f32 f0, f4, f8;
    f32 vel[3];
} Motion;

/* PC: a target (a level-object node) keeps its N64 layout; its state, the cone's motion, is a heap record of its
 * category, held as a 32-bit window address (docs/port/POINTERS.md 10). */
enum n64p_Motion { n64p_Motion_ };
typedef struct Obj {
    u8 pad0[4];
    u8 flags;
    u8 pad5[7];
    s32 model;
    s16 type;
    u8 pad12[2];
    f32 pos[3];
    u8 pad20[56 - 32];
    u8 f56[12];
    u8 f68[12];
    s16 kind;
    u8 pad52[92 - 82];
    s8 car;
    u8 pad5D[108 - 93];
    enum n64p_Motion motion;
} Obj;

/* PC: a target info record (targetInfo) is 0x40 bytes: its name, file, start function and visual function are
 * pointers. Every field is spelled out, whichever view the loader converts the ROM's records by
 * (docs/port/POINTERS.md 10). */
typedef struct ObjType {
    char *name, *file;
    void *start;
    void *f12;
    s16 kind;
    u16 flags;
    s16 id;                 /* 0x14 */
    s8 category, element;
    f32 value;
    s32 f28;                /* 0x1C */
    s32 w20, w24, w28;
    f32 f2C;
} ObjType;

typedef struct Car {
    u8 pad0[20];
    f32 vel[3];
    u8 pad20[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad20_308[0x35F - 0x308];
    u8 f863;
    u8 pad360[0x380 - 0x360]; void *input; u8 pad360_384[0x3B8 - 0x384];
} Car;

typedef struct Lap {
    u8 pad0[1600];
    s8 f1600;
    u8 pad641[2056 - 1601];
} Lap;

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Node {
    struct Node *next;
    s16 f4;
    u8 pad6[6];
    u32 obj;
    f32 f16;
    void *f20;
} Node;

#define N64P(x) _Generic((x), enum n64p_Motion: (Motion *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

VIEW(ObjType[], targetInfo);
#define targetInfo targetInfo_view

Node *GrabEnvEntry(void);

static void Flag1600(Lap *lap)
{
    if (!lap->f1600) {
        lap->f1600 = 1;
    }
}

void StartCone(Obj *obj)
{
    Node *vis;
    f32 tmat[4][3];
    s32 u1;
    s32 u2;
    s32 u3;
    f32 *vel;
    Motion *angV;
    Car *car;
    s32 u4;
    s32 u5;
    ObjType *tInfo;

    tInfo = &targetInfo[obj->type];
    car = PC_CAR(obj->car);
    if (gCarMines && tInfo->id == 236) {
        car->f863 = 1;
        MBOX_HideObject(obj->model, 0, 15);
        obj->flags &= ~6;
        return;
    }
    if (gMineCones && tInfo->id == 236) {
        MBOX_HideObject(obj->model, 0, 15);
        StartBlast(obj->f68, 0, 0.4f, 1);
        Flag1600(PC_MODEL(obj->car));
        obj->flags &= ~6;
        return;
    }
    vis = GrabEnvEntry();
    if (vis == 0) {
        return;
    }
    vis->f4 = 0;
    vis->f20 = tInfo->f12;
    vis->obj = N64W(obj);
    vis->f16 = 5.0f;
    obj->flags &= ~6;
    vel = N64P(obj->motion)->vel;
    angV = N64P(obj->motion);
    vel[0] = car->vel[0] * 0.125f;
    vel[1] = car->vel[1] * 0.125f;
    vel[2] = car->vel[2] * 0.125f;
    LookInDir(vel, tmat);
    if (obj->kind == 339 || obj->kind == 237) {
        vel[1] += 1.0f;
        angV->f0 = 0.0f;
        angV->f4 = 12.0f;
        angV->f8 = 15.0f;
    } else {
        vel[1] += 2.0f;
        angV->f0 = 0.0f;
        angV->f4 = 12.0f;
        angV->f8 = 15.0f;
    }
    CopyUV(tmat, obj->pos);
    vis->next = gEnvList;
    gEnvList = vis;
    PlayTargetSound(targetInfo[obj->type].f28, obj->car, obj->f56, 2);
}
