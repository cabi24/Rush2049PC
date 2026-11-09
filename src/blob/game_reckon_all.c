#include "game/game.h"

typedef f32 F32; typedef s32 S32; typedef s16 S16;
typedef struct MODELDAT {
    u8 pad0[76];
    F32 W[3];               /* 76 */
    u8 pad58[544 - 88];
    F32 RWV[3];             /* 544 */
    F32 RWR[3];             /* 556 */
    u8 pad238[748 - 568];
    F32 UV[3][3];           /* 748 */
    u8 pad310[1812 - 784];
    F32 lasttime;           /* 1812 */
    u8 pad718[1940 - 1816];
    F32 reckonRWR[3];       /* 1940 */
    F32 reckonUV[3][3];     /* 1952 */
    u8 pad7C4[1992 - 1988];
    S16 in_game;            /* 1992 */
    u8 pad7CA[2036 - 1994];
    F32 time_fudge;         /* 2036 */
    u8 pad7F8[2056 - 2040];
} MODELDAT;

void game_reckon_all(F32 time) {
    MODELDAT *m;
    S32 i;
    F32 deltime, temp[3];

    for (i = 0; i < PC_RACE_CARS; i++) {
        m = PC_MODEL(i);
        if (m->in_game && PC_CAR_STATE(i) < 2) {
            deltime = (time - m->lasttime) * m->time_fudge;
            temp[0] = m->RWV[0] * deltime;
            temp[1] = m->RWV[1] * deltime;
            temp[2] = m->RWV[2] * deltime;
            m->reckonRWR[0] = m->RWR[0] + temp[0];
            m->reckonRWR[1] = m->RWR[1] + temp[1];
            m->reckonRWR[2] = m->RWR[2] + temp[2];
            CopyUV(m->UV, m->reckonUV);
            temp[0] = m->W[0] * deltime;
            temp[1] = m->W[1] * deltime;
            temp[2] = m->W[2] * deltime;
            RotateUV(temp, m->reckonUV);
        }
    }
}
