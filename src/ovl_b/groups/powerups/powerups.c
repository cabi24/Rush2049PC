#include "game/game.h"

/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first on a
 * little-endian host (docs/port/ENDIAN.md). */
#define OBJECT_HANDLE union { s32 scene; struct { s16 index, upper; } parts; }
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
typedef struct Slot { Object *object; s16 active, count; } Slot;
typedef struct State {
    s8 count, selected_delay, selected_b, selected_a;
    f32 delay, phase_a, phase_b;
    Slot *slots;
} State;
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
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* PC: the invisible car's fade out takes its u8 alpha down by at most what it has: at 60 a frame of three 30 Hz frames
 * (a slow host's five retraces) takes 24, which from 23 went round to 255. At original exactly the N64's 8. */
#define PC_FADE_STEP(n, a) (pc_fps60 && PC_STEPS(n) > (a) ? (a) : PC_STEPS(n))
void UpdateInvs(void) /* UpdateInvs */
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
void UpdateBattleInvs(void) /* UpdateBattleInvs */
{
    s32 i;
    UpdateInvs();
    if (gBattlePowerUps.phase_a == -2.0f) {
        if (gBattlePowerUps.slots[gBattlePowerUps.selected_a].object->flags & 2) {
            return;
        }
        gBattlePowerUps.phase_a = 90.0f + Random(60.0f);
        for (i = 0; i < gBattlePowerUps.count; i++) {
            if (gBattlePowerUps.slots[i].active == 2) {
                gBattlePowerUps.slots[i].active = 1;
            }
        }
        gBattlePowerUps.slots[gBattlePowerUps.selected_a].active = 2;
        gBattlePowerUps.slots[gBattlePowerUps.selected_a].count = 1;
    } else if ((gBattlePowerUps.phase_a -= gFrameSeconds) < 0.f) {
        i = Random(gBattlePowerUps.count);
        while (gBattlePowerUps.slots[i].count != 1 || gBattlePowerUps.slots[i].active == 2) {
            if (++i >= gBattlePowerUps.count) {
                i = 0;
            }
        }
        gBattlePowerUps.selected_a = i;
        gBattlePowerUps.slots[i].active = 1;
        gBattlePowerUps.slots[i].count = 2;
        gBattlePowerUps.slots[i].object->flags |= 2;
        gBattlePowerUps.slots[i].object->resource = 354;
        ZOID_SetObjectDef(gBattlePowerUps.slots[i].object->handle.parts.index, gObjList[354]);
        MBOX_ShowObject(gBattlePowerUps.slots[i].object->handle.scene, 0, 15);
        gBattlePowerUps.phase_a = -2.0f;
    }
}
void UpdateBattleShield(void)
{
    s32 i;
    if (gBattlePowerUps.phase_b == -2.0f) {
        if (!(gBattlePowerUps.slots[gBattlePowerUps.selected_b].object->flags & 2)) {
            gBattlePowerUps.phase_b = Random(60.0f) + 90.0f;
            for (i = 0; i < gBattlePowerUps.count; i++) {
                if (gBattlePowerUps.slots[i].active == 3) {
                    gBattlePowerUps.slots[i].active = 1;
                }
            }
            gBattlePowerUps.slots[gBattlePowerUps.selected_b].active = 3;
            gBattlePowerUps.slots[gBattlePowerUps.selected_b].count = 1;
        }
    } else {
        gBattlePowerUps.phase_b -= gFrameSeconds;
        if (gBattlePowerUps.phase_b < 0.0f) {
            i = Random(gBattlePowerUps.count);
            while (gBattlePowerUps.slots[i].count != 1 || gBattlePowerUps.slots[i].active == 3) {
                if (++i >= gBattlePowerUps.count) i = 0;
            }
            gBattlePowerUps.selected_b = i;
            gBattlePowerUps.slots[i].active = 1;
            gBattlePowerUps.slots[i].count = 3;
            gBattlePowerUps.slots[i].object->flags |= 2;
            gBattlePowerUps.slots[i].object->resource = 359;
            ZOID_SetObjectDef(gBattlePowerUps.slots[i].object->handle.parts.index, gObjList[359]);
            MBOX_ShowObject(gBattlePowerUps.slots[i].object->handle.scene, 0, 15);
            gBattlePowerUps.phase_b = -2.0f;
        }
    }
}
void UpdateBattleHealth(void)
{
    s32 i;
    if (gBattlePowerUps.delay == -2.0f) {
        if (!(gBattlePowerUps.slots[gBattlePowerUps.selected_delay].object->flags & 2)) {
            gBattlePowerUps.delay = Random(10.0f) + 5.0f;
            for (i = 0; i < gBattlePowerUps.count; i++) {
                if (gBattlePowerUps.slots[i].active == 4) {
                    gBattlePowerUps.slots[i].active = 1;
                }
            }
            gBattlePowerUps.slots[gBattlePowerUps.selected_delay].active = 4;
            gBattlePowerUps.slots[gBattlePowerUps.selected_delay].count = 1;
        }
    } else {
        gBattlePowerUps.delay -= gFrameSeconds;
        if (gBattlePowerUps.delay < 0.0f) {
            i = Random(gBattlePowerUps.count);
            while (gBattlePowerUps.slots[i].count != 1 || gBattlePowerUps.slots[i].active == 4) {
                if (++i >= gBattlePowerUps.count) i = 0;
            }
            gBattlePowerUps.selected_delay = i;
            gBattlePowerUps.slots[i].active = 1;
            gBattlePowerUps.slots[i].count = 4;
            gBattlePowerUps.slots[i].object->flags |= 2;
            gBattlePowerUps.slots[i].object->resource = 353;
            ZOID_SetObjectDef(gBattlePowerUps.slots[i].object->handle.parts.index, gObjList[353]);
            MBOX_ShowObject(gBattlePowerUps.slots[i].object->handle.scene, 0, 15);
            gBattlePowerUps.delay = -2.0f;
        }
    }
}
void UpdateBattlePowerUps(void) /* UpdateBattlePowerUps */
{
    UpdateBattleHealth();
    UpdateBattleShield();
    UpdateBattleInvs();
}
