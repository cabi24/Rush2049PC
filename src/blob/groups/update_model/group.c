/* group.c: one step of the car simulation for every car (MAT3 mdrive.c: model_iteration(), _UpdateModel() and the
 * statics _UpdateLinkCars(), _UpdatePlayerModel(), _UpdateDroneModels()).
 *
 * The model thread calls model_iteration once a retrace (modelthread.c). Each human's car is stepped every time
 * (in a ghost race the ghost cars replay their recordings: GhostUpdate), the drones this machine simulates a few
 * at a time by the drone schedule (update_drone_models), and every car's step is UpdateModel: the death-mode
 * coast-out, the crash and respawn states, then either the respawn's flight or the driving physics (sym), the
 * Rumble Pak, the engine sound, the wreck's smoke, and the delay before a respawned car is solid again. */
#include "game/game.h"

float fabsf(float);
float sqrtf(float);

/* The players' input records (tPadState: the car each drives, its controller, its profile). */
extern tPadState gPlayers[];

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields used here. */
typedef struct tCarData {
    u8 pad00[0xEF];
    s8 place_locked;            /* 0x0EF finished */
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x30C - 0x308];
    f32 collide_time;           /* 0x30C the model time it stopped being solid */
    u8 pad310[0x358 - 0x310];
    s8 crash_state;             /* 0x358 CRASH_NONE, CRASH_WRECKED, CRASH_RESPAWN */
    s8 dead;                    /* 0x359 CAR_ABORTED, CAR_ALIVE, CAR_DYING, CAR_DEAD */
    u8 pad35A[0x380 - 0x35A];
    tPadState *player;          /* 0x380 the car's input record, a host pointer; NULL for a drone */
    u8 pad384[0x388 - 0x384];
    s16 damageThisFrame;        /* 0x388 battle.damageThisFrame */
    u8 pad38A[0x3B8 - 0x38A];
} tCarData;

/* The rumble's strength steps (MAT3 percentTable[36]): the force sums at which the next step starts. */
#define RUMBLE_STEPS percentTable

VIEW(intptr_t, gGhosts);
#define gGhosts gGhosts_view

/* A ghost (SaveInit' Ghost, 64 bytes on the PC) has its header handle at +0x38 (docs/port/CASTS.md); a ghost race's
 * ghost car `g` (gGhosts[slot]) is recording if its header's flag (+5) is negative: MAT3
 * (**(**gGhosts[slot]).header).flag. */
#define GHOST_HEADER 0x38
#define GHOST_HEADER_FLAG(g) (*(s8 *) ((s8 *) **(void ***) ((s8 *) *(void **) (g) + GHOST_HEADER) + 5))
/* The controller/pak records gPaks are 0x310 bytes on the PC (paksys' PakRecord, docs/port/POINTERS.md 9.6); the
 * rumble request (rumble/group.c's Controller.request) is a record's byte at host +0x89. */
#define PAK_RECORD 0x310
#define PAK_RUMBLE_REQUEST(i) (((s8 *) gPaks)[(i) * PAK_RECORD + 0x89])
/* A profile's rumble setting (save.c's profile record, N64 +0x2B, PROFILE_OFS on the PC): 0 off, 1 low, else full. */
#define PROFILE_RUMBLE(profile) (*(s8 *) ((s8 *) (profile) + PROFILE_OFS(0x2B)))
/* The drone schedule on the PC (docs/port/DECOUPLE.md 8). Row r is for n drones with one human or none (r = n) or
 * with two or more (r = PC_DRONE_SCHED_TWO + n, pc_game_types.h), n up to PC_MAX_LINKS; entry k is how many drones
 * the k-th model iteration of a round updates, -1 ending the round. Where the N64 has the row (0-6 drones with one
 * human, 0-4 with two: its rows n and 7 + n) it is read from the game's data as the N64 reads it. Past them a round
 * has as many iterations as the N64's last row of that kind (PC_N64_SCHED_ONE, PC_N64_SCHED_TWO drones), each
 * updating n / that many drones, the first n % that many one more: as even as the N64's rows split them, the larger
 * first. Each drone is then updated once a round, which is as long as the N64's longest. */
