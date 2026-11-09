#include "game/game.h"

/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * UpdateModel + func_800E6AF8 (draft, finished). Unit-EQUAL, 0 locked bodies differ.
 *
 * func_800E6AF8 = the N64 descendant of arcade mdrive.c model_iteration(): per-car speed = |vel|, then the
 * per-player model update (update_player_model / update_model), then arcade update_drone_models() with its
 * update_model() call, both inlined helpers:
 *   update_drone_models (D_80120E74 = drone_sched, sched_to_use, sched_index,
 *   D_80152808 = our_drones, cur_drone, num_our_drones = drone count) is defined BEFORE
 *   func_800E6AF8 and update_model AFTER it (arcade order differs; the definition order sets the .loc
 *   lines that as1 uses as scheduling tie-breaks: the reload of num_drones_to_do must carry a later line
 *   than the loop increment so it lands in the bnez delay slot). Both are non-static and not kept:
 *   they need unit_overrides force_internal and leave no stub (the locked stubs stub_before_model_iteration/stub_before_InitControlSys
 *   stay as they are).
 *
 * Shaping devices / disclosures (owner rules):
 *  1. UpdateModel: `if ((temp_v0 ^ 0) == 1)` -- no-code identity puts the constant first (bnel s5,v0);
 * without it 1 word differs.
 *  2. UpdateModel: compiled-out `do {} while (0);` after `new_sched_to_use -= 1;` before the early `return`:
 *     ends the basic block so uopt closes the .noalias regions of the three address registers before the
 *     `b` (a `.alias` after an unconditional `b` makes as1 give the next region's blocks weight 65534 and
 *     blocks its cross-block hoist of `lui t7,%hi(gCoastFlag)` into the lb delay slot, 3 rows; proven by
 *     deleting the six .alias lines from the listing: 0 rows). `goto ret; ret:;` works the same.
 *  3. func_800E6AF8: unused locals `curtime` (s32) and `n` (s16) declared before `i` -- exact frame residual:
 *     retail frame 160, without them 152 and 2 words off (i/j/r/t homes 124..112 need them; one alone gives
 *     frame 160 but 9 words off). Named after arcade model_iteration's locals.
 *  4. update_model: unused local `result` (s16; arcade update_model's local) -- exact frame residual of the
 *     inlined helper area: without it frame 152 and 13 words off (num_drones_to_do home 100(sp)).
 *  5. `rec = gPlayers; do {` on one source line (as1 line tie-break of the hoisted address halves: retail
 *     addiu s8 after s3/s5); 2 lines: 2 words off.
 */
float fabsf(float); /* : m2c emits fabsf/sqrtf for abs.s/sqrt.s; declare them as IDO intrinsics */
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

VIEW(s32, gGameState);
#define gGameState gGameState_view

typedef struct Inp {
    u8 car;
    u8 pad1[0x48 - 0x1]; void *ref;
} Inp;
extern Inp gPlayers[];

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
typedef struct GC2 {
    u8 pad00[0xEF];
    s8 bEF;
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x358 - 0x308];
    s8 b358;
    s8 b359;
    u8 pad35A[0x380 - 0x35A];
    void *w380;                      /* the car's input record, a host pointer */
    u8 pad384[0x3B8 - 0x384];
} GC2;

VIEW(u8, gCoastFlag);
#define gCoastFlag gCoastFlag_view

VIEW(u8, gDeathMode);
#define gDeathMode gDeathMode_view

