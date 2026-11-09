/* The battle's shots (MAT3 shots.c): the guns' bullets, the missiles, the mines, the sonic blast and the shells'
 * cases; each frame they fly, hit the track (ShotToWorldCollision) or a car, and do their damage; the cars' shields
 * and invisibility (UpdateDefense) and the gun's aim (ElevateGun) too. */
#include "game/game.h"

typedef union RGBA { u8 c[4]; u32 rgba; } RGBA;

typedef struct StatLL { s8 dbl; s32 numElements; s32 elementSize; void *list; void *head; void *store; } StatLL;

typedef struct Pad {
    u8 slot, controller; u8 pad02[2];
    u32 edges;            /* 0x04 */
    u32 levels;           /* 0x08 */
    u32 autoEdges;        /* 0x0c */
    f32 analog[2];        /* 0x10 */
    u32 controls[11];     /* 0x18 */
    u8 pad44[0x50 - 0x44];
} Pad;

typedef struct Defense {
    u32 active;           /* 0x00 */
    f32 invsTime;         /* 0x04 */
    f32 shldTime;         /* 0x08 */
    f32 shldScale;        /* 0x0c */
    f32 unknown10;        /* 0x10 */
} Defense;

typedef struct Battle {
    s8 type;              /* 0x00 */
    s8 ammoCnt;           /* 0x01 */
    s16 health;           /* 0x02 */
    s16 damageThisFrame;  /* 0x04 */
    u8 pad06[2];
    Defense defense;      /* 0x08 */
    s8 shoot;             /* 0x1c */
    u8 fadeAlpha;         /* 0x1d */
    s8 fadeState;         /* 0x1e */
    s8 Points;            /* 0x1f */
    s32 loaded;           /* 0x20 */
    f32 fadeTime;         /* 0x24 */
    f32 reloadingTime;    /* 0x28 */
    f32 pitch;            /* 0x2c */
    f32 yaw;              /* 0x30 */
} Battle;

typedef struct Car {
    u8 pad00[8];
    f32 pos[3];           /* 0x08 */
    f32 vel[3];           /* 0x14 */
    f32 acc[3];           /* 0x20 */
    f32 uvs[3][3];        /* 0x2c */
    u8 pad50[0x110 - 0x50]; struct pc_visual visuals[21];
    s8 collidable;        /* 0x308 */
    u8 pad309[0x34c - 0x309];
    RGBA IRclr;           /* 0x34c */
    u8 pad350[0x359 - 0x350];
    s8 dead;              /* 0x359 */
    u8 pad35a;
    s8 slot;              /* 0x35b */
    u8 pad35c[0x380 - 0x35c];
    Pad *player;          /* 0x380 */
    Battle battle;        /* 0x384 */
} Car;

typedef struct Model {
    u8 pad00[8];
    u8 body_type;         /* 0x08 */
    u8 pad09[0xf4 - 9];
    f32 bodyR[4][3];      /* 0xf4 */
    f32 centerForce[3];   /* 0x124 */
    u8 pad130[0x13c - 0x130];
    f32 centerMoment[3];  /* 0x13c */
    u8 pad148[0x640 - 0x148];
    s8 crashflag;         /* 0x640 */
    u8 pad641[0x654 - 0x641];
    f32 colrad;           /* 0x654 */
    u8 pad658[0x6c4 - 0x658];
    s16 moving_state;     /* 0x6c4 */
    u8 pad6c6[0x808 - 0x6c6];
} Model;

typedef struct ColSize { f32 colfront, colrear, colside, colheight; } ColSize;

typedef struct Streak {
    void *next, *prev;
    u32 flags;
    f32 delay;
    f32 newpos[3];
    f32 lastpos[3];
    f32 backVerts[2][3];
} Streak;

typedef struct ShotObj {
    struct ShotObj *next;
    u32 mbIdx;            /* 0x04 */
    UVMat mat;            /* 0x08 */
    struct Shot *shotInfo;/* 0x38 */
} ShotObj;

typedef struct Shot {
    struct Shot *next;    /* 0x00 */
    s8 slot;              /* 0x04 */
    s8 slotDamaged;       /* 0x05 */
    s8 type;              /* 0x06 */
    s8 flags;             /* 0x07 */
    f32 lifeTime;         /* 0x08 */
    f32 incTime;          /* 0x0c */
    f32 radius;           /* 0x10 */
    f32 vel[3];           /* 0x14 */
    f32 pos[3];           /* 0x20 */
    f32 old[3];           /* 0x2c */
    f32 uvs[3][3];        /* 0x38 */
    Streak *streak;       /* 0x5c */
    ShotObj *obj;         /* 0x60 */
    ShotObj *obj2;        /* 0x64 */
} Shot;

typedef struct ShotCase {
    struct ShotCase *next;
    u32 mbIdx;            /* 0x04 */
    UVMat mat;            /* 0x08 */
    f32 time;             /* 0x38 */
    f32 vel[3];           /* 0x3c */
} ShotCase;

typedef struct HitPoly { struct HitPoly *next; void *poly; } HitPoly;

typedef struct TexDef { u8 b[36]; } TexDef;
typedef struct TexDefSet { TexDef *texDefs; s32 num; } TexDefSet;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInst { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } ObjInst;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

