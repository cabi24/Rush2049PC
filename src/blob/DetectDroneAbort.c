/* DetectDroneAbort.c: abort stuck and fallen cars, and the wrong-way angle (MAT3 resurrect.c, DetectDroneAbort()).
 *
 * Once a frame from five seconds into the race: a car this machine drives (a drone it simulates, or a human) that
 * has fallen out of the world, or that is stuck, goes back to the track (AbortCar, or the respawn set up here);
 * and for each human player who has not made progress for two seconds, the angle between the car's heading and the
 * road ahead, which the HUD shows as WRONG WAY past 90 degrees. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    u8 pad0[0x3F0];
    f32 magvel;                 /* 0x3F0 the speed, feet a second */
    u8 pad3F4[0x6C4 - 0x3F4];
    s16 resurrect_moving_state; /* 0x6C4 resurrect.moving_state: RES_IDLE when not respawning */
    u8 pad6C6[0x6CC - 0x6C6];
    s8 resurrect_autoAbort;     /* 0x6CC resurrect.autoAbort: the game, not a crash, aborted it */
    u8 pad6CD[0x714 - 0x6CD];
    f32 modelTime;              /* 0x714 fastin.modelTime: the model thread's race clock, seconds */
    u8 pad718[0x7C6 - 0x718];
    s16 slot;                   /* 0x7C6 */
    s16 in_game;                /* 0x7C8 */
    s16 we_control;             /* 0x7CA this machine simulates it */
    u8 pad7CC[0x7D8 - 0x7CC];
    f32 stuck_time;             /* 0x7D8 when it became stuck (0: not stuck) */
    u8 pad7DC[0x7E0 - 0x7DC];
    s16 please_abort_me;        /* 0x7E0 the drone code asks for an abort (maxpath) */
    u8 pad7E2[0x7EC - 0x7E2];
    f32 drone_scale;            /* 0x7EC */
    u8 pad7F0[0x808 - 0x7F0];
} tModelData;

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields used here. */
typedef struct {
    u8 pad0[8];
    f32 dr_pos[3];              /* 0x08 the position, feet */
    u8 pad14[0x2C - 0x14];
    f32 dr_uvs[3][3];           /* 0x2C the orientation */
    u8 pad50[0xEF - 0x50];
    s8 place_locked;            /* 0xEF finished: its place is final */
    u8 padF0[0xFA - 0xF0];
    s16 lastResPath;            /* 0xFA the resurrection path's branch it was last near, -1 the main path */
    s16 lastResIdx;             /* 0xFC and the point on it */
    s16 resIdx;                 /* 0xFE */
    f32 distance;               /* 0x100 the distance along the track */
    f32 last_distance;          /* 0x104 the furthest distance so far */
    u8 pad108[0x110 - 0x108]; struct pc_visual visuals[21]; u8 pad108_308[0x340 - 0x308];
    f32 fwd_progress_t;         /* 0x340 when it stopped making progress (0: it is making progress) */
    u8 pad344[0x358 - 0x344];
    s8 crash_state;             /* 0x358 CRASH_NONE, CRASH_WRECKED, CRASH_RESPAWN */
    s8 dead;                    /* 0x359 CAR_ABORTED, CAR_ALIVE, CAR_DYING, CAR_DEAD */
    u8 pad35A[0x364 - 0x35A];
    f32 strayAngle;             /* 0x364 the angle from its heading to the road ahead, radians */
    u8 pad368[0x380 - 0x368]; void *input; u8 pad368_384[0x3B8 - 0x384];
} tCarData;

/* A player's input record (MAT3 tPadState, gPlayers; 80 bytes on the PC), the fields used here. */
typedef struct tPadState {
    u8 slot;                    /* 0x00 the car it drives */
    u8 pad1[7];
    s32 levels;                 /* 0x08 the buttons held */
    u8 padC[0x2C - 0xC];
    s32 control_reverse;        /* 0x2C controls[5]: the reverse control's buttons */
    u8 pad30[0x48 - 0x30]; void *ref;
} tPadState;

/* A branch of the resurrection path (MAT3 anon_resurrect_firstPath_10): its points. */
typedef struct {
    u8 pad0[0xC];
    s16 (*pts)[3];
} tResPathAlt;

/* The resurrection path (MAT3 gResPath, anon_numPts_pts_10): the main path's points and its branches. */
typedef struct {
    u8 pad0[4];
    s16 (*pts)[3];
    u8 pad8[4];
    tResPathAlt *alt;
} tResPath;

extern tPadState gPlayers[];
VIEW(tResPath, gResPath);
#define gResPath gResPath_view

