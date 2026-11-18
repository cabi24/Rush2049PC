/* checkok/group.c: the crash test after each physics step (MAT3 drivsym.c, checkok()).
 *
 * The physics step (sym) calls it: suspension thumps for the sounds and sparks, a wreck from too hard an impact, too
 * much speed or spin, and a car stuck on its roof or side. */
#include "game/game.h"

/* Check model m after its step (a car that is out of the race, CAR_DYING or CAR_DEAD, is left alone):
 * - each suspension compressed past 0.6, 0.9 or 1.2 ft is a thump of level 1, 2 or 3: the hardest goes to thumpflag,
 *   the tires that reached it to thump_side;
 * - the impact: the total force less a share of the tires' (0.4 of it when all four take more than 0.15 of the
 *   threshold, 0.2 with three) against the crash threshold; that, or more than 400 ft/s, or a yaw rate past 10
 *   rad/s, wrecks the car (crashflag), except in a battle and for a human with the invincibility cheat;
 * - the body pressed hard (700) onto the road at a corner, with diagonal tires 3 to 5 ft in the air counting extra:
 *   upside down and nearly stopped, or on its side below 40 ft/s, or tipped over 45 degrees below 100 ft/s, the car
 *   is wrecked and loses a battle point (AddBattlePoint); tipped that far it scrapes its roof (topscrape). */
void checkok(tModelData *m)
{
    f32 forceavg;
    s32 thump_it;
    s16 thump_side;
    s16 i;
    f32 t;
    f32 tt;
    f32 tot;
    s32 cnt;
    f32 temp;

    thump_it = 0;
    thump_side = 0;
    if (PC_CAR_DEAD(m->slot) != CAR_ALIVE) {
        return;
    }
    for (i = 0; i < 4; i++) {
        if (m->tpcomp[i] > 1.2f && thump_it < 3) {
            thump_it = 3;
            thump_side |= 1 << i;
        } else if (m->tpcomp[i] > 0.9f && thump_it < 2) {
            thump_it = 2;
            thump_side |= 1 << i;
        } else if (m->tpcomp[i] > 0.6f && thump_it < 1) {
            thump_it = 1;
            thump_side |= 1 << i;
        }
    }
    m->thump_side = thump_side;
    m->thumpflag = thump_it;
    forceavg = vector_magnitude(m->f);
    if (forceavg > m->crashthreshold) {
        t = m->crashthreshold * 0.15f;
        cnt = 0;
        tot = 0.0f;
        tt = vector_magnitude(m->tireForce[0]);
        if (tt > t) {
            tot += tt;
            cnt++;
        }
        tt = vector_magnitude(m->tireForce[1]);
        if (tt > t) {
            tot += tt;
            cnt++;
        }
        tt = vector_magnitude(m->tireForce[2]);
        if (tt > t) {
            tot += tt;
            cnt++;
        }
        tt = vector_magnitude(m->tireForce[3]);
        if (tt > t) {
            tot += tt;
            cnt++;
        }
        if (cnt >= 4) {
            forceavg -= tot * 0.4f;
        } else if (cnt >= 3) {
            forceavg -= tot * 0.2f;
        }
    }
    if ((gRace != kBattle /* kBattle */ && forceavg > m->crashthreshold) ||
        (gRace != kBattle && (m->magvel > 400.0f || m->w[1] > 10.0f || m->w[1] < -10.0f))) {
        if (!m->crashflag && (m->drone_type != DRONE_TYPE_HUMAN || !gInvincible)) {
            m->crashflag = 1;
        }
    }
    m->topscrape = 0;
    temp = 0.0f;
    if (m->bodyForce[2][1] < -700.0f || m->bodyForce[3][1] < -700.0f || m->bodyForce[0][1] < -700.0f || m->bodyForce[1][1] < -700.0f) {
        temp = 1.0f;
        if (m->airdist[1] > 3.0f && m->airdist[1] < 5.0f && m->airdist[2] > 3.0f && m->airdist[2] < 5.0f) {
            temp += 1.0f;
        }
        if (m->airdist[0] > 3.0f && m->airdist[0] < 5.0f && m->airdist[3] > 3.0f && m->airdist[3] < 5.0f) {
            temp += 1.0f;
        }
    }
    if (temp > 0.0f && m->uvs[1][1] < -0.1f && m->magvel < 5.0f) {
        if (!m->crashflag) {
            AddBattlePoint(m->slot, m->slot, -1);
            m->crashflag = 1;
        }
        return;
    }
    if (temp >= 2.0f && m->magvel < 40.0f) {
        if (!m->crashflag) {
            AddBattlePoint(m->slot, m->slot, -1);
            m->crashflag = 1;
        }
        return;
    }
    if (temp >= 2.0f && m->uvs[1][1] < 0.707f) {
        if (m->magvel < 100.0f && !m->crashflag) {
            AddBattlePoint(m->slot, m->slot, -1);
            m->crashflag = 1;
        }
        m->topscrape = 1;
    }
}