typedef struct ModelRecord {
    u8 pad0[0x220];
    f32 vx; /* 0x220 */
    f32 vy; /* 0x224 */
    f32 vz; /* 0x228 */
    f32 x22C; /* 0x22C */
    f32 y230; /* 0x230 */
    f32 z234; /* 0x234 */
    u8 pad238[0x1B8];
    f32 speed; /* 0x3F0 */
    u8 pad3F4[0x94];
    s8 b488; /* 0x488 */
    u8 pad489[0x5B];
    s8 b4E4; /* 0x4E4 */
    u8 pad4E5[0x14F];
    f32 f634; /* 0x634 */
    f32 f638; /* 0x638 */
    u8 pad63C[0x4];
    s8 b640; /* 0x640 */
    u8 pad641[0x83];
    s16 s6C4; /* 0x6C4 */
    u8 pad6C6[0x6];
    s8 b6CC; /* 0x6CC */
    u8 pad6CD[0x43];
    s32 lastTick; /* 0x710 */
    f32 tickTime; /* 0x714 */
    f32 dt; /* 0x718 */
    u8 pad71C[0x4];
    f32 f720; /* 0x720 */
    u8 pad724[0x4];
    f32 f728; /* 0x728 */
    f32 f72C; /* 0x72C */
    u8 pad730[0x2];
    s8 b732; /* 0x732 */
    u8 pad733[0x55];
    f32 vel[3]; /* 0x788 */
    f32 pos[3]; /* 0x794 */
    u8 pad7A0[0x26];
    s16 unk7C6; /* 0x7C6 */
    u8 pad7C8[0x2];
    s16 s7CA; /* 0x7CA */
    s8 b7CC; /* 0x7CC */
    u8 pad7CD[0x11];
    s8 b7DE; /* 0x7DE */
    u8 pad7DF[0x1];
    s16 s7E0; /* 0x7E0 */
    s16 unk7E2; /* 0x7E2 */
    u8 pad7E4[0x8];
    f32 f7EC; /* 0x7EC */
    u8 pad7F0[0x18];
} ModelRecord;

VIEW(s32[36], percentTable);
#define percentTable percentTable_view
#define RUMBLE_STEPS percentTable

VIEW(s8[], gPaks);
#define gPaks gPaks_view

VIEW(intptr_t, gGhosts);
#define gGhosts gGhosts_view

/* GameCar's pad0EC[x] is the car's byte 0xEC + x; on the PC it is at CAR_OFS(0xEC + x) (docs/port/CARS.md). */
#define PAD0EC(a, x) (*((u8 *)(a) - 0xEC + CAR_OFS(0xEC + (x))))
/* A ghost (SaveInit' Ghost, 64 bytes on the PC) has its header handle at +0x38 (docs/port/CASTS.md). */
#define GHOST_HEADER 0x38
/* The controller/pak records gPaks are 0x310 bytes on the PC (paksys' PakRecord, docs/port/POINTERS.md 9.6); the
 * rumble request (rumble/group.c's Controller.request) is a record's byte at host +0x89. */
#define PAK_RECORD 0x310
#define PAK_RUMBLE_REQUEST(i) (((s8 *) gPaks)[(i) * PAK_RECORD + 0x89])
/* The drone schedule on the PC (docs/port/DECOUPLE.md 8). Row r is for n drones with one human or none (r = n) or
 * with two or more (r = PC_DRONE_SCHED_TWO + n, pc_game_types.h), n up to PC_MAX_LINKS; entry k is how many drones
 * the k-th model iteration of a round updates, -1 ending the round. Where the N64 has the row (0-6 drones with one
 * human, 0-4 with two: its rows n and 7 + n) it is read from the game's data as the N64 reads it. Past them a round
 * has as many iterations as the N64's last row of that kind (PC_N64_SCHED_ONE, PC_N64_SCHED_TWO drones), each
 * updating n / that many drones, the first n % that many one more: as even as the N64's rows split them, the larger
 * first. Each drone is then updated once a round, which is as long as the N64's longest. */
