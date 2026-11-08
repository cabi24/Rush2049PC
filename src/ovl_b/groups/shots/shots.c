/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Runtime image B: the shots.c cluster.
 * Direct typed port of MAT3 GameSource/src/shots.c (reference/repos/mat3, GameCube addresses in the MAT3 comments):
 * ShotToCarCollision 0x8003d108, SonicCarCollision 0x8003d4c4, ShotDone 0x8003da24, _CreateShotObject 0x8003dad0,
 * _UpdateShotObject 0x8003db80, _InitNewShot 0x8003dcb4, UpdateDefense 0x8003dd4c, UpdateShots 0x8003e078,
 * ShotUpdate 0x8003f0f4, ElevateGun 0x8003cb84. Field names from MAT3 rush_types.h, offsets from the N64 code.
 * One -O3 uld -kp unit: the static helpers keep IDO's private register conventions only with their real callers.
 * N64 differences found while matching:
 *  - gFrameSeconds, gNumCars and gNumModels are volatile (per-use lui/addiu address);
 *  - PlayGameSound returns int (as MAT3 declares it);
 *  - gShieldSizes is a plain (non-const) static as in MAT3: retail keeps it in .data (0x80394358);
 *  - ShotUpdate: AnimateCase is written inline; _InitNewShot is a real function inlined by umerge (its deleted
 *    stub is InitNewShot); `incTime = 0` (int spelling) and vel[] zeroed 2,1,0; UpdateShots: one pointer variable
 *    `o = shot->obj` / `o = shot->obj2` re-read before every ZOID_SetColor in the loaded==2 block, an explicit
 *    `default: break;` in the final switch, case 3 of the motion switch assigns the (dead) `vel = shot->vel[2]
 *    * gFrameSeconds` (case 4's line), an explicit `break;` after SetupTarget in the collision switch's case 3,
 *    and EmitGameSound's id is `70.0f + Random(3.0f)` (N64 sound ids differ from the GameCube's 21).
 *  - UpdateShots frame residual (disclosed): `padx` (first local) and `pady` (after off[3]) are never accessed;
 *    retail's homes at sp+284 and sp+224 are unused. Without them the frame is -280 instead of -288 and every
 *    home shifts (54 words). Two 4-byte slots, nothing else.
 *  - ElevateGun: the first (5-way) skip test is `goto next;` with `next:;` closing the loop body (the second
 *    test keeps `continue`; the label's extra block sets retail's register priorities), yaw declared before
 *    pitch (`yaw = 0`, `pitch = 0.0f`), `pos[i] = pos[i] + uvs[1][i] * h`.
 *  - DISCLOSED LEVER: `if (gShotList.head) {}` before the _InitNewShot call in ShotUpdate is a compiled-out read
 *    (no code): it adds the block that keeps ShotUpdate's callee-saved cost at retail's value and the gShotList
 *    address reference that orders the case-1 colouring. No natural source form for it has been found.
 * superseding revision: claims the deleted-inline stub AnimateCase = AnimateCase (MAT3 shots.c
 * static, called first by ShotUpdate; post-order places its 8-byte stub immediately before ShotUpdate).
 * The casing loop moves verbatim out of ShotUpdate into the helper (non-static, not on keep, named after the stub;
 * locals casing, next and the s32 `idx` carrier that keeps retail's `lw; sll; sra; move` argument path).
 * Frame accounting (spill.sh, cfe locals area): installed 88; helper inlined as-is 96 (umerge's 8-byte inline-call
 * area plus the helper's locals rounded to 8 replace the 12 root bytes of casing/next/idx), every home +8
 * (47/764). Retail's root therefore has 13 words of named locals: i (sp+204), StartStreak, streaktype
 * (sp+196), seven 4-byte locals, then off[3] (sp+156..164). This revision gets there with two SHAPING changes
 * (disclosed; text otherwise unchanged): (1) `defense` is not a local: UpdateDefense's test and argument use
 * `car->battle.defense` directly (code-identical); (2) the hit-poly cleanup loop keeps its successor in the dead
 * `shot` pointer (`shot = (Shot *)hp->next`) instead of a separate nexth (shot is not read after the main loop).
 * `z` (case 5) and `sizep` are function-scope locals declared before off[3]. Result: area 88, frame 208, all eleven
 * members text-identical (variant, h/shot reused for hp/nexth, is equally exact).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef union RGBA { u8 c[4]; u32 rgba; } RGBA;
typedef struct UVMat { f32 uvs[3][3]; f32 pos[3]; } UVMat;
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

typedef struct Shield { s32 dlIdx; UVMat mat; } Shield;
typedef struct TexDef { u8 b[36]; } TexDef;
typedef struct TexDefSet { TexDef *texDefs; s32 num; } TexDefSet;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInst { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } ObjInst;

extern volatile f32 gFrameSeconds;
extern f32 gGravity;
extern volatile s16 gNumCars;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gCarTeam[];         /* gCarTeam */
extern ColSize collsize[];    /* collsize */
extern f32 unitmatrix[];        /* unitmatrix */
extern f32 weaponOffsets[8][13][3];/* weaponOffsets */
extern StatLL gShotObjList;
extern Shield gCarShield[4];    /* gCarShield */
extern StatLL gShotList;
extern StatLL gHitPolyList;
extern StatLL gShotCaseList;
extern s32 gWprObjList[10];      /* gWprObjList */
extern s32 gWfxObjList[7];       /* gWfxObjList */
extern u16 gMineTextures[5];       /* gMineTextures */
extern TexDefSet gTexDefPtrs[];  /* gTexDefPtrs */
extern ObjInst ObjInstanceList[];    /* ObjInstanceList */
extern u16 wallTidx;
extern volatile u8 gNumModels;
extern RGBA hitColor;

extern void *GetStatLLE(void *list);
extern void FreeStatLLE(void *list, void *e);
extern void ZOID_RemoveObject(s16 obj);
extern void ZOID_SetColor(s16 obj, RGBA *clr);
extern void ZOID_RemovePoly(void *poly);
extern s32 MBOX_NewObject(s32 obj, void *mat, s32 parent, u32 flags);
extern void CopyUV(f32 *src, f32 *dst);
extern void func_80090F44(f32 a, f32 *uvs);     /* PitchUV */
extern void YawUV(f32 a, f32 *uvs);
extern void func_8009EA68(f32 a, f32 *uvs);     /* RollUV */
extern f32 Random(f32 scale);
extern f32 fatan2(f32 y, f32 x);
extern void BodyVector(f32 *v, f32 *out, f32 *uvs);
extern s32 PlayGameSound(s32 fx, s32 slot, s32 a, s32 b);
extern Streak *NewStreak(s32 type, f32 *pos, f32 *pos2);
extern void UpdateStreaks(void);
extern f32 sqrtf(f32);
extern f32 fabsf(f32);
#pragma intrinsic(sqrtf)
#pragma intrinsic(fabsf)
extern void UpdateShotObject(Shot *shot, s32 unt);
extern ShotObj *CreateShotObject(s32 idx, s32 parent, u32 flags, s32 o2);
extern void ShotDone(Shot *shot);
extern Car *ShotToCarCollision(Shot *shot, f32 *old);
extern void SonicCarCollision(Shot *shot);
extern void UpdateShots(void);
void InitNewShot(Shot *shot, Car *car);       /* _InitNewShot */
extern void BattleDamage(Car *car, Car *hit, s32 damage);
extern void ProximityDamage(f32 *pos, f32 *old, s32 owner, f32 radiussq, s32 damage);
extern void MissileLock(Shot *shot);
extern intptr_t ShotToWorldCollision(f32 *old, f32 *pos, f32 *uvs, f32 radius, s32 a, s32 b);
extern void WorldVector(f32 *v, f32 *out, f32 *uvs);
extern void ZOID_SetTexture(s16 obj, void *tex, s32 a);
extern void ScaleUV(f32 *src, f32 *dst, f32 s);
extern void *ZOID_NewPoly(s32 n, f32 *verts, u16 tidx, RGBA *color, s32 flags, s32 a);
extern void MBOX_FindTexture_WORK(char *name, u16 *out, s8 a, s8 b, s32 c);
extern void SetupTarget(char *name, s32 a, f32 *pos, s32 b, s32 slot, s32 c, s32 d);
extern s32 EmitGameSound(f32 *pos, f32 *pos2, f32 a, f32 b, f32 c, f32 d, s32 e, s32 slot, s32 f, s32 g);
extern void StartBlast(f32 *pos, s32 a, f32 b, s32 c);

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

/* 0x8038F568 ElevateGun */
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

/* 0x8038DA78 ShotToCarCollision */
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

/* 0x8038D498 SonicCarCollision */
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

/* 0x8038E088 ShotDone */
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

/* 0x8038D328 _CreateShotObject */
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

/* 0x8038D200 _UpdateShotObject */
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
        func_80090F44(PC_FSCALE(0.18f), &o->mat.uvs[0][0]);
}

/* 0x8038F938 UpdateDefense */
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
        func_80090F44(defense->unknown10, &gCarShield[i].mat.uvs[0][0]);
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

/* 0x8038E114 UpdateShots */
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
                func_80090F44(2.0734513f, &shot->uvs[0][0]);
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

/* _InitNewShot (MAT3 static, inlined into ShotUpdate; retail keeps the deleted-procedure stub) */
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

void AnimateCase(void);
/* AnimateCase (MAT3 shots.c): inlined into ShotUpdate ShotUpdate; deleted stub 0x8038FCD8. Loop moved verbatim. */
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

/* 0x8038FCE0 ShotUpdate */
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
                func_80090F44(.5f, &shot->uvs[0][0]);
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
                func_80090F44(car->battle.pitch, &shot->uvs[0][0]);
                shot->vel[0] = car->vel[0] + 50.0f*shot->uvs[2][0];
                shot->vel[1] = car->vel[1] + 50.0f*shot->uvs[2][1];
                shot->vel[2] = car->vel[2] + 50.0f*shot->uvs[2][2];
                break;
            case 8:
                car->battle.reloadingTime = 0.1666667f;
                shot->lifeTime = 4.0f;
                shot->radius = 0.5f;
                YawUV(-car->battle.yaw, &shot->uvs[0][0]);
                func_80090F44(car->battle.pitch, &shot->uvs[0][0]);
                break;
            case 0:
                car->battle.reloadingTime = 0.1666667f;
                shot->lifeTime = 4.0f;
                shot->radius = 0.50f;
                func_80090F44(car->battle.pitch, &shot->uvs[0][0]);
                break;
            case 1:
                car->battle.reloadingTime = 0.0666667f;
                shot->lifeTime = 4.0f;
                shot->radius = 0.25f;
                YawUV(Random(0.025f) - 0.0125f - car->battle.yaw, &shot->uvs[0][0]);
                func_80090F44(car->battle.pitch + (Random(0.025f) - 0.0125f), &shot->uvs[0][0]);
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
                    func_8009EA68(0.3f + Random(0.23f), &h->mat.uvs[0][0]);
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