/* Abort cars that fell out of the world or are stuck, and measure each human's wrong-way angle.
 *
 * Before five seconds into the race it only clears every car's stuck time, progress time and angle. Then, for each
 * car this machine controls: below y -190 feet, still in the race and not respawning, it is aborted
 * (resurrect.autoAbort, AbortCar) and the check ends for this frame. Outside stunt, obstacle and battle races, while
 * racing (or in the attract mode), not in practice, with auto-abort on, not at the race's end and not coasting
 * after it, a solid car that hasn't finished, at a drone scale above 0.1 and either slower than 25 feet a second or
 * asked to by the drone code, is stuck: its first such frame's time is kept, and six seconds later (12 while its
 * player holds the reverse control) it is aborted: its respawn point is where it is (SetResurrectionData,
 * INIT_MODE_CRASH) and it respawns (CRASH_RESPAWN, CAR_ABORTED). A car not stuck has its stuck time cleared.
 * (MAT3 lost the timer: only the drone code's request aborts a car there; the N64 has it.)
 *
 * Then for each human player: progress (a larger distance than ever) clears the angle; a car that is not solid has
 * no angle; after two seconds without progress the angle is the one from the car's heading to the resurrection
 * path's point five ahead of the one it was last near, in its own frame (fatan2 of x over z). */
void DetectDroneAbort(void) {
    s16 i;
    s16 node;
    s16 abort_me;
    s16 index1;     /* unused */
    s16 index2;     /* unused */
    f32 dist1;      /* unused */
    f32 dist2;      /* unused */
    f32 pos[3];     /* unused */
    f32 pos2[3];
    f32 delta[3];
    f32 angle;      /* unused */
    s32 wait_time;
    tModelData *m;
    tCarData *gc;
    s32 path;
    s32 idx;

    if (gModelTime < 5.0f) {
        for (i = 0; i < PC_RACE_CARS; i++) {
            PC_MODEL_STUCK_TIME(i) = 0.0f;
            PC_CAR_FWD_PROGRESS(i) = 0.0f;
            PC_CAR_HEADING(i) = 0.0f;
        }
        return;
    }

    for (i = 0; i < gNumActiveCars; i++) {
        node = PC_MODEL_SLOT(i);
        m = PC_MODEL(node);
        gc = PC_CAR(node);
        if (m->we_control || PC_LINK_OWNER(node) == 6) {   /* LINK_OWNER_HUMAN */
            abort_me = m->please_abort_me;
            /* fell out of the world */
            if (gc->dr_pos[1] < -190.0f && gc->dead == CAR_ALIVE && gc->crash_state == CRASH_NONE &&
                m->resurrect_moving_state == RES_IDLE) {
                m->resurrect_autoAbort = 1;
                AbortCar(m->slot, 0);
                return;
            }
            if (gRace == 4 || gRace == 5 || gRace == 6) {   /* kStunt, kObstacle, kBattle */
                continue;
            }
            if ((gGameState & 0x400008) && gRace != 1 &&   /* GS_RACING | GS_DEMO; kPractice */
                !gNoAutoAbort && !gEndGame && !gCoastFlag &&
                gc->place_locked != 1 && AreWeSolid(m->slot) && m->drone_scale > 0.1f &&
                (m->magvel < 25.0f || abort_me)) {
                if (!abort_me) {
                    if (m->stuck_time == 0.0f) {
                        m->stuck_time = m->modelTime;
                    } else {
                        wait_time = 1;
                        if (gPlayers[node].levels & gPlayers[node].control_reverse) {
                            wait_time = 2;
                        }
                        if (wait_time != 4) {   /* always: wait_time is 1 or 2 */
                            if (m->modelTime - m->stuck_time > 6.0f * wait_time) {
                                abort_me = 1;
                            }
                        } else {
                            m->stuck_time = m->modelTime;
                        }
                    }
                }
                if (abort_me) {
                    SetResurrectionData(INIT_MODE_CRASH, m, 1);
                    gc->crash_state = CRASH_RESPAWN;
                    gc->dead = 0xFF;   /* CAR_ABORTED */
                    m->stuck_time = 0.0f;
                    m->resurrect_autoAbort = 1;
                }
            } else {
                m->stuck_time = 0.0f;
            }
        }
    }

    for (node = 0; node < gNumPlayers; node++) {
        m = PC_MODEL(gPlayers[node].slot);
        gc = PC_CAR(gPlayers[node].slot);
        if (gc->last_distance < gc->distance) {
            gc->fwd_progress_t = 0.0f;
            gc->last_distance = gc->distance;
            gc->strayAngle = 0.0f;
        } else if (!AreWeSolid(m->slot)) {
            gc->fwd_progress_t = 0.0f;
            gc->strayAngle = 0.0f;
        } else if (gc->fwd_progress_t == 0.0f) {
            gc->fwd_progress_t = m->modelTime;
        } else if (m->modelTime - gc->fwd_progress_t > 2.0f) {
            path = gc->lastResPath;
            idx = gc->lastResIdx;
            for (i = 0; i < 5; i++) {
                NextResPathPt(path, idx, &path, &idx);
            }
            if (path >= 0) {
                delta[0] = gResPath.alt[path].pts[idx][0] - gc->dr_pos[0];
                delta[1] = gResPath.alt[path].pts[idx][1] - gc->dr_pos[1];
                delta[2] = gResPath.alt[path].pts[idx][2] - gc->dr_pos[2];
            } else {
                delta[0] = gResPath.pts[idx][0] - gc->dr_pos[0];
                delta[1] = gResPath.pts[idx][1] - gc->dr_pos[1];
                delta[2] = gResPath.pts[idx][2] - gc->dr_pos[2];
            }
            BodyVector(delta, pos2, gc->dr_uvs);
            gc->strayAngle = fatan2(pos2[0], pos2[2]);
        }
    }
}
