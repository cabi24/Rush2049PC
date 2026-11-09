/* maxpath: the drones' driving along the drone paths (MAT3 maxpath.c): which stretch of which path a drone is on,
 * the speed and the point it aims for, steering round the other cars, and the steer, gas and brake that come of it.
 * MaxPathControls runs for each drone on the model thread every step. The drone's state is its car record's
 * maxpath control (MAT3 mpctl, the PC's pc_car.nav). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the fields maxpath reads and sets. */
typedef struct tModelData {
    u8 pad0[0x220];
    f32 rwv[3];             /* 0x220 the velocity, feet a second */
    f32 rwr[3];             /* 0x22C the position */
    u8 pad238[0x1B8];
    f32 magvel;             /* 0x3F0 the speed */
    u8 pad3F4[0x94];
    s8 slipflag0;           /* 0x488 tires[0].slipflag: the front tyres' surface flags (40 below: no gas, no brake) */
    u8 pad489[0x5B];
    s8 slipflag1;           /* 0x4E4 tires[1].slipflag */
    u8 pad4E5[0x14F];
    f32 dt;                 /* 0x634 the model step, seconds */
    f32 idt;                /* 0x638 its inverse, 0 while paused */
    u8 pad63C[0x4];
    s8 crashflag;           /* 0x640 */
    u8 pad641[0x83];
    s16 moving_state;       /* 0x6C4 resurrect.moving_state: -1 unless being put back on the road */
    u8 pad6C6[0x6];
    s8 autoAbort;           /* 0x6CC resurrect.autoAbort */
    u8 pad6CD[0x43];
    s32 modelIteration;     /* 0x710 fastin.modelIteration */
    f32 modelTime;          /* 0x714 fastin.modelTime */
    f32 fastDt;             /* 0x718 fastin.dt */
    u8 pad71C[0x4];
    f32 wheel;              /* 0x720 mainin.wheel: the steer */
    u8 pad724[0x4];
    f32 brake;              /* 0x728 mainin.brake */
    f32 throttle;           /* 0x72C mainin.throttle */
    u8 pad730[0x2];
    s8 abort;               /* 0x732 mainin.abort */
    u8 pad733[0x55];
    f32 reckon_rwv[3];      /* 0x788 reckon.rwv: the dead-reckoned velocity */
    f32 reckon_rwr[3];      /* 0x794 reckon.rwr: the dead-reckoned position */
    f32 reckon_uvs[3][3];   /* 0x7A0 reckon.uvs: the dead-reckoned orientation */
    u8 pad7C4[0x2];
    s16 slot;               /* 0x7C6 the car slot */
    u8 pad7C8[0x2];
    s16 we_control;         /* 0x7CA this machine drives it */
    s8 drone_type;          /* 0x7CC 1 a drone, 2 a human */
    u8 pad7CD[0x11];
    s8 can_we_cheat;        /* 0x7DE */
    u8 pad7DF[0x1];
    s16 please_abort_me;    /* 0x7E0 the drone is stuck: put it back on the road (DetectDroneAbort) */
    s16 last_cp_index;      /* 0x7E2 the last checkpoint it passed */
    u8 pad7E4[0x8];
    f32 drone_scale;        /* 0x7EC its speed scale (the catch-up) */
    u8 pad7F0[0x18];
} tModelData;

extern f32 sqrtf(f32), fabsf(f32);

/* Returns the point of drone path `path` nearest `pos`, searching only round where a car at point `lastIdx` of
 * drone path `lastPath` would be: the same distance past checkpoint `lastCP` (its last one) on the other path, give
 * or take the difference in the two paths' lengths over that checkpoint's stretch (at least 5 points). */
