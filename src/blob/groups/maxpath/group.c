#include "game/game.h"

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
typedef struct Nav {
    f32 a;                  /* 0x00 */
    f32 b;                  /* 0x04 */
    f32 c;                  /* 0x08 */
    f32 d;                  /* 0x0C */
    f32 spd;                /* 0x10 */
    f32 tgt[3];             /* 0x14 */
    f32 tm;                 /* 0x20 */
    s16 pt;                 /* 0x24 */
    s16 flag;               /* 0x26 */
    s16 sel;                /* 0x28 */
    s16 tk;                 /* 0x2A */
} Nav;
typedef struct {
    /* 0x00 */ u8 pad0[2];
    /* 0x02 */ s16 last;       /* valid in element 0 */
    /* 0x04 */ u8 pad4[4];
    /* 0x08 */ s16 count;      /* valid in element 0 */
    /* 0x0A */ u8 padA[0x30 - 0xA];
    /* 0x30 */ s16 start[16];
} Section;                     /* 0x50 */
typedef struct { /* 0x0 */ s16 x, y, z; /* 0x6 */ u8 flag; /* 0x7 */ u8 pad7; } TrackPt;
typedef struct { /* 0x0 */ u16 numPoints; /* 0x2 */ u16 pad2; /* 0x4 */ TrackPt *points; } Track;

VIEW(Section[], gCheckpoints);
#define gCheckpoints gCheckpoints_view
VIEW(Track[], gDronePaths);
#define gDronePaths gDronePaths_view

extern f32 sqrtf(f32), fabsf(f32);
#pragma intrinsic(sqrtf, fabsf)

s16 FindClosestDronePathPtEfficiently(s16 *pos, s16 prev, s16 ptIdx, s16 row, s16 cur) {
    Section *ent;
    s16 d;
    s16 diff;
    s16 win;
    s16 best;
    s32 idx;
    s32 k;
    f32 bestd;
    f32 dx, dy, dz, dist;
    TrackPt *pt;

    ent = &gCheckpoints[row];
    d = ent->start[cur] - ent->start[prev];
    if (row + 1 == gCheckpoints[0].count) {
        diff = (gDronePaths[cur].numPoints - gDronePaths[prev].numPoints) - d;
    } else {
        diff = (ent[1].start[cur] - ent[1].start[prev]) - d;
    }
    if (diff < 0) {
        diff = -diff;
    }
    bestd = 1e20f;
    best = 0;
    if (diff < 5) {
        win = 5;
    } else {
        win = diff;
    }
    idx = ptIdx + d - win;
    while (idx < 0) {
        idx += gDronePaths[cur].numPoints;
    }
    while (idx >= gDronePaths[cur].numPoints) {
        idx -= gDronePaths[cur].numPoints;
    }
    win = win * 2 + 1;
    k = 0; if (win > 0) for (;;) {
        pt = &gDronePaths[cur].points[idx];
        dx = (f32) (pt->x - pos[0]);
        dy = (f32) (pt->y - pos[1]);
        dz = (f32) (pt->z - pos[2]);
        dist = dx * dx + dy * dy + dz * dz;
        if (dist < bestd) {
            bestd = dist;
            best = idx;
        }
        if (idx == gDronePaths[cur].numPoints - 1) {
            idx = gCheckpoints[gCheckpoints[0].last].start[cur];
        } else {
            idx++;
        }
        if (++k == win) break;
    }
    return best;
}

/* arcade mp_interval_pos; inlined, the retail 2-word stub remains */
void IntervalPos(ModelRecord *m, Nav *cp) {
    TrackPt *mp;
    TrackPt *nmp;
    s16 nmpi;
    f32 x;
    f32 y;
    f32 invdist;
    f32 n[2];

    mp = &gDronePaths[cp->sel].points[cp->pt];
    nmpi = NextPathPt(cp->pt, cp->sel);
    nmp = &gDronePaths[cp->sel].points[nmpi];
    n[0] = nmp->x - mp->x;
    n[1] = nmp->z - mp->z;
    cp->d = TB_Vector2Normalize(n);
    invdist = (cp->d > 0.01f) ? 1.0f / cp->d : 0.0f;
    x = m->x22C - mp->x;
    y = m->z234 - mp->z;
    cp->a = (y * n[1]) + (n[0] * x);
    cp->b = (x * n[1]) - (n[0] * y);
    nmpi = NextPathPt(nmpi, cp->sel);
    nmp = &gDronePaths[cp->sel].points[nmpi];
    cp->c = ((nmp->x - mp->x) * n[1]) - (n[0] * (nmp->z - mp->z));
}

