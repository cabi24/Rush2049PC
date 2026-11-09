#include "game/game.h"

typedef struct StatsA {u8 pad0[64]; f32 total; u16 count[9]; u8 pad86[2]; u32 sum; u16 flags; u8 pad94[2];} StatsA;

typedef struct Statistics {u8 pad0[140]; StatsA a[12]; StatsB b[4]; StatsC c[8]; StatsD d[1]; StatsE e[4];} Statistics;
typedef struct Holder {Statistics *statistics;} Holder;
typedef struct Model {void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC]; Holder *holder;} Model;
typedef struct Object {Model *model;} Object;
VIEW(StatsA[12], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view
VIEW(StatsB[4], gTotalStuntStats);
#define gTotalStuntStats gTotalStuntStats_view
VIEW(StatsC[8], gTotalBattleStats);
#define gTotalBattleStats gTotalBattleStats_view
VIEW(StatsD[1], gTotalObstacleStats);
#define gTotalObstacleStats gTotalObstacleStats_view
VIEW(StatsE[4], gTotalCircuitStats);
#define gTotalCircuitStats gTotalCircuitStats_view
void AddPlayerStats(Object *object) {
    Statistics *s;
    s32 i, j, k;
    if (!object->model->holder) return;
    s = object->model->holder->statistics;
    for (i = 0; i < 12; i++) {
        gTotalRaceStats[i].total += s->a[i].total;
        gTotalRaceStats[i].count[0] += s->a[i].count[0];
        gTotalRaceStats[i].count[1] += s->a[i].count[1];
        gTotalRaceStats[i].count[2] += s->a[i].count[2];
        gTotalRaceStats[i].count[3] += s->a[i].count[3];
        gTotalRaceStats[i].count[4] += s->a[i].count[4];
        gTotalRaceStats[i].count[5] += s->a[i].count[5];
        gTotalRaceStats[i].count[6] += s->a[i].count[6];
        gTotalRaceStats[i].count[7] += s->a[i].count[7];
        gTotalRaceStats[i].count[8] += s->a[i].count[8];
        gTotalRaceStats[i].sum += s->a[i].sum;
        gTotalRaceStats[i].flags |= s->a[i].flags;
    }
    for (i = 0; i < 4; i++) {
        if (gTotalStuntStats[i].max0 < s->b[i].max0) gTotalStuntStats[i].max0 = s->b[i].max0;
        if (gTotalStuntStats[i].max1 < s->b[i].max1) gTotalStuntStats[i].max1 = s->b[i].max1;
        gTotalStuntStats[i].total += s->b[i].total;
        gTotalStuntStats[i].count0 += s->b[i].count0;
        gTotalStuntStats[i].count1 += s->b[i].count1;
        for (j = 0; j < 10; j++) gTotalStuntStats[i].sum[j] += s->b[i].sum[j];
        gTotalStuntStats[i].flags |= s->b[i].flags;
    }
    for (i = 0; i < 8; i++) {
        gTotalBattleStats[i].count[0] += s->c[i].count[0];
        gTotalBattleStats[i].count[1] += s->c[i].count[1];
        gTotalBattleStats[i].count[2] += s->c[i].count[2];
        gTotalBattleStats[i].count[3] += s->c[i].count[3];
    }
    for (i = 0; i != 1; i++) {
        if (s->d[i].minimum > 0.0f && (gTotalObstacleStats[i].minimum == 0.0f || s->d[i].minimum < gTotalObstacleStats[i].minimum)) gTotalObstacleStats[i].minimum = s->d[i].minimum;
        gTotalObstacleStats[i].total += s->d[i].total;
        gTotalObstacleStats[i].count[0] += s->d[i].count[0];
        gTotalObstacleStats[i].count[1] += s->d[i].count[1];
        gTotalObstacleStats[i].count[2] += s->d[i].count[2];
        gTotalObstacleStats[i].count[3] += s->d[i].count[3];
        gTotalObstacleStats[i].count[4] += s->d[i].count[4];
    }
    for (k = 0; k < 4; k++) {
        gTotalCircuitStats[k].count[0] += s->e[k].count[0];
        gTotalCircuitStats[k].count[1] += s->e[k].count[1];
        gTotalCircuitStats[k].count[2] += s->e[k].count[2];
        gTotalCircuitStats[k].count[3] += s->e[k].count[3];
        gTotalCircuitStats[k].count[4] += s->e[k].count[4];
        gTotalCircuitStats[k].count[5] += s->e[k].count[5];
        if (gTotalCircuitStats[k].maximum < s->e[k].maximum) gTotalCircuitStats[k].maximum = s->e[k].maximum;
        gTotalCircuitStats[k].total += s->e[k].total;
    }
}
