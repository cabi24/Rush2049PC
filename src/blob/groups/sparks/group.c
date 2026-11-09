#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

typedef struct {
    u8 pad000[0xE8];
    s32 unkE8;
    u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad0EC_308[0x35B - 0x308];
    u8 unk35B;
    u8 pad35C[0x380 - 0x35C];
    void *unk380;
    u8 pad384[0x3A3 - 0x384];
    u8 unk3A3;
    u8 pad3A4[0x3B8 - 0x3A4];
} GameCar;

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

typedef struct ModelObj {
    /* 0x0 */ void *pad0;
    /* 0x8 */ s16 state;
    /* 0xA */ s16 model;
    /* 0xC */ s16 player;
} ModelObj;

typedef struct {
    /* 0x0 */ f32 v;
    /* 0x4 */ u8 pad4[0x804];
} Rec808;

s32 CheckVisibleAll(ModelObj *obj, s32 flag) {
    if (flag) {
        MBOX_ShowObject(obj->model, 0, 15);
    } else {
        MBOX_HideObject(obj->model, 1, 15);
    }
    return flag;
}

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; f32 *m; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } ModelSlot;
#define TBL ((ModelSlot *) &ObjInstanceList)
/* deleted static: the 0x44-byte model slot's matrix pointer; inlined at all
 * three uses (this is what puts the index in a0 and the pointer in a1) */
static f32 *model_mat(s16 idx) { return TBL[idx].m; }
/* game.frame_rate 60: the flame's frame index steps once a frame of two retraces, so it flickers and dies out at the
 * N64's rate (pc_game_fps.h, docs/port/FRAMERATE.md 7) */
#define PC_IF30(c) ((!pc_fps60 || pc_frame_steps) && (c))
void PositionObject(s32 idx, f32 x, f32 y, f32 z, f32 nx, f32 ny, f32 nz) {
    f32 *v;

    v = model_mat(idx);
    v[9] = x;
    v[10] = y;
    v[11] = z;
    LookInDir(&v[9], v);
    v[9] = nx;
    v[10] = ny;
    v[11] = nz;
}

void HandleASpark(ModelObj *ipa_s1, s32 arg1, s32 ipa_s2, f32 ipa_f20, f32 ipa_f22, f32 ipa_f24, s16 arg0) {
    GameCar *car;
    s16 t;
    f32 *m;

    car = PC_CAR(ipa_s1->player);
    if (arg0 != 0 && !(gGameState & 8)) {
        if (ipa_s1->state < 0) {
            if (arg1 == 0) {
                goto reload;
            }
            ipa_s1->state = 0;
            PositionObject(ipa_s1->model, M2C_FIELD(car, f32 *, 0xA4), M2C_FIELD(car, f32 *, 0xA8), M2C_FIELD(car, f32 *, 0xAC), ipa_f20, ipa_f22, ipa_f24);
            t = (s32) PC_MODEL_SPEED(ipa_s1->player);
            if (t < 100) {
                m = model_mat(ipa_s1->model);
                ScaleUV(m, m, (f32) t / 100.0f);
            }
        } else if (arg1 != 0) {
            PositionObject(ipa_s1->model, M2C_FIELD(car, f32 *, 0xA4), M2C_FIELD(car, f32 *, 0xA8), M2C_FIELD(car, f32 *, 0xAC), ipa_f20, ipa_f22, ipa_f24);
            t = (s32) PC_MODEL_SPEED(ipa_s1->player);
            if (t < 100) {
                m = model_mat(ipa_s1->model);
                ScaleUV(m, m, (f32) t / 100.0f);
            }
            ipa_s1->state = ipa_s1->state + PC_STEPS(1);
            if (PC_IF30(ipa_s1->state >= 3)) {
                ipa_s1->state = Random(4.0f) + 2.0f;
            }
        } else if (ipa_s1->state >= 4) {
            ipa_s1->state = 3;
        } else {
            ipa_s1->state = ipa_s1->state - PC_STEPS(1);
        }
        if (ipa_s1->state >= 6) {
            ipa_s1->state = 5;
        }
        if (CheckVisibleAll(ipa_s1, ipa_s1->state >= 0) != 0) {
            ZOID_ChangeTexture(gObjList[ipa_s2], 0, gTexList[ipa_s1->state]);
            return;
        }
    }
reload:
    MBOX_HideObject(ipa_s1->model, 1, 15);
    ipa_s1->state = -1;
}

/* the five roots below only set up the register parameters of _a; they are
 * address-taken (function-pointer table in StartSparks) and keep the normal ABI */