void IntervalPosChange(ModelRecord *m, Nav *cp, s32 change_flag) {
    s16 j, high_nextmp, nnmpi, index, node, high_index;
    s32 nmpi;
    f32 x, y, z, nx, ny, score, high_score, delta_spd, mspd;
    f32 xpos, ypos, x1, y1, x2, y2, dx, dy, a1, b1, c1, dist;
    f32 speed, xy_speed, vec1, vec0, dp;
    f32 cur_time;
    s32 i;
    s16 pp[3];

    node = m->unk7C6;
    if (cp->flag >= 0) {
        cp->tm = gModelTime;
        cp->flag = -1;
    }
    cur_time = gModelTime;
    if (change_flag && ((cur_time - gLastPathSwitchTime) > 0.5f) &&
        ((current_drone == -1) || (PC_DRONE(current_drone) == node))) {
        for (j = 0; j < num_drones; j++) {
            if (++current_drone >= num_drones) {
                current_drone = 0;
            }
            if (PC_MODEL_WE_CONTROL(PC_DRONE(current_drone))) {
                break;
            }
        }
        gLastPathSwitchTime = cur_time;

        xpos = m->pos[0];
        ypos = m->pos[2];

        high_score = 0.0f;
        high_index = -1;
        high_nextmp = 0;

        x = m->vx;
        y = m->vz;
        z = m->vy;
        xy_speed = (x * x) + (y * y);
        speed = (z * z) + xy_speed;

        if (xy_speed < 2500.0f) {
            xy_speed = 0.0f;
            vec0 = 0.0f;
            vec1 = 0.0f;
        } else {
            xy_speed = 1.0f / sqrtf(xy_speed);
            vec1 = y * xy_speed;
            vec0 = x * xy_speed;
        }

        for (i = 0; i < 4; i++) {
            score = 0.0f;
            index = i;

            pp[0] = m->pos[0];
            pp[1] = m->pos[1];
            pp[2] = m->pos[2];
            nmpi = FindClosestDronePathPtEfficiently(pp, cp->sel, cp->pt, m->unk7E2, index);

            y1 = gDronePaths[i].points[nmpi].z;
            x1 = gDronePaths[i].points[nmpi].x;

            nnmpi = NextPathPt(nmpi, index);

            y2 = gDronePaths[i].points[nnmpi].z;
            x2 = gDronePaths[i].points[nnmpi].x;

            dy = y1 - ypos;
            dx = x1 - xpos;

            if (((dy * dy) + (dx * dx)) < 400.0f) {
                dx = x1 - x2;
                dy = y1 - y2;

                a1 = dx;
                b1 = -dy;
                c1 = (x1 * dy) - (y1 * dx);

                dist = sqrtf((a1 * a1) + (b1 * b1));

                if (dist > 0.01f) {
                    dist = fabsf((a1 * ypos) + (b1 * xpos) + c1) / dist;

                    if (dist < 10.0f) {
                        score = 1.0f - (dist * 0.1f);
                    }
                }
            }

            mspd = gDronePaths[i].points[nmpi].flag * 1.4666667f;
            delta_spd = fabsf(speed - (mspd * mspd));

            if (delta_spd < (50.0f * (5280.0f / 3600.0f))) {
                score += (1 - (delta_spd / (50.0f * (5280.0f / 3600.0f)))) * 0.5f;
            }

            if (i == cp->tk) {
                score += 1.0f;
            }

            if (xy_speed == 0) {
                score += 1.0f;
            } else {
                dy = y2 - y1;
                dx = x2 - x1;

                dist = 1.0f / sqrtf((dy * dy) + (dx * dx));
                dy *= dist;
                dx *= dist;

                dp = ((vec1 * dy) + (vec0 * dx) + 1) * 0.5f;

                score += dp;
            }

            if (score > high_score) {
                high_score = score;
                high_nextmp = nmpi;
                high_index = i;
            }
        }

        if ((high_index != -1) && (high_index != cp->sel)) {
            cp->pt = high_nextmp;
            cp->tm = cur_time;
            cp->flag = -1;
            cp->sel = high_index;
        }
    }

    IntervalPos(m, cp);
}

