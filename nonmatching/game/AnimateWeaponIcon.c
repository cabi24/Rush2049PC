/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* AnimateWeaponIcon = _AnimateWeaponIcon (MAT3 targets.c, GC 0x8004E0BC, size 0x1F4). */
/* PC (compiled into the PC build, CMakeLists.txt): the weapon icons' visual function, the +0x0C slot of their target
 * info records (PC +0x18; docs/port/POINTERS.md 10). SetTargetDef puts it in the environment visual it grabs
 * for the node (flag 2), and the environment visual pass (UpdateEnvirons) calls it every frame with op 1;
 * op 0 frees the visual. No direct callers. The visual has its PC layout, its data the node's 32-bit window
 * address; the node (arena) and its state, the spin rates (a heap record), keep their N64 layout. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Battle {
    s8 type; s8 ammoCnt; s16 health;
    u8 pad04[0x34 - 4];
} Battle;
/* PC: the car record's layout (0x468 bytes, docs/port/CARS.md). */
typedef struct Car {
    u8 pad00[0x110];
    struct pc_visual visuals[21];
    u8 pad00_308[0x380 - 0x308];
    void *input;
    Battle battle;        /* 0x384, PC 0x430 */
} Car;
typedef struct Visual {
    struct Visual *next;  /* 0x00 */
    s16 index;            /* 0x04 */
    s16 objnum;           /* 0x06 */
    s16 slot;             /* 0x08 */
    u32 data;             /* 0x0c */
    f32 timeStamp;        /* 0x10 */
    void (*func)(struct Visual *, s16); /* 0x14 */
} Visual;
typedef struct Target {
    u8 pad00[4];
    u8 Flags;             /* 0x04 */
    u8 pad05[0x0c - 5];
    s32 dlIdx;            /* 0x0c */
    u8 pad10[0x14 - 0x10];
    f32 uvs[3][3];        /* 0x14 (mat.mat3.uvs) */
    u8 pad38[0x50 - 0x38];
    s16 type;             /* 0x50 */
    u8 pad52[0x6c - 0x52];
    u32 data;             /* 0x6c: a window address (the node keeps its N64 layout) */
} Target;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9.2); flags is its first word. */
typedef struct ObjInstance {
    u32 flags, lights; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip;
    intptr_t pal; u32 prim, env;
} ObjInstance;
#define N64P(x) ((void *)PC_N64P(x))

extern s32 gPauseState;
extern volatile f32 gFrameSeconds;
extern s16 gNumPlayers;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern ObjInstance ObjInstanceList[];     /* ObjInstanceList */
void ReleaseEnviron(Visual *v, s32 flag);
void RotateUV(f32 *angV, f32 (*uvs)[3]);
void MBOX_ShowObject(s32 a, s32 mode, u32 v);
void MBOX_HideObject(s32 a, s32 mode, u32 v);

u32 ZOID_ObjectFlags(s16 objnum);

void AnimateWeaponIcon(Visual *v, s16 op)
{
    Target *t;
    f32 *angV;
    s32 pad;
    f32 angVF[3];
    s32 i;
    s32 ok;
    s32 wep;
    if (!op) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) return;
    t = (Target *)N64P(v->data);
    if (t->Flags & 2) {
        angV = N64P(t->data);
        angVF[0] = angV[0] * gFrameSeconds;
        angVF[1] = angV[1] * gFrameSeconds;
        angVF[2] = angV[2] * gFrameSeconds;
        RotateUV(angVF, t->uvs);
    } else if (ZOID_ObjectFlags(t->dlIdx) & 0x80000000) {
        switch (t->type) {
        case 350: wep = 0; break;
        case 351: wep = 1; break;
        case 352: wep = 2; break;
        case 355: wep = 3; break;
        case 356: wep = 4; break;
        case 357: wep = 5; break;
        case 358: wep = 6; break;
        case 360: wep = 7; break;
        default: wep = -1; break;
        }
        if (wep != -1) {
            ok = 1;
            for (i = 0; i < gNumPlayers; i++) {
                if (wep == PC_CAR_KIND(i)) ok = 0;
            }
            if (ok) {
                MBOX_ShowObject(t->dlIdx, 0, 15);
                t->Flags |= 2;
            }
        }
    } else {
        MBOX_HideObject(t->dlIdx, 0, 15);
    }
}
