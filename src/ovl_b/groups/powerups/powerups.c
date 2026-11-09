/* The battle arena's power-ups (MAT3 powerups.c): health, a shield and invisibility appear at random power-up spots
 * in turn (each kind's countdown, then its object shown until a car takes it), and the invisible cars fade. */
#include "game/game.h"

/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first on a
 * little-endian host (docs/port/ENDIAN.md). */
#define OBJECT_HANDLE union { s32 scene; struct { s16 index, upper; } parts; }
/* A power-up spot, a target (tTarget: its flags, scene object and object definition). */
typedef struct Object {
    /* PC: the power-ups are level-object nodes (InitBattlePowerUps's slots), which keep their N64 layout in the arena:
     * the link is a 32-bit window address, so flags, the handle and resource (+0x50) are where the N64 has them
     * (docs/port/POINTERS.md 10). */
    u32 next;
    u8 flags;
    u8 unknown05[7];
    OBJECT_HANDLE handle;
    u8 unknown10[64];
    s16 resource;
} Object;
/* A power-up spot (tBattlePowerUpT): its target, the kind it last gave (1 none, 2 invisibility, 3 a shield, 4
 * health) and the kind it shows now (1 none). */
typedef struct Slot { Object *t; s16 last, type; } Slot;
/* The power-ups (tBattlePowerUps), the spot each kind last came at, the time to each kind's next (-2 out now). */
typedef struct State {
    s8 num, hIdx, sIdx, iIdx;
    f32 healthTLeft, invsTLeft, shieldTLeft;
    Slot *tS;
} State;
/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its invisibility (battle.defense: active bit 1,
 * its fade's state, alpha, countdown and time). */
typedef struct Player {
    u8 unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; u8 unknown000_308[0x380 - 0x308]; void *input; u8 unknown000_384[0x38C - 0x384];
    u32 flags;
    u8 unknown390[16];
    s8 countdown;
    u8 alpha;
    s8 phase;
    u8 unknown3A3[5];
    f32 timer;
    u8 unknown3AC[12];
} Player;
VIEW(State, gBattlePowerUps);
#define gBattlePowerUps gBattlePowerUps_view

/* PC: the invisible car's fade out takes its u8 alpha down by at most what it has: at 60 a frame of three 30 Hz frames
 * (a slow host's five retraces) takes 24, which from 23 went round to 255. At original exactly the N64's 8. */
#define PC_FADE_STEP(n, a) (pc_fps60 && PC_STEPS(n) > (a) ? (a) : PC_STEPS(n))
/* Each player's invisibility (defence bit 1): fading out to 16, holding (flickering with its countdown), fading back
 * in once its time is up. */
void UpdateInvs(void)
{
    s32 i;
    Player *car;
    u8 invsColor[8] = {16, 32, 64, 96, 112, 144, 176, 176};
    for (i = 0; i < gNumPlayers; i++) {
        car = PC_CAR(i);
        if (car->flags & 1) {
            if (car->phase == 3) {
                car->alpha = invsColor[car->countdown];
            } else if (car->phase == 1) {
                car->countdown = 0;
                if (car->timer > 0.0f) {
                    car->alpha -= PC_FADE_STEP(8, car->alpha);
                    if (car->alpha <= 16) {
                        car->alpha = 16;
                        car->phase = 3;
                    }
                    car->timer -= gFrameSeconds;
                } else {
                    car->phase = 3;
                }
            } else {
                car->countdown = 0;
                if (car->timer > 0.0f) {
                    if (car->alpha + PC_STEPS(8) > 255) {
                        car->alpha = 255;
                    } else {
                        car->alpha = car->alpha + PC_STEPS(8);
                    }
                    car->timer -= gFrameSeconds;
                } else {
                    car->flags &= ~1;
                    car->phase = 0;
                }
            }
        }
        if (car->countdown > 0) {
            PC_EACH30(car->countdown--;)
        }
    }
}
/* The invisibility power-up: shown at a random spot (object 354) when its countdown runs out; once taken, the next
 * comes 90 to 150 seconds later. And the invisible cars (UpdateInvs). */
void UpdateBattleInvs(void)
{
    s32 i;
    UpdateInvs();
    if (gBattlePowerUps.invsTLeft == -2.0f) {
        if (gBattlePowerUps.tS[gBattlePowerUps.iIdx].t->flags & 2) {
            return;
        }
        gBattlePowerUps.invsTLeft = 90.0f + Random(60.0f);
        for (i = 0; i < gBattlePowerUps.num; i++) {
            if (gBattlePowerUps.tS[i].last == 2) {
                gBattlePowerUps.tS[i].last = 1;
            }
        }
        gBattlePowerUps.tS[gBattlePowerUps.iIdx].last = 2;
        gBattlePowerUps.tS[gBattlePowerUps.iIdx].type = 1;
    } else if ((gBattlePowerUps.invsTLeft -= gFrameSeconds) < 0.f) {
        i = Random(gBattlePowerUps.num);
        while (gBattlePowerUps.tS[i].type != 1 || gBattlePowerUps.tS[i].last == 2) {
            if (++i >= gBattlePowerUps.num) {
                i = 0;
            }
        }
        gBattlePowerUps.iIdx = i;
        gBattlePowerUps.tS[i].last = 1;
        gBattlePowerUps.tS[i].type = 2;
        gBattlePowerUps.tS[i].t->flags |= 2;
        gBattlePowerUps.tS[i].t->resource = 354;
        ZOID_SetObjectDef(gBattlePowerUps.tS[i].t->handle.parts.index, gObjList[354]);
        MBOX_ShowObject(gBattlePowerUps.tS[i].t->handle.scene, 0, 15);
        gBattlePowerUps.invsTLeft = -2.0f;
    }
}
/* The shield power-up (object 359): as UpdateBattleInvs. */
void UpdateBattleShield(void)
{
    s32 i;
    if (gBattlePowerUps.shieldTLeft == -2.0f) {
        if (!(gBattlePowerUps.tS[gBattlePowerUps.sIdx].t->flags & 2)) {
            gBattlePowerUps.shieldTLeft = Random(60.0f) + 90.0f;
            for (i = 0; i < gBattlePowerUps.num; i++) {
                if (gBattlePowerUps.tS[i].last == 3) {
                    gBattlePowerUps.tS[i].last = 1;
                }
            }
            gBattlePowerUps.tS[gBattlePowerUps.sIdx].last = 3;
            gBattlePowerUps.tS[gBattlePowerUps.sIdx].type = 1;
        }
    } else {
        gBattlePowerUps.shieldTLeft -= gFrameSeconds;
        if (gBattlePowerUps.shieldTLeft < 0.0f) {
            i = Random(gBattlePowerUps.num);
            while (gBattlePowerUps.tS[i].type != 1 || gBattlePowerUps.tS[i].last == 3) {
                if (++i >= gBattlePowerUps.num) i = 0;
            }
            gBattlePowerUps.sIdx = i;
            gBattlePowerUps.tS[i].last = 1;
            gBattlePowerUps.tS[i].type = 3;
            gBattlePowerUps.tS[i].t->flags |= 2;
            gBattlePowerUps.tS[i].t->resource = 359;
            ZOID_SetObjectDef(gBattlePowerUps.tS[i].t->handle.parts.index, gObjList[359]);
            MBOX_ShowObject(gBattlePowerUps.tS[i].t->handle.scene, 0, 15);
            gBattlePowerUps.shieldTLeft = -2.0f;
        }
    }
}
/* The health power-up (object 353): as UpdateBattleInvs, the next 5 to 15 seconds after one is taken. */
void UpdateBattleHealth(void)
{
    s32 i;
    if (gBattlePowerUps.healthTLeft == -2.0f) {
        if (!(gBattlePowerUps.tS[gBattlePowerUps.hIdx].t->flags & 2)) {
            gBattlePowerUps.healthTLeft = Random(10.0f) + 5.0f;
            for (i = 0; i < gBattlePowerUps.num; i++) {
                if (gBattlePowerUps.tS[i].last == 4) {
                    gBattlePowerUps.tS[i].last = 1;
                }
            }
            gBattlePowerUps.tS[gBattlePowerUps.hIdx].last = 4;
            gBattlePowerUps.tS[gBattlePowerUps.hIdx].type = 1;
        }
    } else {
        gBattlePowerUps.healthTLeft -= gFrameSeconds;
        if (gBattlePowerUps.healthTLeft < 0.0f) {
            i = Random(gBattlePowerUps.num);
            while (gBattlePowerUps.tS[i].type != 1 || gBattlePowerUps.tS[i].last == 4) {
                if (++i >= gBattlePowerUps.num) i = 0;
            }
            gBattlePowerUps.hIdx = i;
            gBattlePowerUps.tS[i].last = 1;
            gBattlePowerUps.tS[i].type = 4;
            gBattlePowerUps.tS[i].t->flags |= 2;
            gBattlePowerUps.tS[i].t->resource = 353;
            ZOID_SetObjectDef(gBattlePowerUps.tS[i].t->handle.parts.index, gObjList[353]);
            MBOX_ShowObject(gBattlePowerUps.tS[i].t->handle.scene, 0, 15);
            gBattlePowerUps.healthTLeft = -2.0f;
        }
    }
}
/* One frame of the power-ups (PlayGame, battle): health, shield, invisibility. */
void UpdateBattlePowerUps(void)
{
    UpdateBattleHealth();
    UpdateBattleShield();
    UpdateBattleInvs();
}