#define veccopy(a, b) { (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; }
#define vecsub(a, b, c) { (c)[0] = (a)[0] - (b)[0]; (c)[1] = (a)[1] - (b)[1]; (c)[2] = (a)[2] - (b)[2]; }
#define scalmul(a, s, b) { (b)[0] = (a)[0] * (s); (b)[1] = (a)[1] * (s); (b)[2] = (a)[2] * (s); }

/* arcade avoid_areas */
void AvoidAreas(s16 drone_index) {
    s16 i, index, hint, can_we_cheat;
    f32 dist, scale, scale1, scale2, tmp2;
    f32 rpos[3];
    f32 pos[3], tpos[3], own_rwr[3], dir[3], temp[3];
    f32 dir_list[6][3], dir_weight[6];
    f32 cur_rate[3], save_dir[3];
    Nav *cp;
    ModelRecord *m;

    m = PC_MODEL(drone_index);
    cp = &PC_CAR_NAV(drone_index);

    veccopy(cp->tgt, tpos);

    vecsub(tpos, m->pos, pos);
    BodyVector(pos, rpos, (f32 *) ((u8 *) m + 0x7A0));

    if ((m->f638 ==  0.f ) || (m->speed < .1f)) {
        PC_MODEL_CAN_CHEAT(drone_index) = 0;
        rpos[0] =  0.0f ;
        cp->spd = 100.0f;
        veccopy(rpos, cp->tgt);
        for (i = 0; i < 3; i++) {
            PC_MP_DIR(drone_index)[i] =  0.0f ;
        }
        return;
    }

    if (m->speed < (f32) (u32) gDronePaths[cp->sel].points[cp->pt].flag * 1.4666667f * .5f) {
        rpos[0] *= .2f;
        cp->spd = (f32) (u32) gDronePaths[cp->sel].points[cp->pt].flag * 1.4666667f * 1.5f;
        veccopy(rpos, cp->tgt);
        for (i = 0; i < 3; i++) {
            PC_MP_DIR(drone_index)[i] = 0;
        }
        PC_MODEL_CAN_CHEAT(drone_index) = 0;
        return;
    }

    if (rpos[2] < fabsf(rpos[0] + rpos[0])) {
        veccopy(rpos, cp->tgt);
        for (i = 0; i < 3; i++) {
            PC_MP_DIR(drone_index)[i] = 0;
        }
        PC_MODEL_CAN_CHEAT(drone_index) = 0;
        return;
    }

    dist = vector_magnitude(rpos);
    scale = m->speed * m->f634 / dist;
    scalmul(rpos, scale, dir);
    veccopy(dir, save_dir);

    veccopy(PC_MODEL_RECKON_RWR(drone_index), own_rwr);

    can_we_cheat = 1;
    scale1 =  1 ;

    for (i = 0; i < gNumActiveCars; i++) {
        index = PC_MODEL_SLOT(i);
        if (index == drone_index) {
            continue;
        }
        m = PC_MODEL(index);

        vecsub(m->pos, own_rwr, pos);
        BodyVector(pos, rpos, (f32 *) ((u8 *) PC_MODEL(drone_index) + 0x7A0));

        dist = rpos[0] * rpos[0] + rpos[2] * rpos[2];
        if (dist > 90000.0f) {
            continue;
        }
        if (m->b7CC == 2) {
            if (fabsf(rpos[0]) < 60 && fabsf(rpos[2]) < 200) {
                can_we_cheat = 0;
            }
        }
        if (rpos[2] <  0.f ) {
            continue;
        }
        if (fabsf(rpos[0]) > 8) {
            continue;
        }
        scale2 = 1;
        if (rpos[2] < 20) {
            scale2 = 0.9f;
        } else if (rpos[2] < 100) {
            scale2 = 1.0f - (100 - rpos[2]) * 0.00125000032f;
        }
        if (scale2 < scale1) {
            scale1 = scale2;
        }
    }

    TargetPathAvoid(PC_MODEL(drone_index), &scale1);

    if (fabsf(dir[0]) * 4 < fabsf(dir[2])) {
        if (dir[0] <  0.0f ) {
            dir[0] += (1.0f - scale1) * 8;
        } else {
            dir[0] -= (1.0f - scale1) * 8;
        }
    } else if (scale1 < 0.9f) {
        if (dir[0] <  0.0f ) {
            dir[0] = (1.0f - scale1) *  4.0f ;
        } else {
            dir[0] = -(1.0f - scale1) *  4.0f ;
        }
    }

    hint = ((s8 *) &gDronePaths[cp->sel].points[cp->pt])[7];

    if (PC_MP_DIR(drone_index)[0] == 0 && PC_MP_DIR(drone_index)[2] == 0) {
        dir[0] = 0;
        cp->spd *= 1.0f - (1.0f - scale1) * .3f;
    } else if (hint == 5 || m->s6C4 >= 0) {
        dir[0] *= .4f;
    } else if (hint == 2 || (PC_MP_DIR(drone_index)[0] == 0 && PC_MP_DIR(drone_index)[2] == 0)) {
        veccopy(save_dir, dir);
        cp->spd *= 1.0f - (1.0f - scale1) * .3f;
    } else if (hint == 1) {
        if (PC_MP_DIR(drone_index)[0] == 0 && PC_MP_DIR(drone_index)[2] == 0) {
            veccopy(save_dir, dir);
        } else {
            dir[0] = save_dir[0] * .15f + PC_MP_DIR(drone_index)[0] * .85f;
        }
        cp->spd *= 1.0f - (1.0f - scale1) * .1f;
    } else if (hint == 3) {
        dir[0] = save_dir[0] - PC_MP_SAVE_DIR(drone_index)[0];
        veccopy(PC_MP_SAVE_DIR(drone_index), save_dir);
    } else if (hint == 4) {
        dir[0] = save_dir[0] - PC_MP_SAVE_DIR(drone_index)[0];
    } else {
        dir[0] = dir[0] * .5f + PC_MP_DIR(drone_index)[0] * .5f;
    }

    PC_MODEL_CAN_CHEAT(drone_index) = can_we_cheat;

    veccopy(dir, cp->tgt);

    veccopy(dir, PC_MP_DIR(drone_index));

    veccopy(save_dir, PC_MP_SAVE_DIR(drone_index));
}

