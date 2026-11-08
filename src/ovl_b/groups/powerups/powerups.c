/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image B battle power-ups (MAT3 GameSource/src/powerups.c). Superseded in place by:
 * UpdateBattlePowerUps = UpdateBattlePowerUps (GC 0x80033c94) is now a strict member/claim.
 *   UpdateBattleHealth = UpdateBattleHealth (GC 0x80033618)  - inherited member, source unchanged
 *   UpdateBattleShield = UpdateBattleShield (GC 0x80033840)  - inherited member, source unchanged
 *   UpdateBattleInvs = UpdateBattleInvs   (GC 0x80033a68)  - inlined into F60; 8-byte deleted stub (member)
 *   UpdateInvs = UpdateInvs         (GC 0x800334d0)  - inlined into UpdateBattleInvs; 8-byte stub (member)
 * rewrote the two inlined bodies as MAT3 typed ports: UpdateBattleInvs calls UpdateInvs first and keeps
 * MAT3's early `return` and `else if ((invsTLeft -= gFrameSeconds) < 0.f)`; UpdateInvs keeps MAT3's
 * `fadeAlpha + 8 > 255` form and invsColor table. N64 differences: `car` is a function-level local declared
 * between i and invsColor (fixes the table home sp+56; MAT3 has it block-local), gFrameSeconds is MAT3's
 * `extern volatile float` (per-use lui/addiu + lwc1 0(reg)), and the invs-phase compare is spelled `0.f`
 * (keeps its zero web distinct from UpdateInvs' `0.0f`, as the inherited packet found). Field names below are
 * the inherited descriptive ones; MAT3 names: State = tBattlePowerUps {num, hIdx, sIdx, iIdx, healthTLeft,
 * invsTLeft, shieldTLeft, tS}, Slot = tBattlePowerUpT {t, last, type}, Player.{flags & 1 = battle.defense.active
 * invisibility, countdown = battle.shoot, alpha = battle.fadeAlpha, phase = battle.fadeState, timer = fadeTime}.
 * Inherited (runtime_b_resource_callbacks) credited for layouts and the B10/D38 bodies.
 * No unused locals.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
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
#define N64_ULONG uintptr_t /* a pointer through an integer: long is 32 bits on Windows */
extern State gBattlePowerUps;
extern f32 gFrameSeconds;
extern u16 health_powerup_def, invisibility_powerup_def, shield_powerup_def;
extern s16 gNumPlayers;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern f32 Random(f32);
extern void ZOID_SetObjectDef(s16, u16);
extern void MBOX_ShowObject(s32, s32, u32);
#define gFrameSeconds (*(volatile f32 *)&gFrameSeconds)
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
        ZOID_SetObjectDef(gBattlePowerUps.slots[i].object->handle.parts.index, invisibility_powerup_def);
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
        gBattlePowerUps.phase_b -= *(f32 *)(N64_ULONG)&gFrameSeconds;
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
            ZOID_SetObjectDef(gBattlePowerUps.slots[i].object->handle.parts.index, shield_powerup_def);
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
        gBattlePowerUps.delay -= *(f32 *)(N64_ULONG)&gFrameSeconds;
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
            ZOID_SetObjectDef(gBattlePowerUps.slots[i].object->handle.parts.index, health_powerup_def);
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