s16 FindClosestDronePathPtEfficiently(s16 *pos, s16 lastPath, s16 lastIdx, s16 lastCP, s16 path) {
    tCheckpoint *cp;
    s16 offset;
    s16 delta;
    s16 window;
    s16 closest;
    s32 idx;
    s32 i;
    f32 min;
    f32 dx, dy, dz, dist;
    DronePathPt *pt;

    cp = &gCheckpoints.points[lastCP];
    offset = cp->dronePathPt[path] - cp->dronePathPt[lastPath];
    if (lastCP + 1 == gCheckpoints.numPoints) {
        delta = (gDronePaths[path].numPts - gDronePaths[lastPath].numPts) - offset;
    } else {
        delta = (cp[1].dronePathPt[path] - cp[1].dronePathPt[lastPath]) - offset;
    }
    if (delta < 0) {
        delta = -delta;
    }
    min = 1e20f;
    closest = 0;
    if (delta < 5) {
        window = 5;
    } else {
        window = delta;
    }
    idx = lastIdx + offset - window;
    while (idx < 0) {
        idx += gDronePaths[path].numPts;
    }
    while (idx >= gDronePaths[path].numPts) {
        idx -= gDronePaths[path].numPts;
    }
    window = window * 2 + 1;
    for (i = 0; i < window; i++) {
        pt = &gDronePaths[path].pts[idx];
        dx = (f32) (pt->pos[0] - pos[0]);
        dy = (f32) (pt->pos[1] - pos[1]);
        dz = (f32) (pt->pos[2] - pos[2]);
        dist = dx * dx + dy * dy + dz * dz;
        if (dist < min) {
            min = dist;
            closest = idx;
        }
        if (idx == gDronePaths[path].numPts - 1) {
            idx = gCheckpoints.points[gCheckpoints.loopPt].dronePathPt[path];
        } else {
            idx++;
        }
    }
    return closest;
}

/* Measures where model `m` is against the stretch of drone path it is on (from point cp->idx to the next):
 * cp->len the stretch's length, cp->xrel how far along it the car is and cp->yrel how far to the side (both in x
 * and z), and cp->cyrel how far to the side of the stretch's line the point after next lies (the bend ahead). */
void IntervalPos(tModelData *m, struct pc_car_nav *cp) {
    DronePathPt *mp;
    DronePathPt *nmp;
    s16 nmpi;
    f32 x;
    f32 y;
    f32 invdist;
    f32 n[2];

    mp = &gDronePaths[cp->path].pts[cp->idx];
    nmpi = NextPathPt(cp->idx, cp->path);
    nmp = &gDronePaths[cp->path].pts[nmpi];
    n[0] = nmp->pos[0] - mp->pos[0];
    n[1] = nmp->pos[2] - mp->pos[2];
    cp->len = TB_Vector2Normalize(n);
    invdist = (cp->len > 0.01f) ? 1.0f / cp->len : 0.0f;
    x = m->rwr[0] - mp->pos[0];
    y = m->rwr[2] - mp->pos[2];
    cp->xrel = (y * n[1]) + (n[0] * x);
    cp->yrel = (x * n[1]) - (n[0] * y);
    nmpi = NextPathPt(nmpi, cp->path);
    nmp = &gDronePaths[cp->path].pts[nmpi];
    cp->cyrel = ((nmp->pos[0] - mp->pos[0]) * n[1]) - (n[0] * (nmp->pos[2] - mp->pos[2]));
}

/* IntervalPos, first taking a pending jump's time (cp->newIdx) and, with `change_flag`, maybe moving the drone to
 * another drone path. One drone at a time looks (current_drone goes round the drones this machine drives, at most
 * every half second): each of the four paths' nearest points (FindClosestDronePathPtEfficiently) is scored for
 * being within 20 feet and on the path's line, for its speed being near the car's, for being the drone's own path
 * (basePath), and for heading the way the car goes; the drone takes the best one's path. */
