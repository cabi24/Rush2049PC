/* The battle's weapons on the cars (MAT3 weapons.c): the weapon a car carries, animated on it (AnimateWeapon), and
 * the machine gun's and cannon's muzzle flashes. */
#include "game/game.h"

typedef union RGBA { u8 c[4]; u32 rgba; } RGBA;

typedef struct Defense {
    u32 active; f32 invsTime; f32 shldTime; f32 shldScale; f32 unknown10;
} Defense;

typedef struct Battle {
    s8 type; s8 ammoCnt; s16 health; s16 damageThisFrame; u8 pad06[2];
    Defense defense;      /* 0x08 */
    s8 shoot;             /* 0x1c */
    u8 fadeAlpha;         /* 0x1d */
    s8 fadeState;         /* 0x1e */
    s8 Points;            /* 0x1f */
    s32 loaded;           /* 0x20 */
    f32 fadeTime; f32 reloadingTime; f32 pitch; f32 yaw;
} Battle;

typedef struct Car {
    u8 pad00[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad00_308[0x34C - 0x308];
    RGBA IRclr;           /* 0x34c */
    u8 pad350[0x35c - 0x350];
    s8 camera;            /* 0x35c */
    s8 view;              /* 0x35d */
    u8 pad35e[0x380 - 0x35E]; void *input;
    Battle battle;        /* 0x384 */
} Car;

typedef struct Model {
    u8 pad00[8];
    u8 body_type;         /* 0x08 */
    u8 pad09[0x640 - 9];
    s8 crashflag;         /* 0x640 */
    u8 pad641[0x808 - 0x641];
} Model;

typedef struct Visual {
    struct Visual *next;  /* 0x00 */
    s16 index;            /* 0x04 */
    s16 objnum;           /* 0x06 */
    s16 slot;             /* 0x08 */
    u32 data;             /* 0x0c */
    f32 timeStamp;        /* 0x10 */
    void (*func)(struct Visual *, s16); /* 0x14 */
} Visual;

typedef struct CarParts {
    s32 frame;            /* 0x00 */
    u8 pad04[0x14];
    s32 weap;             /* 0x18 */
    u8 pad1c[0x24];
} CarParts;

typedef struct CMuzzleObj {
    s32 mdls[5];          /* 0x00 */
    MuzzleMat mats[5];    /* 0x14 */
    s32 ripdl;            /* 0x104 */
    UVMat mat;            /* 0x108 */
    s8 ripCnt;            /* 0x138 */
    s8 ripTIdx;           /* 0x139 */
    s16 on;               /* 0x13a */
    f32 ripScale;         /* 0x13c */
    f32 ripTime;          /* 0x140 */
    f32 onTime;           /* 0x144 */
} CMuzzleObj;

/* A texture definition (MAT3 tTexture, 36 bytes): only its size matters here. */
typedef struct tTexture { u8 b[36]; } TexDef;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(CarParts[], gCarParts);
#define gCarParts gCarParts_view

VIEW(s32[7], gWfxObjList);
#define gWfxObjList gWfxObjList_view
VIEW(u16[5], gRipTextures);
#define gRipTextures gRipTextures_view

extern UVMat *ZOID_FindMatrix(s16 idx);

extern void ZOID_SetTexture(s16 obj, void *tex, s32 a);

f32 weaponOffsets[8][13][3] = {
    0.0f, 2.5f, 0.3f, 0.0f, 3.0f, 0.3f, 0.0f, 2.8f, 0.3f, 0.0f, 3.0f, 0.1f, 0.0f, 3.0f, 0.1f, 0.0f,
    3.4f, -0.4f, 0.0f, 3.1f, 0.3f, 0.0f, 3.3f, 1.0f, 0.0f, 3.39f, -1.82f, 0.0f, 3.05f, 0.65f, 0.0f,
    5.04f, 1.33f, 0.0f, 2.95f, -0.9f, 0.0f, 2.79f, 0.52f, 0.0f, 2.7f, 2.1f, 0.0f, 3.1f, 1.1f, 0.0f,
    2.7f, 0.1f, 0.0f, 2.9f, -0.1f, 0.0f, 2.9f, 0.4f, 0.0f, 3.5f, 0.1f, 0.0f, 3.0f, 0.3f, 0.0f,
    3.3f, 1.2f, 0.0f, 3.35f, -0.69f, 0.0f, 3.04f, 0.97f, 0.0f, 5.05f, 2.02f, 0.0f, 2.98f, -0.31f,
    0.0f, 2.79f, 0.92f, 0.0f, 2.5f, 1.3f, 0.0f, 3.0f, -0.5f, 0.0f, 2.6f, -0.6f, 0.0f, 2.7f, -1.1f,
    0.0f, 2.7f, -1.1f, 0.0f, 3.3f, -1.6f, 0.0f, 2.9f, -0.6f, 0.0f, 3.0f, 0.1f, 0.0f, 3.13f, -1.73f,
    0.0f, 2.89f, -0.41f, 0.0f, 4.84f, 0.37f, 0.0f, 2.75f, -1.67f, 0.0f, 2.66f, -0.35f, 0.0f, 1.9f,
    -5.3f, 0.0f, 1.7f, -5.7f, 0.0f, 1.9f, -5.6f, 0.0f, 2.0f, -5.9f, 0.0f, 1.4f, -6.1f, 0.0f, 1.4f,
    -7.0f, 0.0f, 1.9f, -5.7f, 0.0f, 1.8f, -6.0f, 0.0f, 1.3f, -5.71f, 0.0f, 1.6f, -6.73f, 0.0f,
    1.71f, -9.28f, 0.0f, 1.74f, -6.95f, 0.0f, 1.28f, -5.54f, 0.0f, 2.2f, 0.6f, 0.0f, 2.6f, -0.7f,
    0.0f, 2.4f, -0.8f, 0.0f, 2.4f, -1.1f, 0.0f, 2.6f, -0.5f, 0.0f, 3.0f, -1.7f, 0.0f, 2.6f, -0.5f,
    0.0f, 2.7f, -0.5f, 0.0f, 3.16f, -2.52f, 0.0f, 2.67f, -0.39f, 0.0f, 4.8f, 0.44f, 0.0f, 2.52f,
    -2.15f, 0.0f, 2.37f, -0.44f, 0.0f, 0.0f, 6.4f, 0.0f, 0.0f, 6.7f, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f,
    6.0f, 0.0f, 0.2f, 6.4f, 0.0f, 0.2f, 6.8f, 0.0f, 0.1f, 5.9f, 0.0f, 0.2f, 5.8f, 0.0f, 0.25f,
    4.86f, 0.0f, 0.44f, 7.38f, 0.0f, -0.18f, 8.07f, 0.0f, 0.14f, 5.89f, 0.0f, 0.14f, 5.89f, 0.0f,
    2.9f, 0.3f, 0.0f, 3.3f, -0.7f, 0.0f, 3.1f, -0.9f, 0.0f, 3.2f, -0.9f, 0.0f, 3.2f, -0.9f, 0.0f,
    3.6f, -1.4f, 0.0f, 3.3f, -0.7f, 0.0f, 3.4f, -0.3f, 0.0f, 3.65f, -2.06f, 0.0f, 3.17f, -0.44f,
    0.0f, 5.28f, 0.66f, 0.0f, 3.15f, -1.52f, 0.0f, 3.08f, -0.6f, 0.0f, 2.6f, 0.4f, 0.0f, 3.0f,
    -0.8f, 0.0f, 2.7f, -0.8f, 0.0f, 3.1f, -0.4f, 0.0f, 3.1f, -0.2f, 0.0f, 3.4f, -1.8f, 0.0f, 3.1f,
    -0.9f, 0.0f, 3.1f, -0.9f, 0.0f, 3.52f, -2.0f, 0.0f, 3.4f, -0.33f, 0.0f, 4.38f, -4.39f, 0.0f,
    3.25f, -2.05f, 0.0f, 3.12f, -1.11f
};
/* PC: InitBattlePlayer (the weapon reset on a resurrect) writes these by their addresses, which on the N64 are this
 * file's statics. Declared here, they are one storage on the PC too: image B's BSS arena (port/data), zeroed on
 * every load of the image. */
#define WEAPON_STATE extern
/* PC: battle.type 8 is no weapon. The update reads its row of weaponOffsets when a car loses its weapon, and doesn't
 * use it: past the table's 8 rows, on the N64 into the data after it, on the PC into whatever the linker put there
 * (UBSan, docs/port/SANITIZE.md 3). It reads zeros instead. The table keeps its N64 size: port/data maps the N64
 * addresses a C-defined symbol covers to it, and a ninth row took the next data's. */
static const f32 pc_no_weapon[13][3];
#define WEAPON_OFF(type) ((type) < 8 ? weaponOffsets[type] : pc_no_weapon)
VIEW(s8[4], gCurWeaponType);
#define gCurWeaponType gCurWeaponType_view
VIEW(MuzzleObj[4], gGattMuzzleObjs);
#define gGattMuzzleObjs gGattMuzzleObjs_view
VIEW(CMuzzleObj[4], gCannMuzzleObjs);
#define gCannMuzzleObjs gCannMuzzleObjs_view

/* Removes car `slot`'s machine gun flash (its five scene objects). */
void RemoveGattMFlash(s16 slot)
{
    s32 i;
    for (i = 0; i < 5; i++) {
        if (gGattMuzzleObjs[slot].mdls[i] != -1) {
            ZOID_RemoveObject(gGattMuzzleObjs[slot].mdls[i]);
            gGattMuzzleObjs[slot].mdls[i] = -1;
        }
    }
}

/* Removes car `slot`'s cannon flash and its ripple. */
void RemoveCannMFlash(s16 slot)
{
    s32 i;
    for (i = 0; i < 5; i++) {
        if (gCannMuzzleObjs[slot].mdls[i] != -1) {
            ZOID_RemoveObject(gCannMuzzleObjs[slot].mdls[i]);
            gCannMuzzleObjs[slot].mdls[i] = -1;
        }
    }
    if (gCannMuzzleObjs[slot].ripdl != -1) {
        ZOID_RemoveObject(gCannMuzzleObjs[slot].ripdl);
        gCannMuzzleObjs[slot].ripdl = -1;
        gCannMuzzleObjs[slot].ripTIdx = -1;
    }
}

/* The weapon visual of a car (`v`; `op` 0 removes it): the weapon's model follows the car's weapon type (the
 * car's frame, its offset by body), the flashes and the ripple fade, and the weapon goes when the car has none. */
void AnimateWeapon(Visual *v, s16 op)
{
    Car *car = PC_CAR(v->slot);
    s32 def;
    s32 i;
    s32 r1;
    f32 r2;
    RGBA clr;
    RGBA clr2 = { { 0, 255, 0, 255 } };
    UVMat *mat;
    f32 off[3];
    s32 pad1;   /* unused (the N64 code's) */
    s32 pad2;   /* unused (the N64 code's) */

    if (!op || PC_MODEL_CRASHFLAG(v->slot)) {
        if (gCurWeaponType[v->slot] == 1)
            RemoveGattMFlash(v->slot);
        else if (gCurWeaponType[v->slot] == 0)
            RemoveCannMFlash(v->slot);
        car->battle.type = 8;
        car->battle.ammoCnt = -1;
        MBOX_HideObject(v->objnum, 0, 15);
        return;
    }
    clr.rgba = car->IRclr.rgba;
    if (car->battle.defense.active & 1) {
        clr.c[3] = car->battle.fadeAlpha;
        if (clr.c[3] < 48)
            clr.c[3] = 48;
    }
    ZOID_SetColor(gCarParts[v->slot].weap, &clr);
    mat = ZOID_FindMatrix(gCarParts[v->slot].frame);
    if (car->battle.type == 8) {
        MBOX_HideObject(v->objnum, 1, 15);
    } else if (car->camera >= 0 && car->view <= 1) {
        MBOX_ShowObject(v->objnum, 0, 15);
        MBOX_HideObject(v->objnum, 1, 1 << car->camera);
    } else {
        MBOX_ShowObject(v->objnum, 0, 15);
    }
    if (gCurWeaponType[v->slot] != car->battle.type) {
        off[0] = WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)][0];
        off[1] = WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)][1];
        off[2] = WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)][2];
        if (gCurWeaponType[v->slot] == 1)
            RemoveGattMFlash(v->slot);
        else if (gCurWeaponType[v->slot] == 0)
            RemoveCannMFlash(v->slot);
        gCurWeaponType[v->slot] = car->battle.type;
        switch (car->battle.type) {
        case 8:
            return;
        case 0:
            def = 216;
            off[0] += 0.16f;
            off[1] += 0.48f;
            off[2] += 2.57f;
            for (i = 0; i < 5; i++) {
                CopyUV(&mat->uvs[0][0], &gCannMuzzleObjs[v->slot].mats[i].mat.uvs[0][0]);
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] = mat->pos[0] + mat->uvs[0][0] * off[0];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] = mat->pos[1] + mat->uvs[0][1] * off[0];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] = mat->pos[2] + mat->uvs[0][2] * off[0];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[1][0] * off[1];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[1][1] * off[1];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[1][2] * off[1];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[2][0] * off[2];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[2][1] * off[2];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[2][2] * off[2];
                gCannMuzzleObjs[v->slot].mdls[i] = MBOX_NewObject(gWfxObjList[i], &gCannMuzzleObjs[v->slot].mats[i].mat, -1, 0x42000);
                ZOID_SetColor(gCannMuzzleObjs[v->slot].mdls[i], &clr2);
                MBOX_HideObject(gCannMuzzleObjs[v->slot].mdls[i], 1, 15);
            }
            CopyUV(&mat->uvs[0][0], &gCannMuzzleObjs[v->slot].mat.uvs[0][0]);
            gCannMuzzleObjs[v->slot].mat.pos[0] = mat->pos[0] + mat->uvs[0][0] * off[0];
            gCannMuzzleObjs[v->slot].mat.pos[1] = mat->pos[1] + mat->uvs[0][1] * off[0];
            gCannMuzzleObjs[v->slot].mat.pos[2] = mat->pos[2] + mat->uvs[0][2] * off[0];
            gCannMuzzleObjs[v->slot].mat.pos[0] = gCannMuzzleObjs[v->slot].mat.pos[0] + mat->uvs[1][0] * off[1];
            gCannMuzzleObjs[v->slot].mat.pos[1] = gCannMuzzleObjs[v->slot].mat.pos[1] + mat->uvs[1][1] * off[1];
            gCannMuzzleObjs[v->slot].mat.pos[2] = gCannMuzzleObjs[v->slot].mat.pos[2] + mat->uvs[1][2] * off[1];
            gCannMuzzleObjs[v->slot].mat.pos[0] = gCannMuzzleObjs[v->slot].mat.pos[0] + mat->uvs[2][0] * off[2];
            gCannMuzzleObjs[v->slot].mat.pos[1] = gCannMuzzleObjs[v->slot].mat.pos[1] + mat->uvs[2][1] * off[2];
            gCannMuzzleObjs[v->slot].mat.pos[2] = gCannMuzzleObjs[v->slot].mat.pos[2] + mat->uvs[2][2] * off[2];
            gCannMuzzleObjs[v->slot].ripdl = MBOX_NewObject(gWfxObjList[6], &gCannMuzzleObjs[v->slot].mat, -1, 0x42000);
            ZOID_SetColor(gCannMuzzleObjs[v->slot].ripdl, &clr2);
            MBOX_HideObject(gCannMuzzleObjs[v->slot].ripdl, 1, 15);
            gCannMuzzleObjs[v->slot].ripCnt = -1;
            gCannMuzzleObjs[v->slot].ripTime = 0.0f;
            gCannMuzzleObjs[v->slot].ripScale = 1.5f + Random(1.5f);
            break;
        case 1:
            def = 217;
            off[0] += 0.01f;
            off[1] += 0.65f;
            off[2] += 3.59f;
            for (i = 0; i < 5; i++) {
                CopyUV(&mat->uvs[0][0], &gGattMuzzleObjs[v->slot].mats[i].mat.uvs[0][0]);
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] = mat->pos[0] + mat->uvs[0][0] * off[0];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] = mat->pos[1] + mat->uvs[0][1] * off[0];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] = mat->pos[2] + mat->uvs[0][2] * off[0];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[1][0] * off[1];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[1][1] * off[1];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[1][2] * off[1];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[2][0] * off[2];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[2][1] * off[2];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[2][2] * off[2];
                clr2.c[0] = 255;
                clr2.c[1] = 255;
                clr2.c[2] = 43;
                gGattMuzzleObjs[v->slot].mdls[i] = MBOX_NewObject(gWfxObjList[i], &gGattMuzzleObjs[v->slot].mats[i].mat, -1, 0x42000);
                ZOID_SetColor(gGattMuzzleObjs[v->slot].mdls[i], &clr2);
                MBOX_HideObject(gGattMuzzleObjs[v->slot].mdls[i], 1, 15);
            }
            break;
        case 2: def = 218; break;
        case 3: def = 219; break;
        case 4: def = 220; break;
        case 5: def = 221; break;
        case 6: def = 222; break;
        case 7: def = 223; break;
        }
        ZOID_SetObjectDef(gCarParts[v->slot].weap, gObjList[def]);
        ZOID_UpdateObject(gCarParts[v->slot].weap, WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)], 0);
    }
    {
        off[0] = WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)][0];
        off[1] = WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)][1];
        off[2] = WEAPON_OFF(car->battle.type)[PC_MODEL_BODY(v->slot)][2];
        if (gCurWeaponType[v->slot] == 1) {
            off[0] += 0.01f;
            off[1] += 0.65f;
            off[2] += 3.59f;
            for (i = 0; i < 5; i++) {
                CopyUV(&mat->uvs[0][0], &gGattMuzzleObjs[v->slot].mats[i].mat.uvs[0][0]);
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] = mat->pos[0] + mat->uvs[0][0] * off[0];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] = mat->pos[1] + mat->uvs[0][1] * off[0];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] = mat->pos[2] + mat->uvs[0][2] * off[0];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[1][0] * off[1];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[1][1] * off[1];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[1][2] * off[1];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[2][0] * off[2];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[2][1] * off[2];
                gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] = gGattMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[2][2] * off[2];
            }
            if (gGattMuzzleObjs[v->slot].on) {
                gGattMuzzleObjs[v->slot].onTime += gFrameSeconds;
                if (gGattMuzzleObjs[v->slot].onTime >= 0.0333333f) {
                    gGattMuzzleObjs[v->slot].on = 0;
                    for (i = 0; i < 5; i++)
                        MBOX_HideObject(gGattMuzzleObjs[v->slot].mdls[i], 1, 15);
                }
            }
            if (car->battle.shoot >= 5) {
                gGattMuzzleObjs[v->slot].on = 1;
                gGattMuzzleObjs[v->slot].onTime = 0.0f;
                MBOX_ShowObject(gGattMuzzleObjs[v->slot].mdls[0], 0, 15);
                if (car->camera >= 0 && car->view <= 1)
                    MBOX_HideObject(gGattMuzzleObjs[v->slot].mdls[0], 1, 1 << car->camera);
                r1 = (s32)Random(4.0f) + 1;
                r2 = Random(0.5f);
                RollUV(r2, &gGattMuzzleObjs[v->slot].mats[r1].mat.uvs[0][0]);
                ScaleUV(&gGattMuzzleObjs[v->slot].mats[r1].mat.uvs[0][0], &gGattMuzzleObjs[v->slot].mats[r1].mat.uvs[0][0], 0.85f + Random(0.75f));
                MBOX_ShowObject(gGattMuzzleObjs[v->slot].mdls[r1], 0, 15);
                if (car->camera >= 0 && car->view <= 1)
                    MBOX_HideObject(gGattMuzzleObjs[v->slot].mdls[r1], 1, 1 << car->camera);
                r1++;
                if (r1 >= 5)
                    r1 = 1;
                RollUV(r2, &gGattMuzzleObjs[v->slot].mats[r1].mat.uvs[0][0]);
                ScaleUV(&gGattMuzzleObjs[v->slot].mats[r1].mat.uvs[0][0], &gGattMuzzleObjs[v->slot].mats[r1].mat.uvs[0][0], 0.85f + Random(0.75f));
                MBOX_ShowObject(gGattMuzzleObjs[v->slot].mdls[r1], 0, 15);
                if (car->camera >= 0 && car->view <= 1)
                    MBOX_HideObject(gGattMuzzleObjs[v->slot].mdls[r1], 1, 1 << car->camera);
            }
        }
        if (gCurWeaponType[v->slot] == 0) {
            off[0] += 0.16f;
            off[1] += 0.48f;
            off[2] += 2.57f;
            for (i = 0; i < 5; i++) {
                CopyUV(&mat->uvs[0][0], &gCannMuzzleObjs[v->slot].mats[i].mat.uvs[0][0]);
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] = mat->pos[0] + mat->uvs[0][0] * off[0];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] = mat->pos[1] + mat->uvs[0][1] * off[0];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] = mat->pos[2] + mat->uvs[0][2] * off[0];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[1][0] * off[1];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[1][1] * off[1];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[1][2] * off[1];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[0] + mat->uvs[2][0] * off[2];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[1] + mat->uvs[2][1] * off[2];
                gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] = gCannMuzzleObjs[v->slot].mats[i].mat.pos[2] + mat->uvs[2][2] * off[2];
            }
            CopyUV(&mat->uvs[0][0], &gCannMuzzleObjs[v->slot].mat.uvs[0][0]);
            gCannMuzzleObjs[v->slot].mat.pos[0] = mat->pos[0] + mat->uvs[0][0] * off[0];
            gCannMuzzleObjs[v->slot].mat.pos[1] = mat->pos[1] + mat->uvs[0][1] * off[0];
            gCannMuzzleObjs[v->slot].mat.pos[2] = mat->pos[2] + mat->uvs[0][2] * off[0];
            gCannMuzzleObjs[v->slot].mat.pos[0] = gCannMuzzleObjs[v->slot].mat.pos[0] + mat->uvs[1][0] * off[1];
            gCannMuzzleObjs[v->slot].mat.pos[1] = gCannMuzzleObjs[v->slot].mat.pos[1] + mat->uvs[1][1] * off[1];
            gCannMuzzleObjs[v->slot].mat.pos[2] = gCannMuzzleObjs[v->slot].mat.pos[2] + mat->uvs[1][2] * off[1];
            gCannMuzzleObjs[v->slot].mat.pos[0] = gCannMuzzleObjs[v->slot].mat.pos[0] + mat->uvs[2][0] * off[2];
            gCannMuzzleObjs[v->slot].mat.pos[1] = gCannMuzzleObjs[v->slot].mat.pos[1] + mat->uvs[2][1] * off[2];
            gCannMuzzleObjs[v->slot].mat.pos[2] = gCannMuzzleObjs[v->slot].mat.pos[2] + mat->uvs[2][2] * off[2];
            if (gCannMuzzleObjs[v->slot].ripCnt != -1) {
                gCannMuzzleObjs[v->slot].ripTime -= gFrameSeconds;
                if (gCannMuzzleObjs[v->slot].ripTime <= 0.0f) {
                    gCannMuzzleObjs[v->slot].ripTime = 0.0333333f;
                    gCannMuzzleObjs[v->slot].ripCnt++;
                    if (gCannMuzzleObjs[v->slot].ripCnt >= 5) {
                        MBOX_HideObject(gCannMuzzleObjs[v->slot].ripdl, 1, 15);
                        gCannMuzzleObjs[v->slot].ripCnt = -1;
                    } else {
                        gCannMuzzleObjs[v->slot].ripTIdx++;
                        if (gCannMuzzleObjs[v->slot].ripTIdx >= 5)
                            gCannMuzzleObjs[v->slot].ripTIdx = 0;
                        ZOID_SetTexture(gCannMuzzleObjs[v->slot].ripdl, &gTexDefPtrs[gRipTextures[gCannMuzzleObjs[v->slot].ripTIdx] >> 10].texDefs[gRipTextures[gCannMuzzleObjs[v->slot].ripTIdx] & 1023], -1);
                    }
                }
            }
            if (gCannMuzzleObjs[v->slot].on) {
                gCannMuzzleObjs[v->slot].onTime += gFrameSeconds;
                if (gCannMuzzleObjs[v->slot].onTime >= 0.0666667f) {
                    gCannMuzzleObjs[v->slot].on = 0;
                    for (i = 0; i < 5; i++)
                        MBOX_HideObject(gCannMuzzleObjs[v->slot].mdls[i], 1, 15);
                }
            }
            if (car->battle.shoot >= 5) {
                gCannMuzzleObjs[v->slot].ripScale = 1.5f + Random(1.5f);
                gCannMuzzleObjs[v->slot].ripTime = 0.0333333f;
                gCannMuzzleObjs[v->slot].ripCnt = 0;
                gCannMuzzleObjs[v->slot].ripTIdx = (s32)Random(5.0f);
                ZOID_SetTexture(gCannMuzzleObjs[v->slot].ripdl, &gTexDefPtrs[gRipTextures[0] >> 10].texDefs[gRipTextures[0] & 1023], -1);
                MBOX_ShowObject(gCannMuzzleObjs[v->slot].ripdl, 0, 15);
                if (car->camera >= 0 && car->view <= 1)
                    MBOX_HideObject(gCannMuzzleObjs[v->slot].ripdl, 1, 1 << car->camera);
                gCannMuzzleObjs[v->slot].on = 1;
                gCannMuzzleObjs[v->slot].onTime = 0.0f;
                MBOX_ShowObject(gCannMuzzleObjs[v->slot].mdls[0], 0, 15);
                if (car->camera >= 0 && car->view <= 1)
                    MBOX_HideObject(gCannMuzzleObjs[v->slot].mdls[0], 1, 1 << car->camera);
                for (i = 1; i < 5; i++) {
                    RollUV(Random(1.25f) - 0.5f, &gCannMuzzleObjs[v->slot].mats[i].mat.uvs[0][0]);
                    ScaleUV(&gCannMuzzleObjs[v->slot].mats[i].mat.uvs[0][0], &gCannMuzzleObjs[v->slot].mats[i].mat.uvs[0][0], 0.4f + Random(0.25f));
                    MBOX_ShowObject(gCannMuzzleObjs[v->slot].mdls[i], 0, 15);
                    if (car->camera >= 0 && car->view <= 1)
                        MBOX_HideObject(gCannMuzzleObjs[v->slot].mdls[i], 1, 1 << car->camera);
                }
            }
            ScaleUV(&gCannMuzzleObjs[v->slot].mat.uvs[0][0], &gCannMuzzleObjs[v->slot].mat.uvs[0][0], gCannMuzzleObjs[v->slot].ripScale);
        }
    }
}

