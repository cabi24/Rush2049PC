/* checkok/group.c: the crash test after each physics step (MAT3 drivsym.c, checkok()).
 *
 * The physics step (sym) calls it: suspension thumps for the sounds and sparks, a wreck from too hard an impact, too
 * much speed or spin, and a car stuck on its roof or side. */
#include "game/game.h"

/* The model record (tModelData), the fields checkok reads and writes. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x8];
    f32 f[3];                   /* +0x010 the total force */
    u8 pad1C[0x4C - 0x1C];
    f32 w[3];                   /* +0x04C the angular velocity (w[1]: the yaw rate) */
    u8 pad58[0x64 - 0x58];
    f32 tireForce[4][3];        /* +0x064 */
    u8 pad94[0xC4 - 0x94];
    f32 bodyForce[4][3];        /* +0x0C4 the body corners' forces ([i][1] up: the road pushing the body) */
    u8 padF4[0x2EC - 0xF4];
    f32 uvs[3][3];              /* +0x2EC the orientation: uvs[1][1] the up axis vertically (1 upright) */
    u8 pad310[0x3A0 - 0x310];
    streetyp *lasttp[4];
    u8 pad3B0[0x3F0 - 0x3B0];
    f32 magvel;                 /* +0x3F0 the speed, feet a second */
    u8 pad3F4[0x40C - 0x3F4];
    const s16 *torquecurve;
    u8 pad410[0x5DC - 0x410];
    f32 tpcomp[4];              /* +0x5DC each suspension's compression */
    f32 airdist[4];             /* +0x5EC each tire's height above the road */
    u8 pad5FC[0x63C - 0x5FC];
    f32 crashthreshold;         /* +0x63C */
    s8 crashflag;               /* +0x640 */
    s8 topscrape;               /* +0x641 the roof scraping */
    s8 thumpflag;               /* +0x642 the hardest suspension thump this step, 0..3 */
    s8 pad643;
    s16 thump_side;             /* +0x644 the tires that thumped, a bit each */
    u8 pad646[0x648 - 0x646];
    surftyp *movingSurface;
    u8 pad64C[0x7C6 - 0x64C];
    s16 slot;                   /* +0x7C6 */
    u8 pad7C8[0x7CC - 0x7C8];
    s8 drone_type;              /* +0x7CC DRONE_TYPE_* */
} tModelData;

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
