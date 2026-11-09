#include "game/game.h"

typedef struct ROADUVS { f32 fpuvs[3][3]; } ROADUVS;
typedef struct MODELDAT {
    u8 pad0[196];
    f32 BODYFORCE[4][3];        /* 0x0C4 */
    u8 pad244[580 - 244];
    f32 TIRERWR[4][3];          /* 0x244 */
    u8 pad628[784 - 628];
    ROADUVS roaduvs[4];         /* 0x310 */
    u8 pad928[1484 - 928];
    f32 suscomp[4];             /* 0x5CC */
    f32 tpcomp[4];              /* 0x5DC */
    f32 airdist[4];             /* 0x5EC */
    f32 airvel[4];              /* 0x5FC */
    s32 roadcode[4];            /* 0x60C */
    u16 sviscode[4];            /* 0x61C */
    u8 pad1572[1592 - 1572];
    f32 idt;                    /* 0x638 */
    u8 pad1596[1608 - 1596];
    s32 unk648;                 /* 0x648, cleared each frame before the tire loop */
} MODELDAT;

void road(MODELDAT *m)
{
    f32 BODYRWV[3], igpos[3], temp[3], last_airdist;
    s32 i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            m->BODYFORCE[j][i] = 0.f;

    CollideCarBody(m);
    m->unk648 = 0;
    for (i = 0; i < 4; ++i) {
        TireSurf(m, &m->TIRERWR[i][0], igpos, &m->roadcode[i], m->roaduvs[i].fpuvs[0], i);
        last_airdist = m->airdist[i];
        m->suscomp[i] = m->tpcomp[i] - igpos[1];
        m->airdist[i] = -m->suscomp[i];
        if (m->suscomp[i] > 3.0f)
            m->suscomp[i] = 3.f;
        else if (m->suscomp[i] < -3.0f)
            m->suscomp[i] = -3.0f;
        m->tpcomp[i] = (m->suscomp[i] < 0.0f) ? 0.0f : m->suscomp[i];
        if (m->suscomp[i] < 0.0f)
            m->roadcode[i] = m->sviscode[i] = 8;
        m->airvel[i] = (m->airdist[i] - last_airdist) * m->idt;
        if (m->airvel[i] < -40.0f)
            m->airvel[i] = -40.0f;
        else if (m->airvel[i] > 40.0f)
            m->airvel[i] = 40.f;
    }
    collision(m);
}
