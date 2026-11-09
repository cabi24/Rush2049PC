#include "game/game.h"

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
/* PC: the car record's layout (0x510 bytes, docs/port/CARS.md): its 21 visuals and the input pointer are host-sized. */
typedef struct Car {
    u8 pad00[0xe8];
    u32 appearance;       /* 0xe8 */
    u8 padec[0x110 - 0xec];
    struct pc_visual visuals[21];
    u8 padec_308[0x380 - 0x308];
    void *input;
    Battle battle;        /* 0x384, PC 0x4D8 */
} Car;
typedef struct Model {
    _tCar *car;
    Tune44 *tune;
    u8 body_type;         /* 0x08 */
    u8 pad09[0xf4 - 9];
    f32 bodyR[2][3];      /* 0xf4: [0][2] at 0xfc, [1][2] at 0x108 */
    u8 pad10c[0x3a0 - 0x10c];
    streetyp *lasttp[4];
    u8 pad3b0[0x40c - 0x3b0];
    const s16 *torquecurve;
    u8 pad410[0x648 - 0x410];
    surftyp *movingSurface;
    u8 pad64c[0x654 - 0x64c];
    f32 colrad;           /* 0x654 */
    u8 pad658[0x808 - 0x658];
} Model;
typedef struct CollSize { f32 colfront, colrear, colside, colheight; } CollSize;
f32 sqrtf(f32);
#pragma intrinsic (sqrtf)

/* A battle power-up's start function (MAT3 targets.c _StartWeaponIcon), for target t (its type is the power-up's
 * object). Hit by a car (TARGET_HIT): it goes (not active) and gives the car what it is, unless the car is
 * invisible: a weapon and its ammunition (the missile, type 5, lengthens the car's front), health, invisibility or
 * a shield, with its sound. Started when defined (its kind's flag 0x4000): it becomes a power-up spot (Flags
 * 0x20), counted (IncBattlePowerUps) and hidden. */
void StartWeaponIcon(tTarget *t)
{
    Car *car;
    Model *m;
    s32 type;
    s32 ammo;
    s32 wep;
    CollSize *sizep;
    if (t->Flags & 4) {
        car = PC_CAR(t->slot);
        m = PC_MODEL(t->slot);
        t->Flags &= ~6;
        if (car->appearance & 0x10) {
            t->Flags |= 2;
            return;
        }
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
        switch (t->type) {
        case 350:
            type = 0;
            ammo = ammoCnts[0];
            wep = 1;
            break;
        case 351:
            type = 1;
            ammo = ammoCnts[1];
            wep = 1;
            break;
        case 352:
            type = 2;
            ammo = ammoCnts[2];
            wep = 1;
            break;
        case 353:
            car->battle.health = 800;
            return;
        case 354:
            car->battle.defense.active |= 1;
            car->battle.defense.invsTime = 30.0f;
            car->battle.fadeState = 1;
            car->battle.fadeTime = 0.666667f;
            car->battle.fadeAlpha = 255;
            return;
        case 355:
            type = 3;
            ammo = ammoCnts[3];
            wep = 1;
            break;
        case 356:
            type = 4;
            ammo = ammoCnts[4];
            wep = 1;
            break;
        case 357:
            type = 5;
            ammo = ammoCnts[5];
            wep = 1;
            break;
        case 358:
            type = 6;
            ammo = ammoCnts[6];
            wep = 1;
            break;
        case 359:
            car->battle.defense.active |= 0xa;
            car->battle.defense.shldTime = 0.0333333f;
            car->battle.defense.shldScale = 0.05f;
            car->battle.defense.unknown10 = 0.0f;
            return;
        case 360:
            type = 7;
            ammo = ammoCnts[7];
            wep = 1;
            break;
        default:
            type = 1;
            ammo = ammoCnts[1];
            wep = 1;
            break;
        }
        if (wep) {
        if (type == car->battle.type) {
            car->battle.ammoCnt += ammo;
        } else {
            car->battle.type = type;
            car->battle.ammoCnt = ammo;
        }
        if (type == 5) {
            f32 z;
            sizep = &collsize[m->body_type];
            m->bodyR[0][2] = m->bodyR[1][2] = sizep->colfront + 3.0f;
            z = m->bodyR[0][2] > sizep->colrear ? m->bodyR[0][2] : sizep->colrear;
            m->colrad = sqrtf(z * z + sizep->colside * sizep->colside + sizep->colheight * sizep->colheight);
        }
        }
    } else {
        IncBattlePowerUps();
        t->Flags &= ~2;
        t->Flags |= 0x20;
        MBOX_HideObject(t->dlIdx, 0, 15);
    }
}