#define PC_N64_SCHED_ONE 6
#define PC_N64_SCHED_TWO 4
/* Entry k of the drone schedule's row `row`: how many drones the k-th step of a round updates, -1 the round's
 * end. */
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

/* One step of car `slot` (MAT3 _UpdateModel).
 *
 * A car coasting out of a death-mode race (CAR_DYING) that has slowed below 5 feet a second, or stopped running,
 * is dead (CAR_DEAD): no crash, no speed, not simulated; a human's leaves the two-human drone schedules and its
 * rumble stops, a drone leaves this machine's drone list and schedule. Else: the crash state (CheckCrash), the end
 * of a respawn's flight (FinishResurrecting), a car due to respawn is set up again at its respawn point
 * (End_MDrive, Init_MDrive INIT_MODE_CRASH), a dead car's crash is cleared. The car's time scale (time_fudge) is
 * the catch-up's times its time boost times the boost pad under tire 0 (roadboost[0] x roadboost_time_scale + 1),
 * and its step is stretched by it, so a boosted or catching-up car's physics runs faster. Then a respawning car
 * flies (UpdateResurrectingCar), a running one drives: a human's by its controls (sym) and while racing with the
 * Rumble Pak (not for a ghost race's recording ghost) at the step of percentTable its body force reaches (in a
 * battle half of it, plus 100,000 if wrecked or else its damage this frame scaled to 100,000 for 800), four steps
 * fewer on its profile's low setting, none when off; a drone's by the drone driver (MaxPathControls, sym). Both
 * update the dead-reckoning base. The engine sound (not for a ghost race's ghosts), the wreck's look (APPEAR_CRASHED
 * while crashed, the smoke cleared once it isn't a hulk), and a car that isn't solid becomes solid (and
 * collidable) after five seconds, or in a battle or obstacle race solid_delay_arena (MAT3 0.6) seconds, or once
 * the race is over, if a drone or a human drives it. */
