/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* func_8010D680 = _AnimateWeaponIcon (MAT3 targets.c, GC 0x8004E0BC, size 0x1F4). */
/* PC (compiled into the PC build, CMakeLists.txt): the weapon icons' visual function, the +0x0C slot of their target
 * info records (PC +0x18; docs/port/POINTERS.md 10). engine_torque_calc puts it in the environment visual it grabs
 * for the node (flag 2), and the environment visual pass (PhysicsObjectList_Update) calls it every frame with op 1;
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
#ifdef TARGET_PC
/* PC: the car record's layout (0x468 bytes, docs/port/CARS.md). */
typedef struct Car {
    u8 pad00[0x110];
    struct pc_visual visuals[21];
    u8 pad00_308[0x380 - 0x308];
    void *input;
    Battle battle;        /* 0x384, PC 0x430 */
} Car;
#else
typedef struct Car {
    u8 pad00[0x384];
    Battle battle;        /* 0x384 */
} Car;
#endif
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
#ifdef TARGET_PC
    u32 data;             /* 0x6c: a window address (the node keeps its N64 layout) */
#else
    f32 *data;            /* 0x6c */
#endif
} Target;
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9.2); flags is its first word. */
typedef struct ObjInstance {
    u32 flags, lights; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip;
    intptr_t pal; u32 prim, env;
} ObjInstance;
#define N64P(x) ((void *)(uintptr_t)(x))
#else
typedef struct ObjInstance { u32 flags; u8 pad04[0x44 - 4]; } ObjInstance;
#define N64P(x) x
#endif

extern s32 D_801170FC;               /* gPauseState */
extern volatile f32 D_8002EB94;      /* gFrameSeconds */
extern s16 D_8014A108;               /* gNumPlayers */
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Car D_80152818[];             /* gGameCar */
#define PC_CAR_KIND(i) D_80152818[i].battle.type
#endif
extern ObjInstance D_8012E700[];     /* ObjInstanceList */
void entity_transform_apply(Visual *v, s32 flag);           /* ReleaseEnviron */
void sound_position_set(f32 *angV, f32 (*uvs)[3]);          /* RotateUV */
#ifdef TARGET_PC
void model_transform_setup(s32 a, s32 mode, u32 v);
void model_data_load(s32 a, s32 mode, u32 v);
#else
void model_transform_setup(s32 dl, s32 a, s32 b);           /* MBOX_ShowObject */
void model_data_load(s32 dl, s32 a, s32 b);                 /* MBOX_HideObject */
#endif

static u32 ZOID_ObjectFlags(s16 objnum)
{
    return D_8012E700[objnum].flags;
}

void func_8010D680(Visual *v, s16 op)
{
    Target *t;
    f32 *angV;
    s32 pad;
    f32 angVF[3];
    s32 i;
    s32 ok;
    s32 wep;
    if (!op) {
        entity_transform_apply(v, 1);
        return;
    }
    if (D_801170FC != 0) return;
    t = (Target *)N64P(v->data);
    if (t->Flags & 2) {
        angV = N64P(t->data);
        angVF[0] = angV[0] * D_8002EB94;
        angVF[1] = angV[1] * D_8002EB94;
        angVF[2] = angV[2] * D_8002EB94;
        sound_position_set(angVF, t->uvs);
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
            for (i = 0; i < D_8014A108; i++) {
                if (wep == PC_CAR_KIND(i)) ok = 0;
            }
            if (ok) {
                model_transform_setup(t->dlIdx, 0, 15);
                t->Flags |= 2;
            }
        }
    } else {
        model_data_load(t->dlIdx, 0, 15);
    }
}