void AnimateBSpark(ModelObj *arg0, s16 arg1) {
    GameCar *car;
    s16 lvl;
    s32 t;

    car = PC_CAR(arg0->player);
    lvl = (s32) M2C_FIELD((PC_MODEL(arg0->player)), f32 *, 0x3F0);
    t = M2C_FIELD((PC_MODEL(arg0->player)), s8 *, 0x641);
    HandleASpark(arg0, lvl >= 13 && (t != 0 || (M2C_FIELD(car, s32 *, 0xE8) & 0x800) != 0), 194,
                                 0.0f,
                                 M2C_FIELD(car, f32 *, 0xD8) + (t != 0 ? 2.0f : M2C_FIELD((PC_MODEL(arg0->player)), f32 *, 0x544)),
                                 M2C_FIELD(car, f32 *, 0xDC) - 2.0f,
                                 arg1);
}

void AnimateRRSpark(ModelObj *arg0, s16 arg1) {
    f32 two = 2.0f;
    Rec808 *rec;
    GameCar *car;
    s32 flag;
    s16 lvl;
    f32 f24;

    car = PC_CAR(arg0->player);
    rec = PC_MODEL(arg0->player);
    lvl = (s32) M2C_FIELD(rec, f32 *, 0x3F0);
    flag = lvl >= 21 && (M2C_FIELD(car, s32 *, 0xE8) & 0x4000) != 0;
    f24 = M2C_FIELD(car, f32 *, 0xD0);
    HandleASpark(arg0, flag, 194,
                                 M2C_FIELD(car, f32 *, 0xC8) + rrspark_x_offset,
                                 M2C_FIELD(car, f32 *, 0xCC) + M2C_FIELD(rec, f32 *, 0x4E8) * two,
                                 f24,
                                 arg1);
}

void AnimateRFSpark(ModelObj *arg0, s16 arg1) {
    f32 two = 2.0f;
    Rec808 *rec;
    GameCar *car;
    s32 flag;
    s16 lvl;
    f32 f24;

    car = PC_CAR(arg0->player);
    rec = PC_MODEL(arg0->player);
    lvl = (s32) M2C_FIELD(rec, f32 *, 0x3F0);
    flag = lvl >= 21 && (M2C_FIELD(car, s32 *, 0xE8) & 0x80) != 0;
    f24 = M2C_FIELD(car, f32 *, 0xB8);
    HandleASpark(arg0, flag, 194,
                                 M2C_FIELD(car, f32 *, 0xB0) + rfspark_x_offset,
                                 M2C_FIELD(car, f32 *, 0xB4) + M2C_FIELD(rec, f32 *, 0x430) * two,
                                 f24,
                                 arg1);
}

void AnimateLRSpark(ModelObj *arg0, s16 arg1) {
    f32 two = 2.0f;
    Rec808 *rec;
    GameCar *car;
    s32 flag;
    s16 lvl;
    f32 f24;

    car = PC_CAR(arg0->player);
    rec = PC_MODEL(arg0->player);
    lvl = (s32) M2C_FIELD(rec, f32 *, 0x3F0);
    flag = lvl >= 21 && (M2C_FIELD(car, s32 *, 0xE8) & 0x2000) != 0;
    f24 = M2C_FIELD(car, f32 *, 0xDC);
    HandleASpark(arg0, flag, 194,
                                 M2C_FIELD(car, f32 *, 0xD4) - lrspark_x_offset,
                                 M2C_FIELD(car, f32 *, 0xD8) + M2C_FIELD(rec, f32 *, 0x544) * two,
                                 f24,
                                 arg1);
}

void AnimateLFSpark(ModelObj *arg0, s16 arg1) {
    f32 two = 2.0f;
    Rec808 *rec;
    GameCar *car;
    s32 flag;
    s16 lvl;
    f32 f24;

    car = PC_CAR(arg0->player);
    rec = PC_MODEL(arg0->player);
    lvl = (s32) M2C_FIELD(rec, f32 *, 0x3F0);
    flag = lvl >= 21 && (M2C_FIELD(car, s32 *, 0xE8) & 0x100) != 0;
    f24 = M2C_FIELD(car, f32 *, 0xC4);
    HandleASpark(arg0, flag, 194,
                                 M2C_FIELD(car, f32 *, 0xBC) - lfspark_x_offset,
                                 M2C_FIELD(car, f32 *, 0xC0) + M2C_FIELD(rec, f32 *, 0x48C) * two,
                                 f24,
                                 arg1);
}

/* stand-in caller: keeps CheckVisibleAll out of line under -O3 */