void UpdateModel(s32 slot) {
    tCarData *car;
    f32 time_fudge;
    f32 since;
    s32 controller;
    s32 idx;
    s32 *step_force;
    s32 unused1;            /* unused */
    s32 force;
    s32 unused2;            /* unused */
    s32 appearance;
    intptr_t ghost;
    s32 work;               /* the last drone's index, then the rumble's force */
    s8 rumble_setting;
    s32 step;
    u8 id;                  /* a controller's number, or the car slot's owner */
    tModelData *dying;
    tModelData *m;
    tPadState *player;

    car = PC_CAR(slot);
    player = car->player;
    if (((s8) car->dead == CAR_DYING) && ((dying = &pc_models[slot], (dying->magvel < 5.0f)) || (dying->fastin.modelrun == 0))) {
        *(s8 *)&car->dead = CAR_DEAD;
        car->crash_state = CRASH_NONE;
        dying->crashflag = 0;
        dying->fastin.modelrun = 0;
        dying->magvel = 0.0f;
        if (player != NULL) {
            /* a human is out: back to the one-human schedules */
            if (new_sched_to_use >= PC_DRONE_SCHED_TWO) {
                new_sched_to_use -= PC_DRONE_SCHED_TWO;
            }
            id = player->controller;
            PAK_RUMBLE_REQUEST(id) = 0;
            return;
        }
        /* a drone is out: one drone fewer in this machine's list and schedule */
        idx = cur_drone;
        work = num_our_drones - 1;
        for (; idx < work; idx++) {
            PC_OUR_DRONE(idx) = PC_OUR_DRONE(idx + 1);
        }
        num_our_drones = work;
        cur_drone -= 1;
        new_sched_to_use -= 1;
        return;
    }
    m = &pc_models[slot];
    CheckCrash(slot);
    FinishResurrecting(m);
    if (((s8) car->crash_state == CRASH_RESPAWN) && ((s8) car->dead <= CAR_ALIVE)) {
        End_MDrive(slot);
        Init_MDrive(INIT_MODE_CRASH, slot);
    } else if ((m->crashflag != 0) && ((s8) car->dead >= CAR_DEAD)) {
        m->crashflag = 0;
    }
    time_fudge = (m->catchup * m->time_boost) * (((f32) m->roadboost[0] * roadboost_time_scale) + 1.0f);
    m->time_fudge = time_fudge;
    m->fastin.dt = (f32) (m->fastin.dt * time_fudge);
    if (m->resurrect.moving_state >= RES_START) {
        if (player != NULL) {
            UpdateResurrectingCar(m);
            if (gGameState & GS_RACING) {
                id = player->controller;
                PAK_RUMBLE_REQUEST(id) = 0;
            }
        } else {
            UpdateResurrectingCar(m);
        }
        update_reckon_base((s32) slot, *(s32 *) &m->fastin.modelTime);
    } else if (m->fastin.modelrun != 0) {
        if (player != NULL) {
            sym(m);
            if ((gGameState & GS_RACING) && ((gRace != kGhost) || (ghost = (&gGhosts)[m->slot], (ghost == 0)) || (GHOST_HEADER_FLAG(ghost) < 0))) {
                force = (s32) ((fabsf(m->f[0]) + fabsf(m->f[1])) + fabsf(m->f[2]));
                work = force;
                if (gRace == kBattle) {
                    work /= 2;
                    if ((s8) car->crash_state == CRASH_WRECKED) {
                        work += 100000;
                    } else {
                        work += (car->damageThisFrame * 100000) / 800;
                        car->damageThisFrame = 0;
                    }
                }
                step_force = RUMBLE_STEPS;
                step = 0;
next_step:
                if (work >= *step_force) {
                    step += 1;
                    step_force = step_force + 1;
                    if (step < 36) {
                        goto next_step;
                    }
                }
                rumble_setting = PROFILE_RUMBLE(*player->save);
                if (rumble_setting == 0) {
                    goto rumble_off;
                }
                if (rumble_setting == 1) {
                    step -= 4;
                    if (step < 0) {
rumble_off:
                        step = 0;
                    }
                }
                controller = player->controller;
                PAK_RUMBLE_REQUEST(controller) = step;
            }
        } else {
            MaxPathControls(m);
            sym(m);
        }
        update_reckon_base((s32) slot, *(s32 *) &m->fastin.modelTime);
    }
    if ((gRace != kGhost) || (m->slot == 0)) {   /* kGhost: only the player's car */
        UpdateCarSounds(m);
    }
    if (m->crashflag != 0) {
        m->appearance = (s32) (m->appearance | APPEAR_CRASHED);
    } else {
        appearance = m->appearance;
        if (!(appearance & APPEAR_HULK)) {
            m->appearance = (s32) (appearance & ~APPEAR_WRECK_SMOKE);
        }
    }
    if (m->solid == 0) {
        if (m->resurrect.moving_state >= RES_START) {
            since = 0.0f;
            car->collide_time = m->fastin.modelTime;
        } else {
            since = m->fastin.modelTime - car->collide_time;
        }
        /* Original game: `(gRace != 6) || (gRace != 5)` (not battle or not obstacle) is always true, so every mode
         * takes the 5-second solid delay as well, and battle and obstacle become solid at whichever of the two
         * delays comes first; `&&` was meant. MAT3 marks the same line as a bug. Kept as retail. */
        if (((gCoastFlag != 0) || (((gRace != kBattle) || (gRace != kObstacle)) && (since >= 5.0f)) || (((gRace == kBattle) || (gRace == kObstacle)) && (solid_delay_arena < since))) && ((id = PC_LINK_OWNER(m->slot), (id == 0)) || (id == 6))) {   /* kBattle, kObstacle; LINK_OWNER_DRONE, LINK_OWNER_HUMAN */
            m->collidable = 1;
            m->solid = 1;
            car->collide_time = 0.0f;
        }
    }
}

/* Step the drones this machine simulates (MAT3 _UpdateDroneModels): the drone schedule's next entry (a round of
 * entries in row sched_to_use; a new round takes the row new_sched_to_use asks for) is how many of them to step
 * this time, going round this machine's drone list from cur_drone. */
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
        update_model(&pc_models[PC_OUR_DRONE(cur_drone)]);
        cur_drone++;
        if (cur_drone >= num_our_drones) {
            cur_drone = 0;
        }
    }
}