#define PC_N64_SCHED_ONE 6
#define PC_N64_SCHED_TWO 4
static s16 pc_drone_sched(s32 row, s32 k)
{
    s32 two = row >= PC_DRONE_SCHED_TWO;
    s32 n = two ? row - PC_DRONE_SCHED_TWO : row;
    s32 last = two ? 7 + PC_N64_SCHED_TWO : PC_N64_SCHED_ONE;
    s32 groups;

    if (n <= (two ? PC_N64_SCHED_TWO : PC_N64_SCHED_ONE))
        return drone_sched[two ? 7 + n : n].v[k];
    for (groups = 0; groups < 3 && drone_sched[last].v[groups] != -1; groups++)
        ;
    if (groups == 0)
        groups = 1;
    if (k >= groups)
        return -1;
    return (s16) (n / groups + (k < n % groups));
}
#define PC_DRONE_SCHED(row, k) pc_drone_sched(row, k)
void UpdateModel(s32 ipa_s4) {
    GameCar *temp_s2;
    f32 temp_f0;
    f32 var_f0;
    s32 temp_a0;
    s32 idx;
    s32 *var_v0_2;
    s32 temp_a0_2;
    s32 temp_f6;
    s32 temp_lo;
    s32 temp_v0_2;
    intptr_t temp_v1;
    s32 var_a0;
    s8 temp_v0;
    s32 var_v1;
    u8 temp_v0_3;
    void *temp_s0;
    void *temp_s0_2;
    void *temp_s3;

    temp_s2 = PC_CAR(ipa_s4);
    temp_s3 = M2C_FIELD(temp_s2, void **, CAR_OFS(0x380));
    if (((s8) PAD0EC(temp_s2->pad0EC, 0x26D) == 1) && ((temp_s0 = (ModelRecord *) ((ipa_s4 * 0x808) + (u8 *) PC_MODELS), (M2C_FIELD(temp_s0, f32 *, 0x3F0) < 5.0f)) || (M2C_FIELD(temp_s0, s16 *, 0x71C) == 0))) {
        *(s8 *)&PAD0EC(temp_s2->pad0EC, 0x26D) = 2;
        PAD0EC(temp_s2->pad0EC, 0x26C) = 0;
        M2C_FIELD(temp_s0, s8 *, 0x640) = 0;
        M2C_FIELD(temp_s0, s16 *, 0x71C) = 0;
        M2C_FIELD(temp_s0, f32 *, 0x3F0) = 0.0f;
        if (temp_s3 != NULL) {
            if (new_sched_to_use >= PC_DRONE_SCHED_TWO) {
                new_sched_to_use -= PC_DRONE_SCHED_TWO;
            }
            temp_v0_3 = M2C_FIELD(temp_s3, u8 *, 1);
            PAK_RUMBLE_REQUEST(temp_v0_3) = 0;
            return;
        }
        idx = cur_drone;
        var_a0 = num_our_drones - 1;
        for (; idx < var_a0; idx++) {
            PC_OUR_DRONE(idx) = PC_OUR_DRONE(idx + 1);
        }
        num_our_drones = var_a0;
        cur_drone -= 1;
        new_sched_to_use -= 1;
        do {} while (0);
        return;
    }
    temp_s0_2 = (ModelRecord *) ((ipa_s4 * 0x808) + (u8 *) PC_MODELS);
    CheckCrash(ipa_s4);
    FinishResurrecting(temp_s0_2);
    if (((s8) PAD0EC(temp_s2->pad0EC, 0x26C) == 2) && ((s8) PAD0EC(temp_s2->pad0EC, 0x26D) <= 0)) {
        End_MDrive(ipa_s4);
        Init_MDrive(2, ipa_s4);
    } else if ((M2C_FIELD(temp_s0_2, s8 *, 0x640) != 0) && ((s8) PAD0EC(temp_s2->pad0EC, 0x26D) >= 2)) {
        M2C_FIELD(temp_s0_2, s8 *, 0x640) = 0;
    }
    temp_f0 = (M2C_FIELD(temp_s0_2, f32 *, 0x400) * M2C_FIELD(temp_s0_2, f32 *, 0x7F0)) * (((f32) M2C_FIELD(temp_s0_2, s16 *, 0x624) * roadboost_time_scale) + 1.0f);
    M2C_FIELD(temp_s0_2, f32 *, 0x7F4) = temp_f0;
    M2C_FIELD(temp_s0_2, f32 *, 0x718) = (f32) (M2C_FIELD(temp_s0_2, f32 *, 0x718) * temp_f0);
    if (M2C_FIELD(temp_s0_2, s16 *, 0x6C4) >= 0) {
        if (temp_s3 != NULL) {
            UpdateResurrectingCar(temp_s0_2);
            if (gGameState & 0x400000) {
                temp_v0_3 = M2C_FIELD(temp_s3, u8 *, 1);
            PAK_RUMBLE_REQUEST(temp_v0_3) = 0;
            }
        } else {
            UpdateResurrectingCar(temp_s0_2);
        }
        update_reckon_base((s32) ipa_s4, M2C_FIELD(temp_s0_2, s32 *, 0x714));
    } else if (M2C_FIELD(temp_s0_2, s16 *, 0x71C) != 0) {
        if (temp_s3 != NULL) {
            sym(temp_s0_2);
            if ((gGameState & 0x400000) && ((gRace != 2) || (temp_v1 = (&gGhosts)[M2C_FIELD(temp_s0_2, s16 *, 0x7C6)], (temp_v1 == 0)) || (M2C_FIELD(*M2C_FIELD(M2C_FIELD(temp_v1, void **, 0), void ***, GHOST_HEADER), s8 *, 5) < 0))) {
                temp_f6 = (s32) ((fabsf(M2C_FIELD(temp_s0_2, f32 *, 0x10)) + fabsf(M2C_FIELD(temp_s0_2, f32 *, 0x14))) + fabsf(M2C_FIELD(temp_s0_2, f32 *, 0x18)));
                var_a0 = temp_f6;
                if (gRace == 6) {
                    var_a0 /= 2;
                    if ((s8) PAD0EC(temp_s2->pad0EC, 0x26C) == 1) {
                        var_a0 += 0x186A0;
                    } else {
                        var_a0 += (M2C_FIELD(temp_s2, s16 *, CAR_OFS(0x388)) * 0x186A0) / 800;
                        M2C_FIELD(temp_s2, s16 *, CAR_OFS(0x388)) = 0;
                    }
                }
                var_v0_2 = RUMBLE_STEPS;
                var_v1 = 0;
loop_34:
                if (var_a0 >= *var_v0_2) {
                    var_v1 += 1;
                    var_v0_2 = var_v0_2 + 1;
                    if (var_v1 < 0x24) {
                        goto loop_34;
                    }
                }
                temp_v0 = M2C_FIELD(*M2C_FIELD(temp_s3, void ***, 0x48), s8 *, PROFILE_OFS(0x2B));
                if (temp_v0 == 0) {
                    goto block_40;
                }
                if ((temp_v0 ^ 0) == 1) {
                    var_v1 -= 4;
                    if (var_v1 < 0) {
block_40:
                        var_v1 = 0;
                    }
                }
                temp_a0 = M2C_FIELD(temp_s3, u8 *, 1);
                PAK_RUMBLE_REQUEST(temp_a0) = var_v1;
            }
        } else {
            MaxPathControls(temp_s0_2);
            sym(temp_s0_2);
        }
        update_reckon_base((s32) ipa_s4, M2C_FIELD(temp_s0_2, s32 *, 0x714));
    }
    if ((gRace != 2) || (M2C_FIELD(temp_s0_2, s16 *, 0x7C6) == 0)) {
        UpdateCarSounds(temp_s0_2);
    }
    if (M2C_FIELD(temp_s0_2, s8 *, 0x640) != 0) {
        M2C_FIELD(temp_s0_2, s32 *, 0x7D4) = (s32) (M2C_FIELD(temp_s0_2, s32 *, 0x7D4) | 0x70);
    } else {
        temp_v0_2 = M2C_FIELD(temp_s0_2, s32 *, 0x7D4);
        if (!(temp_v0_2 & 0x10)) {
            M2C_FIELD(temp_s0_2, s32 *, 0x7D4) = (s32) (temp_v0_2 & ~0x60);
        }
    }
    if (M2C_FIELD(temp_s0_2, s8 *, 0x7EB) == 0) {
        if (M2C_FIELD(temp_s0_2, s16 *, 0x6C4) >= 0) {
            var_f0 = 0.0f;
            M2C_FIELD(temp_s2, f32 *, CAR_OFS(0x30C)) = M2C_FIELD(temp_s0_2, f32 *, 0x714);
        } else {
            var_f0 = M2C_FIELD(temp_s0_2, f32 *, 0x714) - M2C_FIELD(temp_s2, f32 *, CAR_OFS(0x30C));
        }
        /* Original game: `(gRace != 6) || (gRace != 5)` (not battle or not obstacle) is always true, so every mode
         * takes the 5-second solid delay as well, and battle and obstacle become solid at whichever of the two
         * delays comes first; `&&` was meant. MAT3 marks the same line as a bug. Kept as retail. */
        if ((((s8) gCoastFlag != 0) || (((gRace != 6) || (gRace != 5)) && (var_f0 >= 5.0f)) || (((gRace == 6) || (gRace == 5)) && (solid_delay_arena < var_f0))) && ((temp_v0_3 = PC_LINK_OWNER(M2C_FIELD(temp_s0_2, s16 *, 0x7C6)), (temp_v0_3 == 0)) || (temp_v0_3 == 6))) {
            M2C_FIELD(temp_s0_2, s8 *, 0x7EA) = 1;
            M2C_FIELD(temp_s0_2, s8 *, 0x7EB) = 1;
            M2C_FIELD(temp_s2, f32 *, CAR_OFS(0x30C)) = 0.0f;
        }
    }
}