#define range_it(v,l,h)		( ((v) < (l)) ? (l) : (((v) > (h)) ? (h) : (v)) )

/* arcade MaxPathControls; defined before its helpers as in maxpath.c */
void MaxPathControls(ModelRecord *m)
{
    f32 tspd;
    Nav *cp;

    cp = &PC_CAR_NAV(m->unk7C6);
    FindInterval(m, cp);
    TargetSpeed(m, cp);
    TargetSteerPos(m, cp);

    AvoidAreas(m->unk7C6);

    tspd = cp->spd;
    if (m->vy <  0.0f ) {
        tspd *= gravitySpeedIncrease[gGravityScale];
    } else if (tspd < gravitySpeedMin[gGravityScale]) {
        tspd = gravitySpeedMin[gGravityScale];
    }
    tspd *= m->f7EC;

    AdjustSpeed(m, tspd);
    AdjustSteer(m, cp->tgt);

    m->s7E0 = (gModelTime - cp->tm > 1.0f) && (cp->pt > 10);

    if (fabsf(cp->a) > 100.0f || fabsf(cp->b) > 100.0f) {
        m->s7E0 = 1;
    }
}

/* arcade MP_FindInterval; inlined, the retail 2-word stub remains */
void FindInterval(ModelRecord *m, Nav *cp)
{
    IntervalPosChange(m, cp, 1);
    if (cp->a >= cp->d) {
        cp->pt = NextPathPt(cp->pt, cp->sel);
        cp->tm = gModelTime;
        IntervalPosChange(m, cp, 0);
    } else if (cp->a < 0.0f) {
        cp->pt = PrevPathPt(cp->pt, cp->sel);
        IntervalPosChange(m, cp, 0);
    }
}

/* arcade unload_extra_overlay; inlined, the retail 2-word stub remains */
void TargetSpeed(ModelRecord *m, Nav *cp)
{
    TrackPt *mp, *nmp;
    s32 nmpi;
    f32 fraction, yrel, offsc;

    nmpi = NextPathPt(cp->pt, cp->sel);

    fraction = cp->a / cp->d;
    fraction = range_it(fraction, 0.0f, 1.0f);
    cp->spd = (f32)(u32)gDronePaths[cp->sel].points[cp->pt].flag * 1.4666667f + ((f32)(u32)gDronePaths[cp->sel].points[nmpi].flag * 1.4666667f - (f32)(u32)gDronePaths[cp->sel].points[cp->pt].flag * 1.4666667f) * fraction;

    yrel = (cp->b > 0.0f) ? cp->b : -cp->b;
    if (yrel < 0.1f) {
        offsc = 1.0f;
    } else if (yrel > 80.0f) {
        offsc = 0.2f;
    } else {
        offsc = 1.0f - (yrel * yrel * 0.00003125f);
    }
    cp->spd *= offsc;
    if (cp->spd < 28.0f) {
        cp->spd = 28;
    }
}

