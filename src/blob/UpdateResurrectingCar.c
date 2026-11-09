/* UpdateResurrectingCar.c: one step of a respawn's flight (MAT3 resurrect.c, UpdateResurrectingCar()).
 *
 * A respawning car is not simulated: UpdateModel calls this each step instead, which flies it from where it
 * crashed to its respawn point along an arc (up to an apex above the midpoint, then down), turning it from its old
 * orientation to the new one on the way up. FinishResurrecting puts it back on the ground at the end. */
#include "game/game.h"

/* The respawn's state in the model record (MAT3 tModelData.resurrect, anon_pos_save_pos_70, 0x70 bytes). */
typedef struct tResurrect {
    f32 pos[3];                 /* 0x00 the respawn point */
    f32 save_pos[3];            /* 0x0C */
    f32 uvs[3][3];              /* 0x18 the orientation there */
    f32 quat_start[4];          /* 0x3C the orientation it leaves with, as a quaternion */
    f32 quat_end[4];            /* 0x4C and the one it arrives with */
    s32 velocity;               /* 0x5C its speed when it arrives, feet a second */
    f32 crash_time;             /* 0x60 */
    s16 moving_state;           /* 0x64 RES_IDLE, RES_START, then frames in flight, RES_DONE */
    f32 resurrect_time;         /* 0x68 the model time the flight began */
    s8 autoAbort;               /* 0x6C */
    s8 waterAbort;              /* 0x6D */
} tResurrect;

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    char pad0[0x220];
    f32 rwv[3];                 /* 0x220 the velocity in the world, feet a second */
    f32 rwr[3];                 /* 0x22C the position in the world, feet */
    char pad238[0x2EC - 0x238];
    f32 uvs[3][3];              /* 0x2EC the orientation */
    char pad310[0x660 - 0x310];
    tResurrect resurrect;       /* 0x660 */
    char pad6D0[0x6D4 - 0x6D0];
    f32 initin_pos[3];          /* 0x6D4 initin.pos: where the flight began (Resurrect sets it) */
    char pad6E0[0x714 - 0x6E0];
    f32 modelTime;              /* 0x714 fastin.modelTime: the model thread's race clock, seconds */
    char pad718[0x7C6 - 0x718];
    s16 slot;                   /* 0x7C6 */
    char pad7C8[2];
    s16 we_control;             /* 0x7CA this machine simulates it */
    char pad7CC[0x7D4 - 0x7CC];
    s32 appearance;             /* 0x7D4 APPEAR_* bits */
    char pad7D8[0x7DF - 0x7D8];
    s8 hide_car;                /* 0x7DF */
} tModelData;

/* The flight's apex for each car, MAT3's static save_pos[6][3]: on the PC pc_resurrect_mid, PC_MAX_LINKS long, with
 * no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6). */
#define resurrect_save_pos pc_resurrect_mid

extern f32 fabsf(f32);

/* Fly model `m` one step along its respawn arc, the flight taking resurrectTime seconds of model time (1.5, or
 * 0.01 in the race kinds that respawn at once: Resurrect sets it).
 * On the first step (RES_START) the apex is the point halfway along the straight line from where the flight began
 * to the respawn point (Interpolate at half the time), raised by an eighth of the distance, 100 feet at most, and
 * the car is shown. Each step counts moving_state up. Past the flight's time the car is done (RES_DONE: no longer a
 * hulk, and shown again if this machine controls it) and FinishResurrecting takes over. In the first half the car
 * rises from the start to the apex while its orientation turns from quat_start to quat_end, both eased in (the
 * fraction goes with the square of the time: 2 t^2 / T), and a human's car loses its velocity the same way (each
 * component under 0.01 feet a second becomes 0); the super speed cheat keeps it fast (make_super_speed). In the
 * second half it comes down from the apex to the respawn point, linearly in time, keeping its new orientation. */
void UpdateResurrectingCar(tModelData *m) {
    f32 pos[3], pos2[3], f32_state, temp, scale, uvs[3][3], quat[4];
    s16 i, node = m->slot;
    void *gc;   /* unused */

    if (m->resurrect.moving_state == RES_START) {
        pos2[0] = m->initin_pos[0] - m->resurrect.pos[0];
        pos2[1] = m->initin_pos[1] - m->resurrect.pos[1];
        pos2[2] = m->initin_pos[2] - m->resurrect.pos[2];
        temp = vector_magnitude(pos2) / 8;
        if (temp > 100) {
            temp = 100;
        }
        Interpolate(m->initin_pos, m->resurrect.pos, 3, 0.0f, resurrectTime, resurrectTime / 2, resurrect_save_pos[node]);
        resurrect_save_pos[node][1] += temp;
        m->appearance &= ~APPEAR_INVISIBLE;
    }
    m->resurrect.moving_state++;
    f32_state = m->modelTime - m->resurrect.resurrect_time;
    if (f32_state > resurrectTime) {
        if (m->appearance & APPEAR_HULK) {
            m->appearance &= ~APPEAR_HULK;
        }
        m->resurrect.moving_state = RES_DONE;
        if (PC_LINK_OWNER(node) == 6 || m->we_control) {   /* LINK_OWNER_HUMAN */
            m->hide_car = 0;
        }
    } else {
        if (m->appearance & APPEAR_HULK) {
            m->appearance &= ~APPEAR_HULK;
        }
        if (f32_state < resurrectTime / 2) {
            scale = 2 * f32_state * f32_state / resurrectTime;
            Interpolate(m->initin_pos, resurrect_save_pos[node], 3, 0.0, resurrectTime / 2, scale, pos);
            Interpolate(m->resurrect.quat_start, m->resurrect.quat_end, 4, 0.0, resurrectTime / 2, scale, quat);
            MakeUvsFromQuat(quat, uvs);
            scale = 1 - 2 * scale / resurrectTime;
            if (PC_LINK_OWNER(node) == 6) {   /* LINK_OWNER_HUMAN */
                for (i = 0; i < 3; i++) {
                    if (fabsf(m->rwv[i] *= scale) < .01f) {
                        m->rwv[i] = 0;
                    }
                }
            }
            CopyUV(&uvs[0][0], &m->uvs[0][0]);
            if (gSuperSpeed) {
                make_super_speed(m);
            }
        } else {
            Interpolate(resurrect_save_pos[node], m->resurrect.pos, 3, resurrectTime / 2, resurrectTime, f32_state, pos);
        }
        m->rwr[0] = pos[0];
        m->rwr[1] = pos[1];
        m->rwr[2] = pos[2];
    }
}
