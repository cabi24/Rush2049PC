/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Historical label only. Accumulates one object's statistics block
 * (object->model->holder->statistics, 1,780 bytes: 140-byte prefix, 12 x 96,
 * 4 x 64, 8 x 12, 1 x 24 and 4 x 28 byte records) into the global totals at
 * gTotalRaceStats / gTotalStuntStats / gTotalBattleStats / gTotalObstacleStats / gTotalCircuitStats: u16 counters
 * and u32/f32 sums are added, flag words are OR-ed, maxima are kept, and the
 * single D record keeps the smallest positive time (0 = not yet set).
 * No arcade ancestor identified (N64 statistics screen data).
 * Layout recovered by (65 words off there); the two levers
 * that close it:
 *   - the one-record D loop is written `i != 1` (gives the `bne` pointer test;
 *     `i < 1` gives `sltu`/`bnez`);
 *   - the last loop uses its own counter `k` (with `i` reused, the destination
 *     and end pointers swap v1/a0).
 * Also MATCH at -O2 with the same source.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;
typedef struct StatsA {u8 pad0[64]; f32 total; u16 count[9]; u8 pad86[2]; u32 sum; u16 flags; u8 pad94[2];} StatsA;
typedef struct StatsB {u32 first, max0, max1, total; u16 count0, count1; u32 sum[10]; u16 flags; u8 pad62[2];} StatsB;
typedef struct StatsC {u32 first; u16 count[4];} StatsC;
typedef struct StatsD {u32 first; f32 minimum, total; u16 count[5]; u8 pad22[2];} StatsD;
typedef struct StatsE {u32 first; u16 count[6], maximum; u8 pad18[6]; u32 total;} StatsE;
typedef struct Statistics {u8 pad0[140]; StatsA a[12]; StatsB b[4]; StatsC c[8]; StatsD d[1]; StatsE e[4];} Statistics;
typedef struct Holder {Statistics *statistics;} Holder;
typedef struct Model {void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC]; Holder *holder;} Model;
typedef struct Object {Model *model;} Object;
extern StatsA gTotalRaceStats[12];
extern StatsB gTotalStuntStats[4];
extern StatsC gTotalBattleStats[8];
extern StatsD gTotalObstacleStats[1];
extern StatsE gTotalCircuitStats[4];
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