VIEW(ColSize[], collsize);
#define collsize collsize_view
VIEW(f32[], unitmatrix);
#define unitmatrix unitmatrix_view
extern f32 weaponOffsets[8][13][3];/* weaponOffsets */
VIEW(StatLL, gShotObjList);
#define gShotObjList gShotObjList_view
VIEW(Shield[4], gCarShield);
#define gCarShield gCarShield_view
VIEW(StatLL, gShotList);
#define gShotList gShotList_view
VIEW(StatLL, gHitPolyList);
#define gHitPolyList gHitPolyList_view
VIEW(StatLL, gShotCaseList);
#define gShotCaseList gShotCaseList_view
VIEW(s32[10], gWprObjList);
#define gWprObjList gWprObjList_view
VIEW(s32[7], gWfxObjList);
#define gWfxObjList gWfxObjList_view
VIEW(u16[5], gMineTextures);
#define gMineTextures gMineTextures_view
VIEW(TexDefSet[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(ObjInst[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(RGBA, hitColor);
#define hitColor hitColor_view

extern void ZOID_RemovePoly(void *poly);

extern Streak *NewStreak(s32 type, f32 *pos, f32 *pos2);

extern f32 sqrtf(f32);
extern f32 fabsf(f32);

extern ShotObj *CreateShotObject(s32 idx, s32 parent, u32 flags, s32 o2);

extern Car *ShotToCarCollision(Shot *shot, f32 *old);

extern intptr_t ShotToWorldCollision(f32 *old, f32 *pos, f32 *uvs, f32 radius, s32 a, s32 b);

extern void ZOID_SetTexture(s16 obj, void *tex, s32 a);

extern void *ZOID_NewPoly(s32 n, f32 *verts, u16 tidx, RGBA *color, s32 flags, s32 a);
extern void MBOX_FindTexture_WORK(char *name, u16 *out, s8 a, s8 b, s32 c);
extern void SetupTarget(char *name, s32 a, f32 *pos, s32 b, s32 slot, s32 c, s32 d);

static const f32 CaseOffsets[1][3] = { 0.6f, 0.65f, -0.69f };
static const f32 ShotOffsets[10][3] = {
    0.16f, 0.49f, 0.57f, 0.01f, 0.65f, 1.59f, 0.0f, 0.9f, 1.1f, 0.0f, -0.3f, -0.7f,
    0.01f, 2.04f, 0.28f, 0.0f, 0.0f, 0.0f, -0.04f, 0.61f, 0.23f, 0.0f, 1.25f, 0.0f,
    0.0f, 1.5f, 2.0f, 0.0f, 0.0f, 0.0f
};
static f32 gShieldSizes[13] = {
    0.85f, 0.8f, 0.75f, 0.77f, 0.83f, 0.85f, 0.75f, 0.75f, 0.65f, 0.85f, 1.1f, 0.8f, 0.73f
};
static s32 sWeaponFx[10] = { 92, 87, 90, 88, 91, 93, 94, 95, 89, -1 };
/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): a weapon's reload counts down by the time of the 30 Hz frames
 * completed, so it ends on the frame the N64's would: a reload is set afresh at each shot, and counted by the frame
 * time at 60 it ended up to a retrace sooner than the N64's frames of two (0.0666667 s: 5 retraces against 6, a
 * machine gun's 12 shots a second against 10). */
#define PC_DT30(dt) (pc_fps60 ? (pc_frame_steps ? (f32)pc_frame_steps * ((dt) / pc_frame_scale) : 0.0f) : (dt))

/* Turns car `car`'s gun (its yaw and pitch, 0.01 a frame) towards the nearest car of another team in a cone ahead of
 * it within 2000 feet, or back to straight ahead. */
void ElevateGun(Car *car)
{
    Car *hit;
    f32 vec[3];
    f32 pos[3];
    f32 best = 2000.0f;
    f32 yaw = 0;
    f32 pitch = 0.0f;
    f32 h;

    for (hit = PC_CARS; hit < PC_CAR(gNumCars); hit++) {
        if (car->slot == hit->slot || gCarTeam[car->slot] == gCarTeam[hit->slot] ||
            PC_MODEL_CRASHFLAG(hit->slot) || !hit->collidable || hit->dead)
            goto next;
        pos[0] = hit->pos[0] - car->pos[0];
        pos[1] = hit->pos[1] - car->pos[1];
        pos[2] = hit->pos[2] - car->pos[2];
        if (car->battle.type >= 8)
            h = -1.0f * ShotOffsets[car->battle.type][1];
        else
            h = -1.0f * (ShotOffsets[car->battle.type][1] +
                  weaponOffsets[car->battle.type][PC_MODEL_BODY(car->slot)][1]);
        pos[0] = pos[0] + car->uvs[1][0] * h;
        pos[1] = pos[1] + car->uvs[1][1] * h;
        pos[2] = pos[2] + car->uvs[1][2] * h;
        BodyVector(pos, vec, &car->uvs[0][0]);
        if (vec[2] < 0.0f || vec[0] < -0.5f * vec[2] || vec[0] > 0.5f * vec[2] ||
            vec[1] < -vec[2] || vec[1] > vec[2] || vec[2] > best)
            continue;
        best = vec[2];
        yaw = fatan2(vec[0], vec[2]);
        pitch = fatan2(vec[1], vec[2]);
next:;
    }
    if (yaw < car->battle.yaw)
        car->battle.yaw -= PC_FSCALE(0.01f);
    else if (yaw > car->battle.yaw)
        car->battle.yaw += PC_FSCALE(0.01f);
    if (pitch < car->battle.pitch)
        car->battle.pitch -= PC_FSCALE(0.01f);
    else if (pitch > car->battle.pitch)
        car->battle.pitch += PC_FSCALE(0.01f);
}

/* Returns the first car shot `shot` hits on its way from `old` this frame (not its own car, nor one crashed, being
 * put back or not collidable; a car as an upright cylinder 6.75 feet round and 3.5 high), the shot moved to the hit,
 * or NULL. A car carrying weapon 5 turns away shots that hit it from the front (shot flag 1). Mines (type 3) hit
 * nothing here. */
Car *ShotToCarCollision(Shot *shot, f32 *old)
{
    Car *car;
    Car *hit;
    f32 dir[3];
    f32 normal[3];
    f32 delta[3];
    f32 projected[3];
    f32 pos[3];
    f32 pos2[3];
    f32 t;
    f32 d0;
    f32 t0;
    f32 y;
    s32 i;

    if (shot->type == 3)
        return 0;
    dir[0] = shot->pos[0] - old[0];
    dir[1] = shot->pos[1] - old[1];
    dir[2] = shot->pos[2] - old[2];
    normal[0] = -dir[0];
    normal[1] = 0.0f;
    normal[2] = -dir[2];
    d0 = dir[0] * normal[0] + dir[1] * normal[1] + dir[2] * normal[2];
    if (fabsf(d0) < 0.001f)
        return 0;
    t0 = 1.0f;
    hit = 0;
    for (i = 0, car = PC_CARS; i < gNumCars; i++, car++) {
        if (shot->slot == car->slot || PC_MODEL_MOVING_STATE(i) != -1 ||
            !car->collidable || PC_MODEL_CRASHFLAG(i))
            continue;
        delta[0] = car->pos[0] - old[0];
        delta[1] = car->pos[1] - old[1];
        delta[2] = car->pos[2] - old[2];
        t = (delta[0] * normal[0] + delta[1] * normal[1] + delta[2] * normal[2]) / d0;
        if (t < 0.0f || t > t0)
            continue;
        projected[0] = old[0] + dir[0] * t;
        projected[1] = old[1] + dir[1] * t;
        projected[2] = old[2] + dir[2] * t;
        pos[0] = projected[0] - car->pos[0];
        pos[1] = projected[1] - car->pos[1];
        pos[2] = projected[2] - car->pos[2];
        if (pos[1] < -shot->radius || pos[1] > 3.5f + shot->radius)
            continue;
        y = sqrtf(pos[0] * pos[0] + pos[2] * pos[2]);
        if (y > 6.75f + shot->radius - 0.5f)
            continue;
        if (car->battle.type == 5) {
            pos2[0] = old[0] - shot->pos[0];
            pos2[1] = old[1] - shot->pos[1];
            pos2[2] = old[2] - shot->pos[2];
            BodyVector(pos2, pos, &car->uvs[0][0]);
            if (fabsf(fatan2(pos[0], pos[2])) < 1.35f)
                shot->flags |= 1;
        }
        shot->pos[0] = projected[0];
        hit = car;
        t0 = t;
        shot->pos[1] = projected[1];
        shot->pos[2] = projected[2];
    }
    return hit;
}

/* The sonic blast `shot`: every other car within its growing ring is hit, pushed away and damaged once. */
void SonicCarCollision(Shot *shot)
{
    Car *car;
    Car *our;
    Model *m;
    s32 i;
    s32 dam;
    f32 vec[3];
    f32 vec2[3];
    f32 pos[3];
    f32 dist;
    f32 rad;
    f32 radsq;
    f32 mul;
    f32 scale;

    our = PC_CAR(shot->slot);
    scale = ObjInstanceList[shot->obj->mbIdx].scale;
    if (scale <= 8.0f) {
        mul = 1.0f;
        dam = 800;
    } else if (scale <= 20.0f) {
        mul = 0.6f;
        dam = 400;
    } else {
        dam = 200;
        mul = 0.5f;
    }
    pos[0] = shot->obj->mat.pos[0];
    pos[1] = shot->obj->mat.pos[1];
    pos[2] = shot->obj->mat.pos[2];
    for (car = PC_CARS, i = 0; i < gNumCars; i++, car++) {
        if (shot->slot == car->slot || PC_MODEL_MOVING_STATE(i) != -1 ||
            !car->collidable || (shot->slotDamaged & (1 << car->slot)))
            continue;
        vec[0] = car->pos[0] - pos[0];
        vec[1] = car->pos[1] - pos[1];
        vec[2] = car->pos[2] - pos[2];
        dist = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
        if (dist <= (scale * 4.0f) * (scale * 4.0f)) {
            shot->slotDamaged |= 1 << car->slot;
            BattleDamage(our, car, dam);
            rad = sqrtf(dist);
            vec2[2] = mul * (330000.0f * (vec[2] / rad));
            vec2[1] = mul * (330000.0f * (vec[1] / rad) + 66000.0f);
            vec2[0] = mul * (330000.0f * (vec[0] / rad));
            BodyVector(vec2, vec, &car->uvs[0][0]);
            m = PC_MODEL(car->slot);
            m->centerForce[0] = m->centerForce[0] + vec[0];
            m->centerForce[1] = m->centerForce[1] + vec[1];
            m->centerForce[2] = m->centerForce[2] + vec[2];
            vec[1] = 0.0f;
            m->centerMoment[0] = m->centerMoment[0] + vec[0];
            m->centerMoment[1] = m->centerMoment[1] + vec[1];
            m->centerMoment[2] = m->centerMoment[2] + vec[2];
        }
    }
}

/* Ends shot `shot`: its streak and scene objects go and it returns to the free list. */
void ShotDone(Shot *shot)
{
    ShotObj *o;
    s32 idx;
    switch (shot->type) {
    case 3:
        o = shot->obj2;
        idx = o->mbIdx;
        ZOID_RemoveObject(idx);
        FreeStatLLE(&gShotObjList, o);
    case 0:
    case 1:
    case 2:
    case 4:
    case 6:
    case 7:
    case 8:
        o = shot->obj;
        idx = o->mbIdx;
        ZOID_RemoveObject(idx);
        FreeStatLLE(&gShotObjList, o);
        break;
    }
}

/* Makes a scene object of shot model `idx` (under `parent`, with scene flags `flags`; `o2` its second object) in a
 * new shot object of gShotObjList. Returns it, or NULL with none free. */
ShotObj *CreateShotObject(s32 idx, s32 parent, u32 flags, s32 o2)
{
    ShotObj *o;
    o = GetStatLLE(&gShotObjList);
    if (o2)
        o->mbIdx = MBOX_NewObject(gWprObjList[idx], 0, parent, flags);
    else
        o->mbIdx = MBOX_NewObject(gWprObjList[idx], &o->mat, parent, flags);
    return o;
}

/* Moves shot `shot`'s scene objects to its position and orientation (`unt`: the second object too). */
void UpdateShotObject(Shot *shot, s32 unt)
{
    ShotObj *o = shot->obj;
    o->mat.pos[0] = shot->pos[0];
    o->mat.pos[1] = shot->pos[1];
    o->mat.pos[2] = shot->pos[2];
    if (unt == 1)
        CopyUV(unitmatrix, &o->mat.uvs[0][0]);
    else if (unt != 2)
        CopyUV(&shot->uvs[0][0], &o->mat.uvs[0][0]);

    if (shot->flags & 0x70) {
        if ((shot->flags & 0x80) && (shot->flags & 0x10) &&
            (shot->type == 1 || shot->type == 8)) {
reduceScale:
            o->mat.uvs[2][0] *= 0.2f;
            o->mat.uvs[2][1] *= 0.2f;
            o->mat.uvs[2][2] *= 0.2f;
        } else if (shot->type == 0) {
            if (shot->flags & 0x80) {
                goto reduceScale;
            } else {
                o->mat.uvs[2][0] *= 0.4f;
                o->mat.uvs[2][1] *= 0.4f;
                o->mat.uvs[2][2] *= 0.4f;
            }
        }
    }
    if (shot->flags & 2)
        PitchUV(PC_FSCALE(0.18f), &o->mat.uvs[0][0]);
}

/* Car `car`'s (model `m`, slot `i`) defence `defense`: its shield (grown to the car's size, spun, faded out) or
 * its invisibility (the car faded out and back) counts down and ends. */
void UpdateDefense(Car *car, Model *m, Defense *defense, s32 i)
{
    RGBA clr = { { 255, 255, 255, 255 } };
    s32 j, k;

    if ((defense->active & 1) && car->battle.fadeState != 2) {
        defense->invsTime -= gFrameSeconds;
        if (defense->invsTime <= 0.0f || m->crashflag) {
            defense->invsTime = 0.0f;
            car->battle.fadeState = 2;
            car->battle.fadeTime = 0.666667f;
            car->battle.fadeAlpha = 16;
        }
    }
    if (defense->active & 2) {
        if (m->crashflag)
            goto removeShield;
        gCarShield[i].mat.pos[0] = car->pos[0];
        gCarShield[i].mat.pos[1] = car->pos[1];
        gCarShield[i].mat.pos[2] = car->pos[2];
        CopyUV(&car->uvs[0][0], &gCarShield[i].mat.uvs[0][0]);
        PitchUV(defense->unknown10, &gCarShield[i].mat.uvs[0][0]);
        defense->unknown10 -= PC_FSCALE(0.005f);
        defense->shldTime -= gFrameSeconds;
        if (defense->shldTime <= 0.0f) {
            if (defense->active & 8) {
                if (defense->shldScale >= gShieldSizes[m->body_type]) {
                    defense->shldScale = gShieldSizes[m->body_type];
                    defense->active &= ~8;
                    defense->active |= 4;
                    defense->shldTime = 30.0f;
                } else {
                    defense->shldScale += 0.05f;
                    if (defense->shldScale > gShieldSizes[m->body_type])
                        defense->shldScale = gShieldSizes[m->body_type];
                    defense->shldTime = 0.0333333f;
                }
            } else if (defense->active & 16) {
                if (defense->shldScale <= 0.06f) {
removeShield:
                    defense->shldTime = 0.0f;
                    defense->active &= ~0x1e;
                    if (gCarShield[i].dlIdx != -1) {
                        ZOID_RemoveObject(gCarShield[i].dlIdx);
                        gCarShield[i].dlIdx = -1;
                    }
                    return;
                } else {
                    defense->shldScale -= 0.05f;
                    defense->shldTime = 0.0333333f;
                }
            } else {
                defense->active &= ~4;
                defense->active |= 16;
                defense->shldTime = 0.0333333f;
            }
        }
        for (j = 0; j < 3; j++)
            for (k = 0; k < 3; k++)
                gCarShield[i].mat.uvs[j][k] *= defense->shldScale;
        if (gCarShield[i].dlIdx == -1)
            gCarShield[i].dlIdx = MBOX_NewObject(gWfxObjList[5], &gCarShield[i].mat, -1, 0x802000);
        if (car->battle.defense.active & 1) {
            clr.c[3] = car->battle.fadeAlpha;
            if (clr.c[3] < 48)
                clr.c[3] = 48;
        }
        ZOID_SetColor(gCarShield[i].dlIdx, &clr);
    }
}

/* One frame of every shot: it flies (bullets straight, missiles homing on their lock, mines falling to the road),
 * hits the track (a scorch mark) or a car (BattleDamage, the hit's sound and sparks), or times out (ShotDone). */
void UpdateShots(void)
{
    s32 padx;
    Shot *shot, *nexts;
    HitPoly *h;
    ShotObj *o;
    Car *car, *hit;
    Model *m;
    f32 old[3];
    f32 vel;
    f32 off[3];
    s32 pady;
    RGBA clr = { { 255, 255, 255, 255 } };
    f32 verts[4][3];
    f32 scale;
    f32 radius;
    f32 uvs[3][3];
    s32 damage;
    f32 side[3];
    f32 v1[3];

    for (shot = gShotList.head; shot; shot = nexts) {
        nexts = shot->next;
        if (shot->type == 3) {
            car = PC_CAR(shot->slot);
            m = PC_MODEL(shot->slot);
            if (shot->incTime == 0) {
                shot->pos[0] = car->pos[0];
                shot->pos[1] = car->pos[1];
                shot->pos[2] = car->pos[2];
                off[0] = ShotOffsets[3][0] + weaponOffsets[3][m->body_type][0];
                off[1] = ShotOffsets[3][1] + weaponOffsets[3][m->body_type][1];
                off[2] = ShotOffsets[3][2] + weaponOffsets[3][m->body_type][2];
                shot->pos[0] = shot->pos[0] + car->uvs[2][0]*off[2];
                shot->pos[1] = shot->pos[1] + car->uvs[2][1]*off[2];
                shot->pos[2] = shot->pos[2] + car->uvs[2][2]*off[2];
                shot->pos[0] = shot->pos[0] + car->uvs[1][0]*off[1];
                shot->pos[1] = shot->pos[1] + car->uvs[1][1]*off[1];
                shot->pos[2] = shot->pos[2] + car->uvs[1][2]*off[1];
                shot->pos[0] = shot->pos[0] + car->uvs[0][0]*off[0];
                shot->pos[1] = shot->pos[1] + car->uvs[0][1]*off[0];
                shot->pos[2] = shot->pos[2] + car->uvs[0][2]*off[0];
                CopyUV(&car->uvs[0][0], &shot->uvs[0][0]);
                PitchUV(2.0734513f, &shot->uvs[0][0]);
                if (car->battle.loaded == 1) {
                    if (car->battle.type != 3 || m->crashflag || !car->collidable || car->dead) {
                        car->battle.loaded = 0;
                        FreeStatLLE(&gShotList, shot);
                        continue;
                    }
                    car->battle.loaded = 2;
                    if (!shot->obj) {
                        clr.rgba = car->IRclr.rgba;
                        if (car->battle.defense.active & 1) {
                            clr.c[3] = car->battle.fadeAlpha;
                            if (clr.c[3] < 48)
                                clr.c[3] = 48;
                        }
                        shot->obj = CreateShotObject(4, -1, 0x2000, 0);
                        ZOID_SetColor(shot->obj->mbIdx, &clr);
                        shot->obj2 = CreateShotObject(5, shot->obj->mbIdx, 0x2080, 1);
                        h = (HitPoly *)shot->obj2;
                        ZOID_SetColor(((ShotObj *)h)->mbIdx, &clr);
                        ZOID_SetTexture(((ShotObj *)h)->mbIdx,
                            &gTexDefPtrs[gMineTextures[gCarTeam[shot->slot]] >> 10].texDefs[gMineTextures[gCarTeam[shot->slot]] & 1023], -1);
                    }
                    shot->radius = 0.0f;
                }
                if (car->battle.loaded == 2) {
                    if (car->battle.type != 3 || m->crashflag || !car->collidable || car->dead) {
                        car->battle.loaded = 0;
                        ShotDone(shot);
                        FreeStatLLE(&gShotList, shot);
                        continue;
                    }
                    o = shot->obj;
                    o->mat.pos[0] = shot->pos[0];
                    o->mat.pos[1] = shot->pos[1];
                    o->mat.pos[2] = shot->pos[2];
                    CopyUV(&shot->uvs[0][0], &o->mat.uvs[0][0]);
                    clr.rgba = car->IRclr.rgba;
                    if (car->battle.defense.active & 1) {
                        clr.c[3] = car->battle.fadeAlpha;
                        if (clr.c[3] < 48)
                            clr.c[3] = 48;
                    }
                    o = shot->obj;
                    ZOID_SetColor(o->mbIdx, &clr);
                    o = shot->obj2;
                    ZOID_SetColor(o->mbIdx, &clr);
                    if (car->player && (car->player->edges & car->player->controls[9])) {
                        clr.c[3] = 255;
                        o = shot->obj;
                        ZOID_SetColor(o->mbIdx, &clr);
                        o = shot->obj2;
                        ZOID_SetColor(o->mbIdx, &clr);
                        if (car->battle.ammoCnt > 0)
                            car->battle.ammoCnt--;
                        car->battle.shoot = 6;
                        PlayGameSound(sWeaponFx[3], shot->slot, 1, 1);
                        car->battle.reloadingTime = 1.0f;
                        shot->vel[0] = car->vel[0] + 15.0f*shot->uvs[1][0];
                        shot->vel[1] = car->vel[1] + 15.0f*shot->uvs[1][1];
                        shot->vel[2] = car->vel[2] + 15.0f*shot->uvs[1][2];
                        car->battle.loaded = 0;
                        shot->lifeTime = 5.0f;
                    } else {
                        o = shot->obj;
                        ScaleUV(&o->mat.uvs[0][0], &o->mat.uvs[0][0], .5f);
                        continue;
                    }
                }
            }
        }
        shot->lifeTime -= gFrameSeconds;
        shot->incTime += gFrameSeconds;
        if (shot->lifeTime <= 0) {
            ShotDone(shot);
            FreeStatLLE(&gShotList, shot);
            continue;
        }
        car = PC_CAR(shot->slot);
        old[0] = shot->pos[0];
        old[1] = shot->pos[1];
        old[2] = shot->pos[2];
        radius = shot->radius;
        switch (shot->type) {
        case 3:
            damage = 960;
            vel = shot->vel[2]*gFrameSeconds;
            scale = 10.0f;
            shot->vel[1] -= gGravity*gFrameSeconds;
            shot->vel[0] *= PC_FILTER(0.9f);
            shot->vel[2] *= PC_FILTER(0.9f);
            shot->pos[0] = old[0] + shot->vel[0]*gFrameSeconds;
            shot->pos[1] = old[1] + shot->vel[1]*gFrameSeconds;
            shot->pos[2] = old[2] + shot->vel[2]*gFrameSeconds;
            break;
        case 2:
            scale = 10.0f;
            shot->vel[1] -= (gGravity*gFrameSeconds)*2.5f;
            radius = 0.5f;
            damage = 800;
            shot->pos[0] = old[0] + shot->vel[0]*gFrameSeconds;
            shot->pos[1] = old[1] + shot->vel[1]*gFrameSeconds;
            shot->pos[2] = old[2] + shot->vel[2]*gFrameSeconds;
            break;
        case 4:
            scale = 12.0f;
            damage = 960;
            MissileLock(shot);
            shot->vel[2] += (100.0f - shot->vel[2])*gFrameSeconds;
            vel = shot->vel[2]*gFrameSeconds;
            goto straightShot;
        case 6:
            damage = 520;
            shot->vel[0] = shot->vel[0] + shot->uvs[2][0]*(800.0f*gFrameSeconds);
            shot->vel[1] = shot->vel[1] + shot->uvs[2][1]*(800.0f*gFrameSeconds);
            shot->vel[2] = shot->vel[2] + shot->uvs[2][2]*(800.0f*gFrameSeconds);
            scale = 8.0f;
            shot->pos[0] = old[0] + shot->vel[0]*gFrameSeconds;
            shot->pos[1] = old[1] + shot->vel[1]*gFrameSeconds;
            shot->pos[2] = old[2] + shot->vel[2]*gFrameSeconds;
            break;
        case 0:
            damage = 400;
            scale = 10.0f;
            vel = 1000.0f*gFrameSeconds;
            goto straightShot;
        case 1:
            damage = 114;
            scale = 3.0f;
            vel = 2000.0f*gFrameSeconds;
            goto straightShot;
        case 7:
            damage = 66;
            break;
        case 8:
        default:
            damage = 80;
            scale = 3.0f;
            vel = 2000.0f*gFrameSeconds;
straightShot:
            shot->pos[0] = old[0] + shot->uvs[2][0]*vel;
            shot->pos[1] = old[1] + shot->uvs[2][1]*vel;
            shot->pos[2] = old[2] + shot->uvs[2][2]*vel;
            break;
        }
        if (shot->streak) {
            shot->streak->flags |= 0x10;
            shot->streak->newpos[0] = shot->pos[0];
            shot->streak->newpos[1] = shot->pos[1];
            shot->streak->newpos[2] = shot->pos[2];
        }
        if (shot->type != 7) {
            if (shot->type == 4 || shot->type == 6)
                hit = ShotToCarCollision(shot, shot->old);
            else
                hit = ShotToCarCollision(shot, old);
            if (hit)
                shot->flags |= 0x80;
            if (ShotToWorldCollision(old, shot->pos, &uvs[0][0], radius, 0, 65535)) {
                shot->flags |= 0x80;
                switch (shot->type) {
                case 3:
                    if (shot->flags & 2) {
                        if (fabsf(shot->vel[0]) < 10.f && fabsf(shot->vel[2]) < 10.f) {
                            shot->lifeTime = 0.0000001f;
                            SetupTarget("WPR_MINE", -2, shot->pos, 0, shot->slot, 1, 0);
                            break;
                        } else {
                            BodyVector(shot->vel, v1, &uvs[0][0]);
                            v1[1] = 0.4f*-v1[1];
                            WorldVector(v1, shot->vel, &uvs[0][0]);
                        }
                    } else {
                        shot->flags |= 2;
                        BodyVector(shot->vel, v1, &uvs[0][0]);
                        v1[1] = 15.0f;
                        WorldVector(v1, shot->vel, &uvs[0][0]);
                    }
                    break;
                case 4: case 6:
                    ProximityDamage(shot->pos, shot->old, shot->slot, 900.0f, damage);
                    StartBlast(shot->pos, 0, 0.5f, 1);
                    shot->lifeTime = 0.0000001f;
                    if (shot->streak)
                        shot->streak->flags |= 0x1000;
                    break;
                case 2:
                    if (hit || shot->lifeTime <= gFrameSeconds)
                        goto carHit;
                    BodyVector(shot->vel, v1, &uvs[0][0]);
                    v1[1] = 0.9f*-v1[1];
                    WorldVector(v1, shot->vel, &uvs[0][0]);
                    EmitGameSound(shot->pos, &unitmatrix[9], 400.0f, 0.0f, 1.0f, 0.0f, (s32)(70.0f + Random(3.0f)), shot->slot, 0, 0x80);
                    break;
                default:
wallHit:
                    shot->lifeTime = 0.0000001f;
                    {
                        side[0] = shot->uvs[0][0]*scale;
                        side[1] = shot->uvs[0][1]*scale;
                        side[2] = shot->uvs[0][2]*scale;
                        v1[0] = shot->uvs[1][0]*scale;
                        v1[1] = shot->uvs[1][1]*scale;
                        v1[2] = shot->uvs[1][2]*scale;
                        verts[0][0] = shot->pos[0] - side[0];
                        verts[0][1] = shot->pos[1] - side[1];
                        verts[0][2] = shot->pos[2] - side[2];
                        verts[1][0] = shot->pos[0] + side[0];
                        verts[1][1] = shot->pos[1] + side[1];
                        verts[1][2] = shot->pos[2] + side[2];
                        verts[2][0] = shot->pos[0] + side[0];
                        verts[2][1] = shot->pos[1] + side[1];
                        verts[2][2] = shot->pos[2] + side[2];
                        verts[3][0] = shot->pos[0] - side[0];
                        verts[3][1] = shot->pos[1] - side[1];
                        verts[3][2] = shot->pos[2] - side[2];
                        verts[0][0] -= v1[0];
                        verts[0][1] -= v1[1];
                        verts[0][2] -= v1[2];
                        verts[1][0] -= v1[0];
                        verts[1][1] -= v1[1];
                        verts[1][2] -= v1[2];
                        verts[2][0] = verts[2][0] + v1[0];
                        verts[2][1] = verts[2][1] + v1[1];
                        verts[2][2] = verts[2][2] + v1[2];
                        verts[3][0] = verts[3][0] + v1[0];
                        verts[3][1] = verts[3][1] + v1[1];
                        verts[3][2] = verts[3][2] + v1[2];
                    }
                    if (!wallTidx)
                        MBOX_FindTexture_WORK("HIT_WALL", &wallTidx, 0, gNumModels - 1, 1);
                    h = GetStatLLE(&gHitPolyList);
                    h->poly = ZOID_NewPoly(4, &verts[0][0], wallTidx, &hitColor, 0x20f, 1);
                    if (!h->poly)
                        FreeStatLLE(&gHitPolyList, h);
                    break;
                }
            } else if (hit) {
carHit:
                shot->lifeTime = 0.0000001f;
                if (shot->type == 2 || shot->type == 4 || shot->type == 6) {
                    if (shot->streak)
                        shot->streak->flags |= 0x1000;
                    if (shot->type == 4 || shot->type == 6)
                        ProximityDamage(shot->pos, shot->old, shot->slot, 900.0f, damage);
                    else
                        ProximityDamage(shot->pos, old, shot->slot, 900.0f, damage);
                    StartBlast(shot->pos, 0, 0.5f, 1);
                } else {
                    if (shot->flags & 1)
                        damage *= 0.35f;
                    BattleDamage(car, hit, damage);
                    goto wallHit;
                }
            } else if (shot->lifeTime <= gFrameSeconds) {
                if (shot->type == 2 || shot->type == 4 || shot->type == 6) {
                    if (shot->type == 4 || shot->type == 6)
                        ProximityDamage(shot->pos, shot->old, shot->slot, 900.0f, damage);
                    else
                        ProximityDamage(shot->pos, old, shot->slot, 900.0f, damage);
                    StartBlast(shot->pos, 0, 0.5f, 1);
                    if (shot->streak)
                        shot->streak->flags |= 0x1000;
                    shot->lifeTime = 0.0000001f;
                }
            }
        } else if (shot->obj) {
            SonicCarCollision(shot);
            shot->obj->mat.pos[0] = car->pos[0];
            shot->obj->mat.pos[1] = car->pos[1];
            shot->obj->mat.pos[2] = car->pos[2];
            if (shot->incTime >= 0.0333333f) {
                shot->incTime -= 0.0333333f;
                ObjInstanceList[shot->obj->mbIdx].scale += 1.0f;
                shot->obj->mat.uvs[0][0] = ObjInstanceList[shot->obj->mbIdx].scale;
                shot->obj->mat.uvs[1][1] = ObjInstanceList[shot->obj->mbIdx].scale;
                shot->obj->mat.uvs[2][2] = ObjInstanceList[shot->obj->mbIdx].scale;
            }
        }
        if (!(shot->flags & 0x70)) {
            shot->old[0] = old[0];
            shot->old[1] = old[1];
            shot->old[2] = old[2];
        }
        switch (shot->type) {
        case 0:
            if (!shot->obj)
                shot->obj = CreateShotObject(2, -1, 0, 0);
            UpdateShotObject(shot, 0);
            break;
        case 2:
            if (!shot->obj)
                shot->obj = CreateShotObject(3, -1, 0, 0);
            UpdateShotObject(shot, 0);
            break;
        case 3:
            UpdateShotObject(shot, 2);
            break;
        case 4:
            if (!shot->obj)
                shot->obj = CreateShotObject(7, -1, 0, 0);
            UpdateShotObject(shot, 0);
            break;
        case 6:
            if (!shot->obj)
                shot->obj = CreateShotObject(8, -1, 0, 0);
            UpdateShotObject(shot, 0);
            break;
        case 7:
            if (!shot->obj) {
                shot->obj = CreateShotObject(9, -1, 0x800000, 0);
                UpdateShotObject(shot, 1);
            }
            break;
        case 1: case 8:
            if (!shot->obj)
                shot->obj = CreateShotObject(1, -1, 0, 0);
            UpdateShotObject(shot, 0);
            break;
        default:
            break;
        }
        if (shot->flags & 0x10) {
            PC_EACH30(shot->flags &= ~0x10;)
            PC_EACH30(shot->flags |= 0x20;)
        } else if (shot->flags & 0x20) {
            PC_EACH30(shot->flags &= ~0x20;)
            PC_EACH30(shot->flags |= 0x40;)
        } else if (shot->flags & 0x40) {
            PC_EACH30(shot->flags &= ~0x40;)
        }
    }
}

/* Fills in new shot `shot` fired by car `car`: its type, owner, time, position at the gun and velocity (MAT3
 * _InitNewShot). */
void InitNewShot(Shot *shot, Car *car)
{
    shot->slotDamaged = 0;
    shot->obj = 0;
    shot->obj2 = 0;
    shot->flags = 0x10;
    shot->type = car->battle.type;
    shot->slot = car->slot;
    shot->incTime = 0;
    shot->vel[2] = 0.0f;
    shot->vel[1] = 0.0f;
    shot->vel[0] = 0.0f;
    shot->pos[0] = car->pos[0];
    shot->pos[1] = car->pos[1];
    shot->pos[2] = car->pos[2];
    shot->old[0] = car->pos[0];
    shot->old[1] = car->pos[1];
    shot->old[2] = car->pos[2];
    CopyUV(&car->uvs[0][0], &shot->uvs[0][0]);
}

/* One frame of the spent shell cases: each falls and spins, and goes after its time. */
void AnimateCase(void)
{
    ShotCase *casing, *next;
    s32 idx;

    for (casing = gShotCaseList.head; casing; casing = next) {
        next = casing->next;
        casing->time -= gFrameSeconds;
        if (casing->time <= 0) {
            idx = casing->mbIdx;
            ZOID_RemoveObject(idx);
            FreeStatLLE(&gShotCaseList, casing);
        } else {
            casing->vel[1] -= (gGravity * gFrameSeconds) * 2.5f;
            casing->mat.pos[0] = casing->mat.pos[0] + casing->vel[0] * gFrameSeconds;
            casing->mat.pos[1] = casing->mat.pos[1] + casing->vel[1] * gFrameSeconds;
            casing->mat.pos[2] = casing->mat.pos[2] + casing->vel[2] * gFrameSeconds;
        }
    }
}

/* One frame of the battle's weapons (PlayGame): each car's gun aim, its firing (a new shot of its weapon when it
 * shoots and has ammunition, the muzzle flash and the sound), its defence; then the shots and the cases. */
void ShotUpdate(void)
{
    s32 i;
    s32 StartStreak;
    s32 streaktype;
    Shot *shot;
    ShotCase *h;
    HitPoly *hp;
    Car *car;
    Model *m;
    ColSize *sizep;
    f32 z;
    f32 off[3];

    AnimateCase();
    for (i = 0, car = PC_CARS; i < gNumCars; i++, car++) {
        m = PC_MODEL(car->slot);
        if (car->battle.defense.active)
            UpdateDefense(car, m, &car->battle.defense, car->slot);
        switch (car->battle.type) {
        case 0: case 1: case 6: case 8:
            ElevateGun(car);
            break;
        }
        if (car->player->edges & car->player->controls[6])
            goto resetWeapon;
        if (!car->battle.ammoCnt) {
            if (car->battle.type == 5) {
                sizep = &collsize[m->body_type];
                m->bodyR[0][2] = m->bodyR[1][2] = sizep->colfront;
                z = m->bodyR[0][2] > sizep->colrear ? m->bodyR[0][2] : sizep->colrear;
                m->colrad = sqrtf(z*z + sizep->colside*sizep->colside + sizep->colheight*sizep->colheight);
            }
resetWeapon:
            car->battle.type = 8;
            car->battle.ammoCnt = -1;
            car->battle.pitch = 0.0f;
            car->battle.yaw = 0.0f;
        }
        if (car->battle.reloadingTime > 0.0f)
            car->battle.reloadingTime -= PC_DT30(gFrameSeconds);
        if (car->battle.reloadingTime <= 0.0f && car->battle.ammoCnt) {
            if (car->battle.type == 3) {
                if (car->battle.loaded)
                    continue;
                car->battle.loaded = 1;
            } else {
                if (!car->player || car->battle.type == 5 ||
                    (!(car->player->edges & car->player->controls[9]) &&
                     (car->battle.type != 1 || !(car->player->levels & car->player->controls[9]))) ||
                    m->crashflag || !car->collidable || car->dead)
                    continue;
                car->battle.shoot = 6;
            }
            shot = GetStatLLE(&gShotList);
            if (!shot)
                continue;
            if (gShotList.head) {}
            InitNewShot(shot, car);
            if (shot->type != 3)
                PlayGameSound(sWeaponFx[car->battle.type], car->slot, 1, 1);
            StartStreak = 0;
            switch (shot->type) {
            case 2:
                StartStreak = 1;
                streaktype = 1;
                PitchUV(.5f, &shot->uvs[0][0]);
                car->battle.reloadingTime = 0.1666667f;
                shot->lifeTime = 3.1666667f;
                shot->radius = 15.0f;
                shot->vel[0] = car->vel[0] + 75.0f*shot->uvs[2][0];
                shot->vel[1] = car->vel[1] + 75.0f*shot->uvs[2][1];
                shot->vel[2] = car->vel[2] + 75.0f*shot->uvs[2][2];
                break;
            case 4:
                StartStreak = 1;
                streaktype = 0;
                car->battle.reloadingTime = 1.0f;
                shot->lifeTime = 15.0f;
                shot->radius = 2.0f;
                shot->vel[2] = 165.0f + (car->vel[0]*shot->uvs[2][0] + car->vel[1]*shot->uvs[2][1] + car->vel[2]*shot->uvs[2][2]);
                break;
            case 6:
                StartStreak = 1;
                streaktype = 2;
                car->battle.reloadingTime = 0.1666667f;
                shot->lifeTime = 10.0f;
                shot->radius = 1.f;
                PitchUV(car->battle.pitch, &shot->uvs[0][0]);
                shot->vel[0] = car->vel[0] + 50.0f*shot->uvs[2][0];
                shot->vel[1] = car->vel[1] + 50.0f*shot->uvs[2][1];
                shot->vel[2] = car->vel[2] + 50.0f*shot->uvs[2][2];
                break;
            case 8:
                car->battle.reloadingTime = 0.1666667f;
                shot->lifeTime = 4.0f;
                shot->radius = 0.5f;
                YawUV(-car->battle.yaw, &shot->uvs[0][0]);
                PitchUV(car->battle.pitch, &shot->uvs[0][0]);
                break;
            case 0:
                car->battle.reloadingTime = 0.1666667f;
                shot->lifeTime = 4.0f;
                shot->radius = 0.50f;
                PitchUV(car->battle.pitch, &shot->uvs[0][0]);
                break;
            case 1:
                car->battle.reloadingTime = 0.0666667f;
                shot->lifeTime = 4.0f;
                shot->radius = 0.25f;
                YawUV(Random(0.025f) - 0.0125f - car->battle.yaw, &shot->uvs[0][0]);
                PitchUV(car->battle.pitch + (Random(0.025f) - 0.0125f), &shot->uvs[0][0]);
                h = GetStatLLE(&gShotCaseList);
                if (h) {
                    off[0] = weaponOffsets[1][m->body_type][0] + CaseOffsets[0][0];
                    off[1] = weaponOffsets[1][m->body_type][1] + CaseOffsets[0][1];
                    off[2] = weaponOffsets[1][m->body_type][2] + CaseOffsets[0][2];
                    h->mat.pos[0] = shot->pos[0] + car->uvs[2][0]*off[2];
                    h->mat.pos[1] = shot->pos[1] + car->uvs[2][1]*off[2];
                    h->mat.pos[2] = shot->pos[2] + car->uvs[2][2]*off[2];
                    h->mat.pos[0] = h->mat.pos[0] + car->uvs[1][0]*off[1];
                    h->mat.pos[1] = h->mat.pos[1] + car->uvs[1][1]*off[1];
                    h->mat.pos[2] = h->mat.pos[2] + car->uvs[1][2]*off[1];
                    h->mat.pos[0] = h->mat.pos[0] + car->uvs[0][0]*off[0];
                    h->mat.pos[1] = h->mat.pos[1] + car->uvs[0][1]*off[0];
                    h->mat.pos[2] = h->mat.pos[2] + car->uvs[0][2]*off[0];
                    CopyUV(&car->uvs[0][0], &h->mat.uvs[0][0]);
                    RollUV(0.3f + Random(0.23f), &h->mat.uvs[0][0]);
                    h->mbIdx = MBOX_NewObject(gWprObjList[0], &h->mat, -1, 0);
                    h->time = .5f + Random(0.3f);
                    h->vel[0] = car->vel[0] + 28.0f*h->mat.uvs[1][0];
                    h->vel[1] = car->vel[1] + 28.0f*h->mat.uvs[1][1];
                    h->vel[2] = car->vel[2] + 28.0f*h->mat.uvs[1][2];
                    h->vel[2] *= 0.88f;
                    h->vel[1] *= 0.88f;
                    h->vel[0] *= 0.88f;
                }
                break;
            case 7:
                car->battle.reloadingTime = 1.0f;
                shot->lifeTime = 1.0f;
                shot->radius = 2.0f;
                break;
            }
            {
                if (car->battle.type >= 8) {
                    off[0] = ShotOffsets[car->battle.type][0];
                    off[1] = ShotOffsets[car->battle.type][1];
                    off[2] = ShotOffsets[car->battle.type][2];
                } else {
                    off[0] = ShotOffsets[car->battle.type][0] + weaponOffsets[car->battle.type][m->body_type][0];
                    off[1] = ShotOffsets[car->battle.type][1] + weaponOffsets[car->battle.type][m->body_type][1];
                    off[2] = ShotOffsets[car->battle.type][2] + weaponOffsets[car->battle.type][m->body_type][2];
                }
                shot->pos[0] = shot->pos[0] + car->uvs[2][0]*off[2];
                shot->pos[1] = shot->pos[1] + car->uvs[2][1]*off[2];
                shot->pos[2] = shot->pos[2] + car->uvs[2][2]*off[2];
                shot->pos[0] = shot->pos[0] + car->uvs[1][0]*off[1];
                shot->pos[1] = shot->pos[1] + car->uvs[1][1]*off[1];
                shot->pos[2] = shot->pos[2] + car->uvs[1][2]*off[1];
                shot->pos[0] = shot->pos[0] + car->uvs[0][0]*off[0];
                shot->pos[1] = shot->pos[1] + car->uvs[0][1]*off[0];
                shot->pos[2] = shot->pos[2] + car->uvs[0][2]*off[0];
            }
            if (StartStreak)
                shot->streak = NewStreak(streaktype, shot->pos, shot->pos);
            else
                shot->streak = 0;
            if (car->battle.ammoCnt > 0 && car->battle.type != 5 && car->battle.type != 3)
                car->battle.ammoCnt--;
        }
    }
    for (hp = gHitPolyList.head; hp; hp = (HitPoly *)shot) {
        shot = (Shot *)hp->next;
        ZOID_RemovePoly(hp->poly);
        FreeStatLLE(&gHitPolyList, hp);
    }
    UpdateShots();
    UpdateStreaks();
}

