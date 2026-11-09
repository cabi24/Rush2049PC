#include "game/game.h"

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Visual {
    struct Visual *next;    /* 0x00 */
    s16 index;              /* 0x04 */
    s16 objnum;             /* 0x06 */
    s16 slot;               /* 0x08 */
    u8 padA[2];
    u32 data;               /* 0x0C: a target's window address */
    f32 timeStamp;          /* 0x10 */
    void *func;             /* 0x14 */
} Visual;

typedef struct Target {
    u8 pad0[4];
    u8 flags;               /* 0x04 */
    u8 pad5[11];
    s16 type;               /* 0x10 */
    u8 pad12[2];
    f32 uv[3][3];           /* 0x14 */

    f32 soundPos[3];        /* 0x38 */
    u8 pad44[22];
    s16 state;              /* 0x5A */
    s8 slot;                /* 0x5C */
} Target;

typedef struct GameCar {
    u8 pad0[20];
    f32 RWV[3];             /* 0x14 */
    u8 pad20[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad20_308[0x380 - 0x308]; void *input; u8 pad20_384[0x3B8 - 0x384];
} GameCar;                  /* 0x3B8 */

typedef struct MATRIX {
    f32 uvs[3][3];
    f32 pos[3];
} MATRIX;

#define N64P(x) ((void *)PC_N64P(x))
#define N64W(x) PC_N64W(x)

VIEW(Visual *, gEnvList);
#define gEnvList gEnvList_view

Visual *GrabEnvEntry(void);

static void PointInDir(f32 *dir, MATRIX *m)
{
    LookInDir(dir, m->uvs);
}

void StartKnockdown(Target *t)
{
    Visual *v;
    MATRIX tmat;
    GameCar *car;

    if (!(v = GrabEnvEntry())) {
        return;
    }
    v->index = 0;
    v->func = targetInfo[t->type].visFunc;
    v->data = N64W(t);
    v->timeStamp = 0.0333333f;
    t->state = 4;
    t->flags &= ~6;
    car = PC_CAR(t->slot);
    PointInDir(car->RWV, &tmat);
    CopyUV(tmat.uvs, t->uv);
    v->next = gEnvList;
    gEnvList = v;
    PlayTargetSound(targetInfo[t->type].sound, t->slot, t->soundPos, 2);
}
