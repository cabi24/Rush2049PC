/* sparks/group.c: the sparks a car throws where its body scrapes (MAT3 visuals.c, AnimateLFSpark() ...
 * AnimateBSpark(), HandleASpark(), PositionObject(), CheckVisibleAll()).
 *
 * Each car has five spark visuals (car visuals 1-5, StartSparks): one at each corner and one under the car. The
 * physics side sets the car's APPEAR_SPARK_C0..C3 bits where a corner scrapes and APPEAR_THUMP for an underbody hit;
 * each visual's animator here shows its spark object (one of the car's parts, gCarParts) flickering through the
 * spark textures while its scrape lasts, turned along the car's motion and smaller at low speed. */
#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

/* The car record (tCarData, N64 offsets; docs/port/CARS.md 2): its motion, its tires' places and its appearance. */
typedef struct {
    u8 pad000[0xA4];
    f32 v[3];                   /* 0xA4 the car's velocity, in its own axes */
    f32 tireR[4][3];            /* 0xB0 the tires' places, in the car's axes */
    u8 pad0E0[0xE8 - 0xE0];
    s32 appearance;             /* 0xE8 APPEAR_* */
    u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad308[0x35B - 0x308];
    u8 slot;
    u8 pad35C[0x380 - 0x35C];
    void *player;
    u8 pad384[0x3A3 - 0x384];
    u8 Points;
    u8 pad3A4[0x3B8 - 0x3A4];
} tCarData;

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* A car visual (MAT3 tVisual) as the sparks use it. PC: next is a host pointer. */
typedef struct tVisual {
    /* 0x0 */ void *next;
    /* 0x8 */ s16 index;        /* the spark's texture, 0-5, -1 not sparking */
    /* 0xA */ s16 objnum;       /* the spark's scene object */
    /* 0xC */ s16 slot;         /* the car */
} tVisual;

/* A tire of the model record (MAT3 anon_tradius_springK_5c, 0x5C bytes): its radius. */
typedef struct { f32 tradius; u8 pad04[0x5C - 4]; } Tire;
/* The model record (tModelData): the speed, the tires' radii, the roof scraping. */
typedef struct {
    /* 0x000 */ u8 pad000[0x3F0];
    /* 0x3F0 */ f32 magvel;     /* the speed, mph */
    /* 0x3F4 */ u8 pad3F4[0x430 - 0x3F4];
    /* 0x430 */ Tire tires[4];
    /* 0x5A0 */ u8 pad5A0[0x641 - 0x5A0];
    /* 0x641 */ s8 topscrape;   /* the roof is scraping */
    /* 0x642 */ u8 pad642[0x808 - 0x642];
} tModelData;

/* Show visual v's spark object in every view if `on`, else hide it; returns `on`. */
s32 CheckVisibleAll(tVisual *v, s32 on) {
    if (on) {
        MBOX_ShowObject(v->objnum, 0, 15);
    } else {
        MBOX_HideObject(v->objnum, 1, 15);
    }
    return on;
}

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, culling; f32 *mat; f32 scale, w; u16 index; s16 child, next, numLights; void *lights, *texture[4], *tile, *tileSize, *palette; u32 color, envcolor; } ObjInstance;
#define TBL ((ObjInstance *) &ObjInstanceList)
/* deleted static: the scene node's matrix pointer (ZOID_FindMatrix); inlined at all three uses (this is what puts
 * the index in a0 and the pointer in a1) */
static f32 *model_mat(s16 objnum) { return TBL[objnum].mat; }
/* game.frame_rate 60: the flame's frame index steps once a frame of two retraces, so it flickers and dies out at the
 * N64's rate (pc_game_fps.h, docs/port/FRAMERATE.md 7) */
#define PC_IF30(c) ((!pc_fps60 || pc_frame_steps) && (c))

/* Place scene object `objnum` (a child of the car's frame) at (x, y, z), turned to look along (dx, dy, dz). */
void PositionObject(s32 objnum, f32 dx, f32 dy, f32 dz, f32 x, f32 y, f32 z) {
    f32 *mat;

    mat = model_mat(objnum);
    mat[9] = dx;
    mat[10] = dy;
    mat[11] = dz;
    LookInDir(&mat[9], mat);
    mat[9] = x;
    mat[10] = y;
    mat[11] = z;
}

/* One frame of spark visual v (`op` 0: the visual stops): while `on` (its corner scrapes) the spark sits at
 * (x, y, z) in the car's axes, looks along the car's motion and is scaled by speed/100 below 100 mph; it lights up
 * at texture 0, steps a texture a frame, and from texture 3 jumps at random to 2-5, so it flickers. Once the scrape
 * ends it drops to texture 3 and counts down to nothing. Its texture comes from gTexList and goes on model
 * gObjList[spark]. No sparks in the attract demo (gGameState 8: GS_DEMO). */