/* One step of the simulation, while the countdown, the race or the attract mode runs and modelrun lets it: the
 * model clock moves on one retrace (gRetraceTime); the drones another machine drives get their speed from their
 * dead-reckoned velocity (MAT3 _UpdateLinkCars); the controls are read into the models (UpdateModelControls);
 * each human's car is stepped (MAT3 _UpdatePlayerModel): its step's time since its last one, or in a ghost race a
 * ghost's recorded step (GhostUpdate: none when negative, and a second step when it says so), the clutch, the
 * abort button (respawns a car that hasn't finished and is alive, outside the race's end, and in death mode only
 * when slow and not wrecked; a stunt race counts it as a crash), and UpdateModel, or only the collisions for a dead
 * car; then the drones (update_drone_models), the dead-reckoning snapshots once the drones have gone round, and
 * the sound listeners. */
void model_iteration(void) {
    f32 fz, fx, fy;
    s32 v;
    tModelData *m;
    tCarData *gc;
    s32 curtime;            /* unused */
    s16 n;                  /* unused */
    s32 i;
    s32 j;
    s32 r;
    f32 t;
    tPadState *rec;

    if ((gGameState & (GS_RACING | GS_COUNTDOWN | GS_DEMO)) && (modelrun != 0)) {
        gModelIteration = gModelIteration + 1;
        gModelTime = (f32) gModelIteration * gRetraceTime;
        for (v = 0; v != PC_RACE_CARS; v++) {
            if (PC_CAR_INPUT(v) == 0) {
                m = &pc_models[v];
                if (m->we_control == 0) {
                    fz = m->reckon.rwv[2];
                    fx = m->reckon.rwv[0];
                    fy = m->reckon.rwv[1];
                    m->magvel = sqrtf((fz * fz) + ((fx * fx) + (fy * fy)));
                }
            }
        }
        UpdateModelControls();
        i = 0;
        if (num_humans > 0) {
            rec = gPlayers;
            do {
                m = &pc_models[rec->slot];
                j = 0;
                for (;;) {
                    if (gRace == kGhost) {
                        r = GhostUpdate(i, &t);
                        if (r < 0) {
                            break;
                        }
                        m->fastin.dt = t;
                        m->fastin.modelIteration += 1;
                        m->fastin.modelTime = (f32) m->fastin.modelIteration * t;
                    } else {
                        m->fastin.dt = (f32) (gModelIteration - m->fastin.modelIteration) * gRetraceTime;
                        m->fastin.modelIteration = gModelIteration;
                        m->fastin.modelTime = gModelTime;
                    }
                    SetClutch(m);
                    if ((m->mainin.abort != 0) && ((gRace != kBattle) || (m->crashflag != 0)) && (gGameState & GS_RACING) && (m->resurrect.moving_state == RES_IDLE)) {
                        gc = &((tCarData *) PC_CARS)[rec->slot];
                        if ((gc->place_locked == 0) && (gc->dead == CAR_ALIVE) && (gCoastFlag == 0) && (((s8) gDeathMode == 0) || ((m->magvel < 10.0f) && (gc->crash_state == CRASH_NONE)))) {
                            if (gRace == kStunt) {
                                CrashedStunt(i);
                            }
                            m->resurrect.autoAbort = 1;
                            AbortCar(rec->slot, 0);
                        }
                    }
                    if (PC_CAR_DEAD(rec->slot) < CAR_DEAD) {
                        UpdateModel(rec->slot);
                    } else {
                        collision(m);
                    }
                    if ((gRace == kGhost) && (j == 0) && (r > 0)) {
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



/* Step drone model `m` (MAT3 update_model): its step's time since its last one, then UpdateModel. */
void update_model(tModelData *m) {
    s16 result;             /* unused */

    m->fastin.dt = (f32) (gModelIteration - m->fastin.modelIteration) * gRetraceTime;
    m->fastin.modelIteration = gModelIteration;
    m->fastin.modelTime = gModelTime;
    UpdateModel(m->slot);
}
