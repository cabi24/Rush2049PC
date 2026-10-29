/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_8010C2E4: per-player trigger callback for a camera/scene object (no direct callers; reached
 * through a function pointer with four arguments, the last two unused and homed).
 * For player *player (record D_8014A250[], 0x808 bytes), when the record's +0x640 byte is 0 and its
 * +0x6C4 halfword is negative, it reads the 24-byte polygon records D_801497F8[] under the four
 * wheel polygon indices (+0x5A0..+0x5A6).  If any wheel polygon has flag 0x20 set and its 5-bit
 * id (bits 11..15) equals the scene id, the scene gets bit (24 + player) set (plus 0x200 when no
 * player bit was set yet or flag 0x4000 is set); otherwise that player's bit is cleared.
 * Always returns 0.  No arcade ancestor identified.
 * Shaping: the empty `if (cam == NULL)` (a compiled-out debug message) is what gives the camera
 * parameter its register priority, so it stays in a0 and ctl/sc take a1/a2 (found with the traced
 * uopt: forcing ctl=a1, sc=a2 reproduced retail exactly); `idx` as a variable (kept in v0 for the
 * bit shift); `id = sc->id; id <<= 11;` as two statements. The 's group/ring label is a
 * false positive.  Also matches at -O2.
 */
#define NULL ((void *)0)
#define DEBUG_PRINT(args)
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct Poly24 {
    u16 pad0;
    u16 flags;
    u8 pad4[20];
} Poly24;

typedef struct Car {
    u8 pad0[0x5A0];
    u16 wheelPoly[4];
    u8 pad5A8[0x640 - 0x5A8];
    s8 b640;
    u8 pad641[0x6C4 - 0x641];
    s16 s6C4;
    u8 pad6C6[0x808 - 0x6C6];
} Car;

#ifdef TARGET_PC
/* PC: a camera (a level-object node), its state CamCtl (a heap record of its category) and the scene, a track
 * WAD's PTHD record with its key list, keep their N64 layout: their pointers are 32-bit window addresses
 * (docs/port/POINTERS.md 10). */
enum n64p_CamCtl { n64p_CamCtl_ };
enum n64p_CamScene { n64p_CamScene_ };
typedef struct CamScene {
    u8 pad00[0x10];
    u32 flags;
    s16 count;
    s16 id;
} CamScene;
typedef struct CamCtl {
    enum n64p_CamScene scene;
} CamCtl;
typedef struct Camera {
    u8 pad00[0x6C];
    enum n64p_CamCtl ctl;
} Camera;
#define N64P(x) _Generic((x), enum n64p_CamCtl: (CamCtl *)(uintptr_t)(x), enum n64p_CamScene: (CamScene *)(uintptr_t)(x), default: (x))
#else
typedef struct CamScene {
    u8 pad00[0x10];
    u32 flags;
    s16 count;
    s16 id;
} CamScene;
typedef struct CamCtl {
    CamScene *scene;
} CamCtl;
typedef struct Camera {
    u8 pad00[0x6C];
    CamCtl *ctl;
} Camera;
#define N64P(x) x
#endif

#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Car D_8014A250[];
#define PC_MODEL(i) &D_8014A250[i]
#endif
extern Poly24 *D_801497F8;

#ifdef TARGET_PC
/* PC: a scene's cars (docs/port/MORECARS.md 3.1). Bits 24-31 of its flags are cars 0-7, as on the N64; a race of
 * more cars has no bit there for car 8 on (1 << 32 is undefined in C, and a shift that takes its count's low 5 bits,
 * as sllv and x86-64's do, would set the scene's flag 1). Car 8 and up keep theirs in their car record instead,
 * PC_CAR_SCENES: bit n while the car is in a scene whose trigger is collision index n, the 0-31 the test below can
 * match (pc_game_types.h). The car records of the race are cleared at its start (init_cars), as the scenes' bits
 * are when their track's file loads. A scene has a car when either says so. */
extern volatile s16 D_801543CA;
static u32 scene_cars_past_8(CamScene *sc)
{
    s32 j;
    u32 any = 0;

    if ((u32)sc->id < 32) {
        for (j = 8; j < D_801543CA; j++) {
            any |= (PC_CAR_SCENES(j) >> sc->id) & 1;
        }
    }
    return any;
}
static void scene_car(CamScene *sc, s32 idx, s32 in)
{
    u32 bit;

    if (idx < 8) {
        bit = 1u << (idx + 24);
        sc->flags = in ? sc->flags | bit : sc->flags & ~bit;
    } else if ((u32)sc->id < 32) {
        bit = 1u << sc->id;
        PC_CAR_SCENES(idx) = in ? PC_CAR_SCENES(idx) | bit : PC_CAR_SCENES(idx) & ~bit;
    }
}
#define SCENE_CARS(sc) ((sc->flags >> 24) | scene_cars_past_8(sc))
#define SCENE_CAR_IN(sc, idx) scene_car(sc, idx, 1)
#define SCENE_CAR_OUT(sc, idx) scene_car(sc, idx, 0)
#else
#define SCENE_CARS(sc) (sc->flags >> 24)
#define SCENE_CAR_IN(sc, idx) sc->flags |= 1 << (idx + 24)
#define SCENE_CAR_OUT(sc, idx) sc->flags &= ~(1 << (idx + 24))
#endif

s32 func_8010C2E4(Camera *cam, s16 *player, s32 arg2, s32 arg3) {
    s32 idx;
    Car *car;
    CamCtl *ctl;
    CamScene *sc;
    s32 id;
    u16 f0, f1, f2, f3;

    idx = *player;
    car = PC_MODEL(idx);
    if (car->b640 != 0 || car->s6C4 >= 0) {
        return 0;
    }
    if (cam == NULL) {
        DEBUG_PRINT(("func_8010C2E4: no camera\n"));
    }
    ctl = N64P(cam->ctl);
    sc = N64P(ctl->scene);
    id = sc->id;
    id <<= 11;
    f0 = D_801497F8[car->wheelPoly[0]].flags;
    f1 = D_801497F8[car->wheelPoly[1]].flags;
    f2 = D_801497F8[car->wheelPoly[2]].flags;
    f3 = D_801497F8[car->wheelPoly[3]].flags;
    if (((f0 & 0x20) && (f0 & 0xF800) == id) || ((f1 & 0x20) && (f1 & 0xF800) == id) ||
        ((f2 & 0x20) && (f2 & 0xF800) == id) || ((f3 & 0x20) && (f3 & 0xF800) == id)) {
        if (SCENE_CARS(sc) == 0 || (sc->flags & 0x4000)) {
            sc->flags |= 0x200;
        }
        SCENE_CAR_IN(sc, idx);
    } else {
        SCENE_CAR_OUT(sc, idx);
    }
    return 0;
}
