#include "game/game.h"

typedef struct MODELDAT {
    char pad0[0x220];
    f32 RWV[3];                 /* 0x220 */
    f32 RWR[3];                 /* 0x22C */
    char pad238[0x2EC - 0x238];
    f32 uvs[3][3];              /* 0x2EC */
    char pad310[0x660 - 0x310];
    f32 res_pos[3];             /* 0x660 */
    char pad66C[0x69C - 0x66C];
    f32 quat_start[4];          /* 0x69C */
    f32 quat_end[4];            /* 0x6AC */
    char pad6BC[0x6C4 - 0x6BC];
    s16 moving_state;           /* 0x6C4 */
    char pad6C6[2];
    f32 resurrect_time;         /* 0x6C8 */
    char pad6CC[0x6D4 - 0x6CC];
    f32 init_pos[3];            /* 0x6D4 */
    char pad6E0[0x714 - 0x6E0];
    f32 time;                   /* 0x714 */
    char pad718[0x7C6 - 0x718];
    s16 net_node;               /* 0x7C6 */
    char pad7C8[2];
    s16 we_control;             /* 0x7CA */
    char pad7CC[0x7D4 - 0x7CC];
    s32 appearance;             /* 0x7D4 */
    char pad7D8[0x7DF - 0x7D8];
    s8 hide_car;                /* 0x7DF */
} MODELDAT;

/* On the PC the resurrect's midpoints are pc_resurrect_mid, PC_MAX_LINKS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 6). */
#define resurrect_save_pos pc_resurrect_mid

#define ABS(x) (((x) < 0) ? -(x) : (x))
extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

void UpdateResurrectingCar(MODELDAT *m) {
    f32 pos[3], pos2[3], f32_state, temp, scale, uvs[3][3], quat[4];
    s16 i, node = m->net_node;
    void *gc;

    if (m->moving_state == 0) {
        pos2[0] = m->init_pos[0] - m->res_pos[0];
        pos2[1] = m->init_pos[1] - m->res_pos[1];
        pos2[2] = m->init_pos[2] - m->res_pos[2];
        temp = vector_magnitude(pos2) / 8;
        if (temp > 100) {
            temp = 100;
        }
        Interpolate(m->init_pos, m->res_pos, 3, 0.0f, resurrectTime, resurrectTime / 2, resurrect_save_pos[node]);
        resurrect_save_pos[node][1] += temp;
        m->appearance &= ~8;
    }
    m->moving_state++;
    f32_state = m->time - m->resurrect_time;
    if (f32_state > resurrectTime) {
        if (m->appearance & 0x10) {
            m->appearance &= ~0x10;
        }
        m->moving_state = -2;
        if (PC_LINK_OWNER(node) == 6 || m->we_control) {
            m->hide_car = 0;
        }
    } else {
        if (m->appearance & 0x10) {
            m->appearance &= ~0x10;
        }
        if (f32_state < resurrectTime / 2) {
            scale = 2 * f32_state * f32_state / resurrectTime;
            Interpolate(m->init_pos, resurrect_save_pos[node], 3, 0.0, resurrectTime / 2, scale, pos);
            Interpolate(m->quat_start, m->quat_end, 4, 0.0, resurrectTime / 2, scale, quat);
            MakeUvsFromQuat(quat, uvs);
            scale = 1 - 2 * scale / resurrectTime;
            if (PC_LINK_OWNER(node) == 6) {
                for (i = 0; i < 3; i++) {
                    if (fabsf(m->RWV[i] *= scale) < .01f) {
                        m->RWV[i] = 0;
                    }
                }
            }
            CopyUV(&uvs[0][0], &m->uvs[0][0]);
            if (gSuperSpeed) {
                make_super_speed(m);
            }
        } else {
            Interpolate(resurrect_save_pos[node], m->res_pos, 3, resurrectTime / 2, resurrectTime, f32_state, pos);
        }
        m->RWR[0] = pos[0];
        m->RWR[1] = pos[1];
        m->RWR[2] = pos[2];
    }
}