void HandleASpark(tVisual *v, s32 on, s32 spark, f32 x, f32 y, f32 z, s16 op) {
    tCarData *car;
    s16 mph;
    f32 *mat;

    car = PC_CAR(v->slot);
    if (op != 0 && !(gGameState & 8)) {
        if (v->index < 0) {
            if (on == 0) {
                goto reload;
            }
            v->index = 0;
            PositionObject(v->objnum, car->v[0], car->v[1], car->v[2], x, y, z);
            mph = (s32) PC_MODEL_SPEED(v->slot);
            if (mph < 100) {
                mat = model_mat(v->objnum);
                ScaleUV(mat, mat, (f32) mph / 100.0f);
            }
        } else if (on != 0) {
            PositionObject(v->objnum, car->v[0], car->v[1], car->v[2], x, y, z);
            mph = (s32) PC_MODEL_SPEED(v->slot);
            if (mph < 100) {
                mat = model_mat(v->objnum);
                ScaleUV(mat, mat, (f32) mph / 100.0f);
            }
            v->index = v->index + PC_STEPS(1);
            if (PC_IF30(v->index >= 3)) {
                v->index = Random(4.0f) + 2.0f;
            }
        } else if (v->index >= 4) {
            v->index = 3;
        } else {
            v->index = v->index - PC_STEPS(1);
        }
        if (v->index >= 6) {
            v->index = 5;
        }
        if (CheckVisibleAll(v, v->index >= 0) != 0) {
            ZOID_ChangeTexture(gObjList[spark], 0, gTexList[v->index]);
            return;
        }
    }
reload:
    MBOX_HideObject(v->objnum, 1, 15);
    v->index = -1;
}

/* The five animators below only pick their spark's place and condition for HandleASpark; they are address-taken
 * (StartSparks puts them in the car's visuals) and keep the normal ABI. All five use the spark model
 * gObjList[194]. */

/* Under the car: from 13 mph, while the roof scrapes (topscrape: the spark goes 2 ft up) or the underbody hits
 * (APPEAR_THUMP: at the rear tire's radius), 2 ft ahead of the left rear tire. */
void AnimateBSpark(tVisual *v, s16 op) {
    tCarData *car;
    s16 mph;
    s32 top;

    car = PC_CAR(v->slot);
    mph = (s32) ((tModelData *) PC_MODEL(v->slot))->magvel;
    top = ((tModelData *) PC_MODEL(v->slot))->topscrape;
    HandleASpark(v, mph >= 13 && (top != 0 || (car->appearance & APPEAR_THUMP) != 0), 194,
                                 0.0f,
                                 car->tireR[3][1] + (top != 0 ? 2.0f : ((tModelData *) PC_MODEL(v->slot))->tires[3].tradius),
                                 car->tireR[3][2] - 2.0f,
                                 op);
}

/* The right rear corner (tire 2, the +x side): from 21 mph while it scrapes (APPEAR_SPARK_C2), rrspark_x_offset
 * outside the tire and a tire's diameter up. */
void AnimateRRSpark(tVisual *v, s16 op) {
    f32 two = 2.0f;
    tModelData *m;
    tCarData *car;
    s32 on;
    s16 mph;
    f32 z;

    car = PC_CAR(v->slot);
    m = PC_MODEL(v->slot);
    mph = (s32) m->magvel;
    on = mph >= 21 && (car->appearance & APPEAR_SPARK_C2) != 0;
    z = car->tireR[2][2];
    HandleASpark(v, on, 194,
                                 car->tireR[2][0] + rrspark_x_offset,
                                 car->tireR[2][1] + m->tires[2].tradius * two,
                                 z,
                                 op);
}

/* The right front corner (tire 0): as AnimateRRSpark, by APPEAR_SPARK_C0 and rfspark_x_offset. */
void AnimateRFSpark(tVisual *v, s16 op) {
    f32 two = 2.0f;
    tModelData *m;
    tCarData *car;
    s32 on;
    s16 mph;
    f32 z;

    car = PC_CAR(v->slot);
    m = PC_MODEL(v->slot);
    mph = (s32) m->magvel;
    on = mph >= 21 && (car->appearance & APPEAR_SPARK_C0) != 0;
    z = car->tireR[0][2];
    HandleASpark(v, on, 194,
                                 car->tireR[0][0] + rfspark_x_offset,
                                 car->tireR[0][1] + m->tires[0].tradius * two,
                                 z,
                                 op);
}

/* The left rear corner (tire 3, the -x side): by APPEAR_SPARK_C3, lrspark_x_offset outside the tire. */
void AnimateLRSpark(tVisual *v, s16 op) {
    f32 two = 2.0f;
    tModelData *m;
    tCarData *car;
    s32 on;
    s16 mph;
    f32 z;

    car = PC_CAR(v->slot);
    m = PC_MODEL(v->slot);
    mph = (s32) m->magvel;
    on = mph >= 21 && (car->appearance & APPEAR_SPARK_C3) != 0;
    z = car->tireR[3][2];
    HandleASpark(v, on, 194,
                                 car->tireR[3][0] - lrspark_x_offset,
                                 car->tireR[3][1] + m->tires[3].tradius * two,
                                 z,
                                 op);
}

/* The left front corner (tire 1): by APPEAR_SPARK_C1, lfspark_x_offset outside the tire. */
void AnimateLFSpark(tVisual *v, s16 op) {
    f32 two = 2.0f;
    tModelData *m;
    tCarData *car;
    s32 on;
    s16 mph;
    f32 z;

    car = PC_CAR(v->slot);
    m = PC_MODEL(v->slot);
    mph = (s32) m->magvel;
    on = mph >= 21 && (car->appearance & APPEAR_SPARK_C1) != 0;
    z = car->tireR[1][2];
    HandleASpark(v, on, 194,
                                 car->tireR[1][0] - lfspark_x_offset,
                                 car->tireR[1][1] + m->tires[1].tradius * two,
                                 z,
                                 op);
}

/* stand-in caller: keeps CheckVisibleAll out of line under -O3 */
