/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * StartKnockdown: N64 StartKnockdown (arcade game/targets.c StartKnockdown), reached through
 * the target type table (no direct callers). Same family as StartWindow.
 *   v = GrabEnvEntry() (GrabEnvEntry) or return; v->index = 0;
 *   v->func = gTargInfo[t->type].VisFunc; v->data = t; v->timeStamp = 0.0333333f;
 *   t->state = 4; t->flags &= ~6 (t->active = false);
 *   point the target along the hitting car's velocity: PointInDir(car->RWV, &tmat) then copy
 *   tmat.uvs into t->uv (CopyUV); AddToEnvList(v) inlined; target_sound inlined as
 *   PlayTargetSound(info[t->type].sound, t->slot, t->soundPos, 2).
 *   (The arcade vcopy120 into lv[] and ZOID_UpdateObject are gone on N64.)
 * Shaping quirks: PointInDir is a static wrapper around LookInDir that is
 *   inlined; its two parameters take the 8 bytes of frame below tmat (96-byte frame, v home
 *   sp+92, tmat sp+44). An equivalent spelling with the wrapper taking the GameCar * and no
 *   car local also matches. Literal 0.0333333f (0x3D088880) own .rodata, verified.
 *   -O2 does not match. Whole-program unit: EQUAL.
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32; typedef float f32;

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

/* PC: a target info record (targetInfo) is 0x40 bytes: its name, file, start function and visual function are
 * pointers. Every field is spelled out, whichever view the loader converts the ROM's records by
 * (docs/port/POINTERS.md 10). */
typedef struct TargInfo {
    char *name, *file;
    void *start;
    void *visFunc;          /* 0x0C */
    s16 kind;               /* 0x10 */
    u16 flags;
    s16 shape;
    s8 category, element;
    f32 value;
    s32 sound;              /* 0x1C */
    s32 w20, w24, w28;
    f32 f2C;
} TargInfo;

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
extern TargInfo targetInfo[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Visual *gEnvList;

Visual *GrabEnvEntry(void);
void LookInDir(f32 *v, f32 m[3][3]);
void CopyUV(f32 src[3][3], f32 dst[3][3]);
s32 PlayTargetSound(s32 sound, s32 slot, f32 *position, u8 mode);

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
