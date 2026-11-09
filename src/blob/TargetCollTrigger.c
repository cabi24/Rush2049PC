#include "game/game.h"


#define DEBUG_PRINT(args)

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
#define N64P(x) _Generic((x), enum n64p_CamCtl: (CamCtl *)PC_N64P(x), enum n64p_CamScene: (CamScene *)PC_N64P(x), default: (x))

VIEW(Poly24 *, stree_surf);
#define stree_surf stree_surf_view

static u32 scene_cars_past_8(CamScene *sc)
{
    s32 j;
    u32 any = 0;

    if ((u32)sc->id < 32) {
        for (j = 8; j < gNumCars; j++) {
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

s32 TargetCollTrigger(Camera *cam, s16 *player, s32 arg2, s32 arg3) {
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
    f0 = stree_surf[car->wheelPoly[0]].flags;
    f1 = stree_surf[car->wheelPoly[1]].flags;
    f2 = stree_surf[car->wheelPoly[2]].flags;
    f3 = stree_surf[car->wheelPoly[3]].flags;
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
