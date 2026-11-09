#include "game/game.h"

typedef struct PathPoint {
    s16 pos[3];
} PathPoint;

typedef struct PathSet {
    /* 0x0 */ u8 type;
    /* 0x1 */ u8 pad1[9];
    /* 0xA */ u16 count;
    /* 0xC */ PathPoint *points;
} PathSet; /* 0x10 */

typedef struct PathGraph {
    /* 0x0 */ u16 count;
    /* 0x4 */ PathPoint *points;
    /* 0x8 */ u8 numSets;
    /* 0xC */ PathSet *sets;
} PathGraph;

typedef struct Track {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s16 unk8;
    /* 0x0A */ char padA[0x24];
    /* 0x2E */ s16 unk2E;
    /* 0x30 */ char pad30[0x20];
} Track; /* 0x50 */

typedef struct TrackLen {
    /* 0x00 */ char pad0[0x58];
    /* 0x58 */ f32 unk58;
} TrackLen;

typedef struct Model {
    /* 0x000 */ char pad0[0x22C];
    /* 0x22C */ f32 pos[3];
    /* 0x238 */ char pad238[0x58E];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ char pad7C8[0x1A];
    /* 0x7E2 */ s16 unk7E2;
    /* 0x7E4 */ s16 unk7E4;
    /* 0x7E6 */ s16 unk7E6;
    /* 0x7E8 */ s8 unk7E8;
    /* 0x7E9 */ char pad7E9[0x1F];
} Model; /* 0x808 */

typedef struct Car {
    /* 0x000 */ char pad0[0xEF];
    /* 0x0EF */ s8 unkEF;
    /* 0x0F0 */ char padF0[0xA];
    /* 0x0FA */ s16 unkFA;
    /* 0x0FC */ s16 unkFC;
    /* 0x0FE */ s16 unkFE;
    /* 0x100 */ f32 unk100;
    /* 0x104 */ char pad104[0x110 - 0x104]; struct pc_visual visuals[21]; char pad104_308[0x380 - 0x308]; void *input; char pad104_384[0x3B8 - 0x384];
} Car; /* 0x3B8 */

VIEW(PathGraph, gResPath);
#define gResPath gResPath_view
VIEW(Track[], gCheckpoints);
#define gCheckpoints gCheckpoints_view

