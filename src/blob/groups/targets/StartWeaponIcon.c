#include "game/game.h"

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
    tCarData *car;
    tModelData *m;
    s32 type;
    s32 ammo;
    s32 wep;
    CollSize *sizep;
    if (t->Flags & 4) {
        car = &pc_cars[t->slot];
        m = &pc_models[t->slot];
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
            car->battle.defense.shldPitch = 0.0f;
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
        if (type == car->battle.weapon.type) {
            car->battle.weapon.ammoCnt += ammo;
        } else {
            car->battle.weapon.type = type;
            car->battle.weapon.ammoCnt = ammo;
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