void IntervalPosChange(tModelData *m, struct pc_car_nav *cp, s32 change_flag) {
    s16 j, high_nextmp, nnmpi, index, node, high_index;
    s32 nmpi;
    f32 x, y, z, nx, ny, score, high_score, delta_spd, mspd;
    f32 xpos, ypos, x1, y1, x2, y2, dx, dy, a1, b1, c1, dist;
    f32 speed, xy_speed, vec1, vec0, dp;
    f32 cur_time;
    s32 i;
    s16 pp[3];

    node = m->slot;
    if (cp->newIdx >= 0) {
        cp->interval_time = gModelTime;
        cp->newIdx = -1;
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

        xpos = m->reckon_rwr[0];
        ypos = m->reckon_rwr[2];

        high_score = 0.0f;
        high_index = -1;
        high_nextmp = 0;

        x = m->rwv[0];
        y = m->rwv[2];
        z = m->rwv[1];
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

            pp[0] = m->reckon_rwr[0];
            pp[1] = m->reckon_rwr[1];
            pp[2] = m->reckon_rwr[2];
            nmpi = FindClosestDronePathPtEfficiently(pp, cp->path, cp->idx, m->last_cp_index, index);

            y1 = gDronePaths[i].pts[nmpi].pos[2];
            x1 = gDronePaths[i].pts[nmpi].pos[0];

            nnmpi = NextPathPt(nmpi, index);

            y2 = gDronePaths[i].pts[nnmpi].pos[2];
            x2 = gDronePaths[i].pts[nnmpi].pos[0];

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

            /* Original game: `speed` is the square of the car's speed, so the difference of squares is measured
             * against a speed (50 mph in feet a second); the term only scores speeds within a few mph of the
             * path's. MAT3 has it so too. */
            mspd = gDronePaths[i].pts[nmpi].mph * 1.4666667f;
            delta_spd = fabsf(speed - (mspd * mspd));

            if (delta_spd < (50.0f * (5280.0f / 3600.0f))) {
                score += (1 - (delta_spd / (50.0f * (5280.0f / 3600.0f)))) * 0.5f;
            }

            if (i == cp->basePath) {
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

        if ((high_index != -1) && (high_index != cp->path)) {
            cp->idx = high_nextmp;
            cp->interval_time = cur_time;
            cp->newIdx = -1;
            cp->path = high_index;
        }
    }

    IntervalPos(m, cp);
}

#define veccopy(a, b) { (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; }
#define vecsub(a, b, c) { (c)[0] = (a)[0] - (b)[0]; (c)[1] = (a)[1] - (b)[1]; (c)[2] = (a)[2] - (b)[2]; }
#define scalmul(a, s, b) { (b)[0] = (a)[0] * (s); (b)[1] = (a)[1] * (s); (b)[2] = (a)[2] * (s); }

/* Turns the drone in slot `drone_index`'s target point (cp->tgtpos) into the direction to steer, in the car's own
 * frame (BodyVector), slowing (cp->tgtspd) and edging aside for cars close ahead and for moving targets
 * (TargetPathAvoid). A stopped or paused drone aims straight on at 100 feet a second; one going under half the
 * path's speed aims gently at 1.5 times it; a target behind or far to the side is aimed at as it is. The drone path
 * point's hint picks how the new direction blends with the last (pc_mp_dir) and the one before (pc_mp_save_dir).
 * can_we_cheat goes off when a human is near (within 60 feet across, 200 along). */
void AvoidAreas(s16 drone_index) {
    s16 i, index, hint, can_we_cheat;
    f32 dist, scale, scale1, scale2, tmp2;
    f32 rpos[3];
    f32 pos[3], tpos[3], own_rwr[3], dir[3], temp[3];
    f32 dir_list[6][3], dir_weight[6];
    f32 cur_rate[3], save_dir[3];
    struct pc_car_nav *cp;
    tModelData *m;

    m = PC_MODEL(drone_index);
    cp = &PC_CAR_NAV(drone_index);

    veccopy(cp->tgtpos, tpos);

    vecsub(tpos, m->reckon_rwr, pos);
    BodyVector(pos, rpos, (f32 *) m->reckon_uvs);

    if ((m->idt ==  0.f ) || (m->magvel < .1f)) {
        PC_MODEL_CAN_CHEAT(drone_index) = 0;
        rpos[0] =  0.0f ;
        cp->tgtspd = 100.0f;
        veccopy(rpos, cp->tgtpos);
        for (i = 0; i < 3; i++) {
            PC_MP_DIR(drone_index)[i] =  0.0f ;
        }
        return;
    }

    if (m->magvel < (f32) (u32) gDronePaths[cp->path].pts[cp->idx].mph * 1.4666667f * .5f) {
        rpos[0] *= .2f;
        cp->tgtspd = (f32) (u32) gDronePaths[cp->path].pts[cp->idx].mph * 1.4666667f * 1.5f;
        veccopy(rpos, cp->tgtpos);
        for (i = 0; i < 3; i++) {
            PC_MP_DIR(drone_index)[i] = 0;
        }
        PC_MODEL_CAN_CHEAT(drone_index) = 0;
        return;
    }

    if (rpos[2] < fabsf(rpos[0] + rpos[0])) {
        veccopy(rpos, cp->tgtpos);
        for (i = 0; i < 3; i++) {
            PC_MP_DIR(drone_index)[i] = 0;
        }
        PC_MODEL_CAN_CHEAT(drone_index) = 0;
        return;
    }

    dist = vector_magnitude(rpos);
    scale = m->magvel * m->dt / dist;
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

        vecsub(m->reckon_rwr, own_rwr, pos);
        BodyVector(pos, rpos, (f32 *) ((tModelData *) PC_MODEL(drone_index))->reckon_uvs);

        dist = rpos[0] * rpos[0] + rpos[2] * rpos[2];
        if (dist > 90000.0f) {
            continue;
        }
        if (m->drone_type == 2) {
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

    hint = gDronePaths[cp->path].pts[cp->idx].hints;

    if (PC_MP_DIR(drone_index)[0] == 0 && PC_MP_DIR(drone_index)[2] == 0) {
        dir[0] = 0;
        cp->tgtspd *= 1.0f - (1.0f - scale1) * .3f;
    } else if (hint == 5 || m->moving_state >= 0) {
        dir[0] *= .4f;
    } else if (hint == 2 || (PC_MP_DIR(drone_index)[0] == 0 && PC_MP_DIR(drone_index)[2] == 0)) {
        veccopy(save_dir, dir);
        cp->tgtspd *= 1.0f - (1.0f - scale1) * .3f;
    } else if (hint == 1) {
        if (PC_MP_DIR(drone_index)[0] == 0 && PC_MP_DIR(drone_index)[2] == 0) {
            veccopy(save_dir, dir);
        } else {
            dir[0] = save_dir[0] * .15f + PC_MP_DIR(drone_index)[0] * .85f;
        }
        cp->tgtspd *= 1.0f - (1.0f - scale1) * .1f;
    } else if (hint == 3) {
        dir[0] = save_dir[0] - PC_MP_SAVE_DIR(drone_index)[0];
        veccopy(PC_MP_SAVE_DIR(drone_index), save_dir);
    } else if (hint == 4) {
        dir[0] = save_dir[0] - PC_MP_SAVE_DIR(drone_index)[0];
    } else {
        dir[0] = dir[0] * .5f + PC_MP_DIR(drone_index)[0] * .5f;
    }

    PC_MODEL_CAN_CHEAT(drone_index) = can_we_cheat;

    veccopy(dir, cp->tgtpos);

    veccopy(dir, PC_MP_DIR(drone_index));

    veccopy(save_dir, PC_MP_SAVE_DIR(drone_index));
}

#define range_it(v,l,h)		( ((v) < (l)) ? (l) : (((v) > (h)) ? (h) : (v)) )

/* One step of a drone's driving, on the model thread: finds its stretch of drone path (FindInterval), its target
 * speed (TargetSpeed) and point (TargetSteerPos), steers round the others (AvoidAreas), and sets its gas, brake and
 * steer (AdjustSpeed, AdjustSteer). The target speed goes up downhill and has a floor uphill by the gravity setting,
 * and is scaled by the drone's catch-up scale. A drone more than a second on one stretch (past point 10) or more
 * than 100 feet off its path asks to be put back on the road (please_abort_me). */
void MaxPathControls(tModelData *m)
{
    f32 tspd;
    struct pc_car_nav *cp;

    cp = &PC_CAR_NAV(m->slot);
    FindInterval(m, cp);
    TargetSpeed(m, cp);
    TargetSteerPos(m, cp);

    AvoidAreas(m->slot);

    tspd = cp->tgtspd;
    if (m->rwv[1] <  0.0f ) {
        tspd *= gravitySpeedIncrease[gGravityScale];
    } else if (tspd < gravitySpeedMin[gGravityScale]) {
        tspd = gravitySpeedMin[gGravityScale];
    }
    tspd *= m->drone_scale;

    AdjustSpeed(m, tspd);
    AdjustSteer(m, cp->tgtpos);

    m->please_abort_me = (gModelTime - cp->interval_time > 1.0f) && (cp->idx > 10);

    if (fabsf(cp->xrel) > 100.0f || fabsf(cp->yrel) > 100.0f) {
        m->please_abort_me = 1;
    }
}

/* Finds the stretch of drone path model `m` is on: IntervalPosChange, then a step on to the next stretch once the
 * car is past this one's end (its time taken), or back to the previous one if it is behind its start. */
void FindInterval(tModelData *m, struct pc_car_nav *cp)
{
    IntervalPosChange(m, cp, 1);
    if (cp->xrel >= cp->len) {
        cp->idx = NextPathPt(cp->idx, cp->path);
        cp->interval_time = gModelTime;
        IntervalPosChange(m, cp, 0);
    } else if (cp->xrel < 0.0f) {
        cp->idx = PrevPathPt(cp->idx, cp->path);
        IntervalPosChange(m, cp, 0);
    }
}

/* Sets the target speed (cp->tgtspd, feet a second): the path's speeds at the stretch's two points, interpolated by
 * how far along it the car is, cut the further the car is to the side (to a fifth at 80 feet), and at least 28. */
void TargetSpeed(tModelData *m, struct pc_car_nav *cp)
{
    DronePathPt *mp, *nmp;
    s32 nmpi;
    f32 fraction, yrel, offsc;

    nmpi = NextPathPt(cp->idx, cp->path);

    fraction = cp->xrel / cp->len;
    fraction = range_it(fraction, 0.0f, 1.0f);
    cp->tgtspd = (f32)(u32)gDronePaths[cp->path].pts[cp->idx].mph * 1.4666667f + ((f32)(u32)gDronePaths[cp->path].pts[nmpi].mph * 1.4666667f - (f32)(u32)gDronePaths[cp->path].pts[cp->idx].mph * 1.4666667f) * fraction;

    yrel = (cp->yrel > 0.0f) ? cp->yrel : -cp->yrel;
    if (yrel < 0.1f) {
        offsc = 1.0f;
    } else if (yrel > 80.0f) {
        offsc = 0.2f;
    } else {
        offsc = 1.0f - (yrel * yrel * 0.00003125f);
    }
    cp->tgtspd *= offsc;
    if (cp->tgtspd < 28.0f) {
        cp->tgtspd = 28;
    }
}

/* Sets the target point (cp->tgtpos): 80 feet ahead of the car along the drone path. A stretch of zero length
 * shows a message on the screen (target_steer_zero_dist_message), a leftover of the arcade's debugging. */
void TargetSteerPos(tModelData *m, struct pc_car_nav *cp)
{
    DronePathPt *mp, *nmp;
    s16 nmpi;
    f32 x, y, z, dist, tgtdist, fraction;

    mp = &gDronePaths[cp->path].pts[cp->idx];
    nmpi = NextPathPt(cp->idx, cp->path);
    nmp = &gDronePaths[cp->path].pts[nmpi];

    tgtdist = 80.0f + cp->xrel;
    dist = cp->len;
    tgtdist -= dist;
    while (tgtdist > 0.0f) {
        mp = nmp;
        nmpi = NextPathPt(nmpi, cp->path);
        nmp = &gDronePaths[cp->path].pts[nmpi];

        x = nmp->pos[0] - mp->pos[0];
        y = nmp->pos[1] - mp->pos[1];
        z = nmp->pos[2] - mp->pos[2];
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

    cp->tgtpos[0] = (nmp->pos[0] - mp->pos[0]) * fraction + mp->pos[0];
    cp->tgtpos[1] = (nmp->pos[1] - mp->pos[1]) * fraction + mp->pos[1];
    cp->tgtpos[2] = (nmp->pos[2] - mp->pos[2]) * fraction + mp->pos[2];
}

/* Sets model `m`'s gas and brake to make its speed `tspd` (and 5% more): the gas follows the speed shortfall
 * (an excess counts half), the brake comes on once the gas would be below -0.75; both 0 to 1, kept per slot
 * (pc_mp_gas, pc_mp_brake). On the two lowest gravity settings a front tyre's slip flag 40 cuts both. */
void AdjustSpeed(tModelData *m, f32 tspd)
{
    s32 i, slip, spddir, node, cur_time;
    f32 x, y, z, dspd, delta_speed;

    node = m->slot;

    tspd *= 1.05f;

    if ((gGravityScale < 2) && ((m->slipflag0 == 40) || (m->slipflag1 == 40))) {
        PC_MP_BRAKE(node) = 0;
        PC_MP_GAS(node) = 0;
    } else {
        x = m->rwv[0];
        y = m->rwv[1];
        z = m->rwv[2];
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

    m->throttle = PC_MP_GAS(node);
    m->brake = PC_MP_BRAKE(node);
}

/* Sets model `m`'s steer (pc_mp_steer, and the model's wheel) from the direction to steer, `pos`, in the car's
 * frame: its angle off straight ahead, 1 full one way to -1 the other, the sideways part first scaled by the step. */
void AdjustSteer(tModelData *m, f32 *pos)
{
    s32 node;
    f32 angle;

    node = m->slot;

    pos[0] *= m->dt * 108.43373f;

    angle = fatan2(pos[2], pos[0]);

    angle = range_it(angle, 0, 3.1415927f);

    PC_MP_STEER(node) = 1.0f - angle * 0.63661975f;
    m->wheel = PC_MP_STEER(node);
}