/* arcade AudioPlayEffectInternal; inlined, the retail 2-word stub remains */
void TargetSteerPos(ModelRecord *m, Nav *cp)
{
    TrackPt *mp, *nmp;
    s16 nmpi;
    f32 x, y, z, dist, tgtdist, fraction;

    mp = &gDronePaths[cp->sel].points[cp->pt];
    nmpi = NextPathPt(cp->pt, cp->sel);
    nmp = &gDronePaths[cp->sel].points[nmpi];

    tgtdist = 80.0f + cp->a;
    dist = cp->d;
    tgtdist -= dist;
    while (tgtdist > 0.0f) {
        mp = nmp;
        nmpi = NextPathPt(nmpi, cp->sel);
        nmp = &gDronePaths[cp->sel].points[nmpi];

        x = nmp->x - mp->x;
        y = nmp->y - mp->y;
        z = nmp->z - mp->z;
        dist = sqrtf(x * x + y * y + z * z);
        tgtdist -= dist;
    }

    if (dist < 1e-5f) {
        TB_SetTextColor(5);
        TB_DrawText(80, 160, (void *)&target_steer_zero_dist_message);
        fraction = 1;
    } else {
        fraction = (dist + tgtdist) / dist;
    }
    fraction = range_it(fraction, 0.0f, 1.0f);

    cp->tgt[0] = (nmp->x - mp->x) * fraction + mp->x;
    cp->tgt[1] = (nmp->y - mp->y) * fraction + mp->y;
    cp->tgt[2] = (nmp->z - mp->z) * fraction + mp->z;
}

/* arcade AdjustSpeed; inlined, the retail 2-word stub remains */
void AdjustSpeed(ModelRecord *m, f32 tspd)
{
    s32 i, slip, spddir, node, cur_time;
    f32 x, y, z, dspd, delta_speed;

    node = m->unk7C6;

    tspd *= 1.05f;

    if ((gGravityScale < 2) && ((m->b488 == 40) || (m->b4E4 == 40))) {
        PC_MP_BRAKE(node) = 0;
        PC_MP_GAS(node) = 0;
    } else {
        x = m->vx;
        y = m->vy;
        z = m->vz;
        dspd = sqrtf(x * x + y * y + z * z);

        delta_speed = tspd - dspd;

        if (delta_speed < 0) {
            delta_speed *= 0.5f;
        }

        PC_MP_GAS(node) += (delta_speed * 0.183f);

        if (PC_MP_GAS(node) < -0.75f) {
            PC_MP_BRAKE(node) -= (PC_MP_GAS(node) + 0.75f) / 4;
        } else {
            PC_MP_BRAKE(node) = 0;
        }
    }

    if (PC_MP_GAS(node) < 0) {
        PC_MP_GAS(node) = 0;
    } else if (PC_MP_GAS(node) > 1.0f) {
        PC_MP_GAS(node) = 1.0f;
    }

    if (PC_MP_BRAKE(node) < 0) {
        PC_MP_BRAKE(node) = 0;
    } else if (PC_MP_BRAKE(node) > 1.0f) {
        PC_MP_BRAKE(node) = 1.0f;
    }

    m->f72C = PC_MP_GAS(node);
    m->f728 = PC_MP_BRAKE(node);
}

/* arcade PS_PurgeFile; inlined, the retail 2-word stub remains */
void AdjustSteer(ModelRecord *m, f32 *pos)
{
    s32 node;
    f32 angle;

    node = m->unk7C6;

    pos[0] *= m->f634 * 108.43373f;

    angle = fatan2(pos[2], pos[0]);

    angle = range_it(angle, 0, 3.1415927f);

    PC_MP_STEER(node) = 1.0f - angle * 0.63661975f;
    m->f720 = PC_MP_STEER(node);
}