void FindMaxpathIntervals(void)
{
    s16 i;
    s32 slot;
    s32 j;
    s16 end;
    s32 k;
    Model *m;
    Model *m2;
    Car *gc;
    s32 idx;
    s32 limit;
    s32 bestSet;
    s32 bestIdx;
    s32 next;
    f32 best;
    s16 last;
    s16 pos[3];
    f32 d[3];
    f32 rel[3];
    f32 dist;
    f32 dx;
    f32 dy;
    f32 dz;

    for (i = 0; i < gNumActiveCars; i++) {
        slot = PC_MODEL_SLOT(i);
        m = PC_MODEL(slot);
        m2 = PC_MODEL(slot);
        if (AreWeSolid(slot)) {
            gc = PC_CAR(slot);
            if (gc->unkEF == 1) {
                continue;
            }
            pos[0] = m->pos[0];
            pos[1] = m->pos[1];
            pos[2] = m->pos[2];
            best = 1e20f;
            bestIdx = 0;
            bestSet = -1;
            end = 5;
            idx = gc->unkFE;
            for (j = 0; j < end; j++) {
                dx = gResPath.points[idx].pos[0] - pos[0];
                dy = gResPath.points[idx].pos[1] - pos[1];
                dz = gResPath.points[idx].pos[2] - pos[2];
                dist = dx * dx + dy * dy + dz * dz;
                if (dist < best) {
                    best = dist;
                    bestIdx = idx;
                }
                NextResPathPt(-1, idx, 0, &idx);
            }
            if (best > 10000.0f) {
                if (gc->unkFA >= 0) {
                    if (gResPath.sets[gc->unkFA].count - gc->unkFC >= 6) {
                        end = gc->unkFC + 5;
                    } else {
                        end = gResPath.sets[gc->unkFA].count;
                    }
                    for (j = gc->unkFC; j < end; j++) {
                        dx = gResPath.sets[gc->unkFA].points[j].pos[0] - pos[0];
                        dy = gResPath.sets[gc->unkFA].points[j].pos[1] - pos[1];
                        dz = gResPath.sets[gc->unkFA].points[j].pos[2] - pos[2];
                        dist = dx * dx + dy * dy + dz * dz;
                        if (dist < best) {
                            bestIdx = j;
                            bestSet = gc->unkFA;
                            best = dist;
                        }
                    }
                } else if (gc->unkFE != gc->unkFC) {
                    end = 5;
                    idx = gc->unkFC;
                    for (j = 0; j < end; j++) {
                        dx = gResPath.points[idx].pos[0] - pos[0];
                        dy = gResPath.points[idx].pos[1] - pos[1];
                        dz = gResPath.points[idx].pos[2] - pos[2];
                        dist = dx * dx + dy * dy + dz * dz;
                        if (dist < best) {
                            best = dist;
                            bestIdx = idx;
                            bestSet = -1;
                        }
                        NextResPathPt(-1, idx, 0, &idx);
                    }
                }
            }
            if (best > 40000.0f) {
                if (m->unk7E4 < m->unk7E2) {
                    end = gResPath.count;
                } else {
                    end = gCheckpoints[m->unk7E4].unk2E;
                }
                for (j = gCheckpoints[m->unk7E2].unk2E; j < end; j++) {
                    dx = gResPath.points[j].pos[0] - pos[0];
                    dy = gResPath.points[j].pos[1] - pos[1];
                    dz = gResPath.points[j].pos[2] - pos[2];
                    dist = dx * dx + dy * dy + dz * dz;
                    if (dist < best) {
                        best = dist;
                        bestIdx = j;
                        bestSet = -1;
                        if (dist <= 10000.0f) {
                            break;
                        }
                    }
                }
                if (j == end) {
                    for (k = 0; k < gResPath.numSets; k++) {
                        if (gResPath.sets[k].type == 2) {
                            continue;
                        }
                        for (j = 0; j < gResPath.sets[k].count; j++) {
                            dx = gResPath.sets[k].points[j].pos[0] - pos[0];
                            dy = gResPath.sets[k].points[j].pos[1] - pos[1];
                            dz = gResPath.sets[k].points[j].pos[2] - pos[2];
                            dist = dx * dx + dy * dy + dz * dz;
                            if (dist < best) {
                                best = dist;
                                bestIdx = j;
                                bestSet = k;
                                if (dist <= 2500.0f) {
                                    break;
                                }
                            }
                        }
                        if (j < gResPath.sets[k].count) {
                            break;
                        }
                    }
                    if (k == gResPath.numSets) {
                        for (j = gc->unkFE;;) {
                            dx = gResPath.points[j].pos[0] - pos[0];
                            dy = gResPath.points[j].pos[1] - pos[1];
                            dz = gResPath.points[j].pos[2] - pos[2];
                            dist = dx * dx + dy * dy + dz * dz;
                            if (dist < best) {
                                best = dist;
                                bestIdx = j;
                                bestSet = -1;
                                if (dist <= 10000.0f) {
                                    break;
                                }
                            }
                            j++;
                            if (j >= gResPath.count) {
                                j = 0;
                            }
                            if (j == gc->unkFE) {
                                break;
                            }
                        }
                    }
                }
            }
            gc->unkFA = bestSet;
            gc->unkFC = bestIdx;
            if (best > 40000.0f) {
                continue;
            }
            if (bestSet >= 0) {
                FindResPt(bestSet, bestIdx, &bestSet, &bestIdx, 0);
            }
            if (bestIdx < gCheckpoints[m->unk7E2].unk2E) {
                continue;
            }
            if (m->unk7E4 < m->unk7E2) {
                limit = gResPath.count;
            } else {
                limit = gCheckpoints[m->unk7E4].unk2E;
            }
            if (bestIdx >= limit) {
                continue;
            }
            gc->unkFE = bestIdx;
            if (gc->unkFA >= 0) {
                best = 0.0f;
            } else {
                NextResPathPt(-1, bestIdx, 0, &next);
                d[0] = gResPath.points[next].pos[0] - gResPath.points[bestIdx].pos[0];
                d[1] = gResPath.points[next].pos[1] - gResPath.points[bestIdx].pos[1];
                d[2] = gResPath.points[next].pos[2] - gResPath.points[bestIdx].pos[2];
                d[1] = 0.0f;
                normalize_vector_or_x(d);
                rel[0] = m->pos[0] - gResPath.points[bestIdx].pos[0];
                rel[1] = m->pos[1] - gResPath.points[bestIdx].pos[1];
                rel[2] = m->pos[2] - gResPath.points[bestIdx].pos[2];
                best = rel[0] * d[0] + rel[2] * d[2];
            }
            gc->unk100 = 0.0f;
            if (m->unk7E8 > 0) {
                gc->unk100 += gFirstLapDistance + (m->unk7E8 - 1) * gLapDistance;
                last = (m->unk7E2 < gCheckpoints[0].unk4) ? gCheckpoints[0].unk8 : m->unk7E2;
                for (idx = gCheckpoints[0].unk4; idx < last; idx++) {
                    gc->unk100 += ((TrackLen *)&gCheckpoints[idx])->unk58;
                }
                if (last != m->unk7E2) {
                    for (idx = gCheckpoints[0].unk2; idx < m2->unk7E2; idx++) {
                        gc->unk100 += ((TrackLen *)&gCheckpoints[idx])->unk58;
                    }
                }
            } else {
                for (idx = 0; idx < m2->unk7E2; idx++) {
                    gc->unk100 += ((TrackLen *)&gCheckpoints[idx])->unk58;
                }
            }
            gc->unk100 += GetPathDistance(gCheckpoints[m->unk7E2].unk2E, bestIdx);
            gc->unk100 += best;
        }
    }
}
