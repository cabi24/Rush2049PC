#include "game/game.h"

typedef struct MODELDAT {
    u8 pad0[0x3F0];
    f32 magvel;             /* 0x3F0 */
    u8 pad3F4[0x6C4 - 0x3F4];
    s16 unk6C4;             /* 0x6C4 */
    u8 pad6C6[0x6CC - 0x6C6];
    s8 unk6CC;              /* 0x6CC */
    u8 pad6CD[0x714 - 0x6CD];
    f32 time;               /* 0x714 */
    u8 pad718[0x7C6 - 0x718];
    s16 slot;               /* 0x7C6 */
    s16 unk7C8;
    s16 we_control;         /* 0x7CA */
    u8 pad7CC[0x7D8 - 0x7CC];
    f32 stuck_time;         /* 0x7D8 */
    u8 pad7DC[0x7E0 - 0x7DC];
    s16 please_abort_me;    /* 0x7E0 */
    u8 pad7E2[0x7EC - 0x7E2];
    f32 drone_scale;        /* 0x7EC */
    u8 pad7F0[0x808 - 0x7F0];
} MODELDAT;

typedef struct {
    u8 pad0[8];
    f32 dr_pos[3];          /* 0x08 */
    u8 pad14[0x2C - 0x14];
    f32 dr_uvs[3][3];       /* 0x2C */
    u8 pad50[0xEF - 0x50];
    s8 place_locked;        /* 0xEF */
    u8 padF0[0xFA - 0xF0];
    s16 mpi;                /* 0xFA */
    s16 seg;                /* 0xFC */
    s16 padFE;
    f32 distance;           /* 0x100 */
    f32 last_distance;      /* 0x104 */
    u8 pad108[0x110 - 0x108]; struct pc_visual visuals[21]; u8 pad108_308[0x340 - 0x308];
    f32 fwd_progress_t;     /* 0x340 */
    u8 pad344[0x358 - 0x344];
    s8 we_died;             /* 0x358 */
    s8 unk359;              /* 0x359 */
    u8 pad35A[0x364 - 0x35A];
    f32 stray_angle;        /* 0x364 */
    u8 pad368[0x380 - 0x368]; void *input; u8 pad368_384[0x3B8 - 0x384];
} CAR_DATA;

typedef struct Player {
    u8 node;
    u8 pad1[7];
    s32 unk8;
    u8 padC[0x2C - 0xC];
    s32 unk2C;
    u8 pad30[0x48 - 0x30]; void *ref;
} Player;

typedef struct {
    u8 pad0[0xC];
    s16 (*pts)[3];
} PathBranch;

typedef struct {
    u8 pad0[4];
    s16 (*pts)[3];
    u8 pad8[4];
    PathBranch *branch;
} PathData;

extern Player gPlayers[];
VIEW(PathData, gResPath);
#define gResPath gResPath_view

void DetectDroneAbort(void) {
    s16 i;
    s16 node;
    s16 abort_me;
    s16 index1;
    s16 index2;
    f32 dist1;
    f32 dist2;
    f32 pos[3];
    f32 pos2[3];
    f32 delta[3];
    f32 angle;
    s32 wait_time;
    MODELDAT *m;
    CAR_DATA *gc;
    s32 idx;
    s32 seg;

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
        if (m->we_control || PC_LINK_OWNER(node) == 6) {
            abort_me = m->please_abort_me;
            if (gc->dr_pos[1] < -190.0f && gc->unk359 == 0 && gc->we_died == 0 && m->unk6C4 == -1) {
                m->unk6CC = 1;
                AbortCar(m->slot, 0);
                return;
            }
            if (gRace == 4 || gRace == 5 || gRace == 6) {
                continue;
            }
            if ((gGameState & 0x400008) && gRace != 1 && !gNoAutoAbort && !gEndGame && !gCoastFlag &&
                gc->place_locked != 1 && AreWeSolid(m->slot) && m->drone_scale > 0.1f &&
                (m->magvel < 25.0f || abort_me)) {
                if (!abort_me) {
                    if (m->stuck_time == 0.0f) {
                        m->stuck_time = m->time;
                    } else {
                        wait_time = 1;
                        if (gPlayers[node].unk8 & gPlayers[node].unk2C) {
                            wait_time = 2;
                        }
                        if (wait_time != 4) {
                            if (m->time - m->stuck_time > 6.0f * wait_time) {
                                abort_me = 1;
                            }
                        } else {
                            m->stuck_time = m->time;
                        }
                    }
                }
                if (abort_me) {
                    SetResurrectionData(2, m, 1);
                    gc->we_died = 2;
                    gc->unk359 = 0xFF;
                    m->stuck_time = 0.0f;
                    m->unk6CC = 1;
                }
            } else {
                m->stuck_time = 0.0f;
            }
        }
    }

    for (node = 0; node < gNumPlayers; node++) {
        m = PC_MODEL(gPlayers[node].node);
        gc = PC_CAR(gPlayers[node].node);
        if (gc->last_distance < gc->distance) {
            gc->fwd_progress_t = 0.0f;
            gc->last_distance = gc->distance;
            gc->stray_angle = 0.0f;
        } else if (!AreWeSolid(m->slot)) {
            gc->fwd_progress_t = 0.0f;
            gc->stray_angle = 0.0f;
        } else if (gc->fwd_progress_t == 0.0f) {
            gc->fwd_progress_t = m->time;
        } else if (m->time - gc->fwd_progress_t > 2.0f) {
            idx = gc->mpi;
            seg = gc->seg;
            for (i = 0; i < 5; i++) {
                NextResPathPt(idx, seg, &idx, &seg);
            }
            if (idx >= 0) {
                delta[0] = gResPath.branch[idx].pts[seg][0] - gc->dr_pos[0];
                delta[1] = gResPath.branch[idx].pts[seg][1] - gc->dr_pos[1];
                delta[2] = gResPath.branch[idx].pts[seg][2] - gc->dr_pos[2];
            } else {
                delta[0] = gResPath.pts[seg][0] - gc->dr_pos[0];
                delta[1] = gResPath.pts[seg][1] - gc->dr_pos[1];
                delta[2] = gResPath.pts[seg][2] - gc->dr_pos[2];
            }
            BodyVector(delta, pos2, gc->dr_uvs);
            gc->stray_angle = fatan2(pos2[0], pos2[2]);
        }
    }
}