void update_drone_models(void) {
    s16 i, num_drones_to_do;

    if (PC_DRONE_SCHED(sched_to_use, ++sched_index) == -1) {
        sched_index = 0;
        sched_to_use = new_sched_to_use;
    }
    num_drones_to_do = PC_DRONE_SCHED(sched_to_use, sched_index);
    if (num_drones_to_do == 0) {
        return;
    }
    for (i = 0; i < num_drones_to_do; i++) {
        update_model(PC_MODEL(PC_OUR_DRONE(cur_drone)));
        cur_drone++;
        if (cur_drone >= num_our_drones) {
            cur_drone = 0;
        }
    }
}


void model_iteration(void) {
    f32 fz, fx, fy;
    s32 v;
    ModelRecord *car;
    GC2 *gc;
    s32 curtime;
    s16 n;
    s32 i;
    s32 j;
    s32 r;
    f32 t;
    Inp *rec;

    if ((gGameState & 0x600008) && (modelrun != 0)) {
        gModelIteration = gModelIteration + 1;
        gModelTime = (f32) gModelIteration * gRetraceTime;
        for (v = 0; v != PC_RACE_CARS; v++) {
            if (PC_CAR_INPUT(v) == 0) {
                car = PC_MODEL(v);
                if (car->s7CA == 0) {
                    fz = car->vel[2];
                    fx = car->vel[0];
                    fy = car->vel[1];
                    car->speed = sqrtf((fz * fz) + ((fx * fx) + (fy * fy)));
                }
            }
        }
        UpdateModelControls();
        i = 0;
        if (num_humans > 0) {
            rec = gPlayers; do {
                car = PC_MODEL(rec->car);
                j = 0;
                for (;;) {
                    if (gRace == 2) {
                        r = GhostUpdate(i, &t);
                        if (r < 0) {
                            break;
                        }
                        car->dt = t;
                        car->lastTick += 1;
                        car->tickTime = (f32) car->lastTick * t;
                    } else {
                        car->dt = (f32) (gModelIteration - car->lastTick) * gRetraceTime;
                        car->lastTick = gModelIteration;
                        car->tickTime = gModelTime;
                    }
                    SetClutch(car);
                    if ((car->b732 != 0) && ((gRace != 6) || (car->b640 != 0)) && (gGameState & 0x400000) && (car->s6C4 == -1)) {
                        gc = &((GC2 *) PC_CARS)[rec->car];
                        if ((gc->bEF == 0) && (gc->b359 == 0) && ((s8) gCoastFlag == 0) && (((s8) gDeathMode == 0) || ((car->speed < 10.0f) && (gc->b358 == 0)))) {
                            if (gRace == 4) {
                                CrashedStunt(i);
                            }
                            car->b6CC = 1;
                            AbortCar(rec->car, 0);
                        }
                    }
                    if (PC_CAR_STATE(rec->car) < 2) {
                        UpdateModel(rec->car);
                    } else {
                        collision(car);
                    }
                    if ((gRace == 2) && (j == 0) && (r > 0)) {
                        j = 1;
                        continue;
                    }
                    break;
                }
                i += 1;
                rec++;
            } while (i < num_humans);
        }
        update_drone_models();
        if (cur_drone == 0) {
            dead_reckon_all();
        }
        UpdateListeners();
    }
}



void update_model(ModelRecord *car) {
    s16 result;

    car->dt = (f32) (gModelIteration - car->lastTick) * gRetraceTime;
    car->lastTick = gModelIteration;
    car->tickTime = gModelTime;
    UpdateModel(car->unk7C6);
}

