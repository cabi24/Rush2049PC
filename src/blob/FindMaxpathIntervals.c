/* FindMaxpathIntervals: where each car is along the resurrection path, and from that how far round the race it has
 * got, which orders the cars (MAT3 maxpath.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the fields this reads. */
typedef struct Model {
    /* 0x000 */ _tCar *car;
    /* 0x004 */ Tune44 *tune;
    /* 0x008 */ char pad8[0x224];
    /* 0x22C */ f32 rwr[3];        /* its position */
    /* 0x238 */ char pad238[0x168];
    /* 0x3A0 */ streetyp *lasttp[4];
    /* 0x3B0 */ char pad3B0[0x5C];
    /* 0x40C */ const s16 *torquecurve;
    /* 0x410 */ char pad410[0x238];
    /* 0x648 */ surftyp *movingSurface;
    /* 0x64C */ char pad64C[0x17A];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ char pad7C8[0x1A];
    /* 0x7E2 */ s16 last_cp_index; /* the last checkpoint it passed */
    /* 0x7E4 */ s16 next_cp_index; /* the next one */
    /* 0x7E6 */ s16 drone_target;
    /* 0x7E8 */ s8 laps;           /* laps completed */
    /* 0x7E9 */ char pad7E9[0x1F];
} Model; /* 0x808 */

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): the fields this reads and sets. */
typedef struct Car {
    /* 0x000 */ char pad0[0xEF];
    /* 0x0EF */ s8 place_locked;   /* 1 once it has finished */
    /* 0x0F0 */ char padF0[0xA];
    /* 0x0FA */ s16 lastResPath;   /* the path of the point last found for it: an alternative path, -1 the main one */
    /* 0x0FC */ s16 lastResIdx;    /* that point */
    /* 0x0FE */ s16 resIdx;        /* its point on the main path, where the searches start */
    /* 0x100 */ f32 distance;      /* how far round the race it has got, in feet (the HUD, the places) */
    /* 0x104 */ char pad104[0x110 - 0x104]; struct pc_visual visuals[21]; char pad104_308[0x380 - 0x308]; void *input; char pad104_384[0x3B8 - 0x384];
} Car; /* 0x3B8 */

/* For each car that is solid and still racing: finds the nearest point of the resurrection path to it, looking
 * first at the 5 main-path points from its last one, then (more than 100 feet off) on along the branch it was on or
 * at the 5 from its last point, then (more than 200 feet off) through its checkpoint section of the main path,
 * every branch cars may be put back on, and the whole main path from its point; each search stops early on a point
 * close enough. A point on a branch is mapped onto the main path (FindResPt). If that point lies in the car's
 * checkpoint section, its distance round the race is the laps done, the checkpoint stretches since the lap's
 * start, the path from its last checkpoint to the point, and how far past the point it is along the path. */
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
    s32 bestPath;
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
            if (gc->place_locked == 1) {
                continue;
            }
            pos[0] = m->rwr[0];
            pos[1] = m->rwr[1];
            pos[2] = m->rwr[2];
            best = 1e20f;
            bestIdx = 0;
            bestPath = -1;
            end = 5;
            idx = gc->resIdx;
            for (j = 0; j < end; j++) {
                dx = gResPath.pts[idx][0] - pos[0];
                dy = gResPath.pts[idx][1] - pos[1];
                dz = gResPath.pts[idx][2] - pos[2];
                dist = dx * dx + dy * dy + dz * dz;
                if (dist < best) {
                    best = dist;
                    bestIdx = idx;
                }
                NextResPathPt(-1, idx, 0, &idx);
            }
            if (best > 10000.0f) {
                /* more than 100 feet off: along the branch it was on, or the main path from its last point */
                if (gc->lastResPath >= 0) {
                    if (gResPath.alt[gc->lastResPath].numPts - gc->lastResIdx >= 6) {
                        end = gc->lastResIdx + 5;
                    } else {
                        end = gResPath.alt[gc->lastResPath].numPts;
                    }
                    for (j = gc->lastResIdx; j < end; j++) {
                        dx = gResPath.alt[gc->lastResPath].pts[j][0] - pos[0];
                        dy = gResPath.alt[gc->lastResPath].pts[j][1] - pos[1];
                        dz = gResPath.alt[gc->lastResPath].pts[j][2] - pos[2];
                        dist = dx * dx + dy * dy + dz * dz;
                        if (dist < best) {
                            bestIdx = j;
                            bestPath = gc->lastResPath;
                            best = dist;
                        }
                    }
                } else if (gc->resIdx != gc->lastResIdx) {
                    end = 5;
                    idx = gc->lastResIdx;
                    for (j = 0; j < end; j++) {
                        dx = gResPath.pts[idx][0] - pos[0];
                        dy = gResPath.pts[idx][1] - pos[1];
                        dz = gResPath.pts[idx][2] - pos[2];
                        dist = dx * dx + dy * dy + dz * dz;
                        if (dist < best) {
                            best = dist;
                            bestIdx = idx;
                            bestPath = -1;
                        }
                        NextResPathPt(-1, idx, 0, &idx);
                    }
                }
            }
            if (best > 40000.0f) {
                /* more than 200 feet off: its checkpoint section, then the branches, then the whole main path */
                if (m->next_cp_index < m->last_cp_index) {
                    end = gResPath.numPts;
                } else {
                    end = gCheckpoints.points[m->next_cp_index].resPathPt;
                }
                for (j = gCheckpoints.points[m->last_cp_index].resPathPt; j < end; j++) {
                    dx = gResPath.pts[j][0] - pos[0];
                    dy = gResPath.pts[j][1] - pos[1];
                    dz = gResPath.pts[j][2] - pos[2];
                    dist = dx * dx + dy * dy + dz * dz;
                    if (dist < best) {
                        best = dist;
                        bestIdx = j;
                        bestPath = -1;
                        if (dist <= 10000.0f) {
                            break;
                        }
                    }
                }
                if (j == end) {
                    for (k = 0; k < gResPath.numAlt; k++) {
                        if (gResPath.alt[k].resurrect == ALT_PATH_ALONE) {
                            continue;
                        }
                        for (j = 0; j < gResPath.alt[k].numPts; j++) {
                            dx = gResPath.alt[k].pts[j][0] - pos[0];
                            dy = gResPath.alt[k].pts[j][1] - pos[1];
                            dz = gResPath.alt[k].pts[j][2] - pos[2];
                            dist = dx * dx + dy * dy + dz * dz;
                            if (dist < best) {
                                best = dist;
                                bestIdx = j;
                                bestPath = k;
                                if (dist <= 2500.0f) {
                                    break;
                                }
                            }
                        }
                        if (j < gResPath.alt[k].numPts) {
                            break;
                        }
                    }
                    if (k == gResPath.numAlt) {
                        for (j = gc->resIdx;;) {
                            dx = gResPath.pts[j][0] - pos[0];
                            dy = gResPath.pts[j][1] - pos[1];
                            dz = gResPath.pts[j][2] - pos[2];
                            dist = dx * dx + dy * dy + dz * dz;
                            if (dist < best) {
                                best = dist;
                                bestIdx = j;
                                bestPath = -1;
                                if (dist <= 10000.0f) {
                                    break;
                                }
                            }
                            j++;
                            if (j >= gResPath.numPts) {
                                j = 0;
                            }
                            if (j == gc->resIdx) {
                                break;
                            }
                        }
                    }
                }
            }
            gc->lastResPath = bestPath;
            gc->lastResIdx = bestIdx;
            if (best > 40000.0f) {
                continue;
            }
            if (bestPath >= 0) {
                FindResPt(bestPath, bestIdx, &bestPath, &bestIdx, 0);
            }
            if (bestIdx < gCheckpoints.points[m->last_cp_index].resPathPt) {
                continue;
            }
            if (m->next_cp_index < m->last_cp_index) {
                limit = gResPath.numPts;
            } else {
                limit = gCheckpoints.points[m->next_cp_index].resPathPt;
            }
            if (bestIdx >= limit) {
                continue;
            }
            gc->resIdx = bestIdx;
            if (gc->lastResPath >= 0) {
                best = 0.0f;
            } else {
                NextResPathPt(-1, bestIdx, 0, &next);
                d[0] = gResPath.pts[next][0] - gResPath.pts[bestIdx][0];
                d[1] = gResPath.pts[next][1] - gResPath.pts[bestIdx][1];
                d[2] = gResPath.pts[next][2] - gResPath.pts[bestIdx][2];
                d[1] = 0.0f;
                normalize_vector_or_x(d);
                rel[0] = m->rwr[0] - gResPath.pts[bestIdx][0];
                rel[1] = m->rwr[1] - gResPath.pts[bestIdx][1];
                rel[2] = m->rwr[2] - gResPath.pts[bestIdx][2];
                best = rel[0] * d[0] + rel[2] * d[2];
            }
            /* how far round the race: the laps done, the stretches since the lap's start, then within this one */
            gc->distance = 0.0f;
            if (m->laps > 0) {
                gc->distance += gFirstLapDistance + (m->laps - 1) * gLapDistance;
                last = (m->last_cp_index < gCheckpoints.finishPt) ? gCheckpoints.numPoints : m->last_cp_index;
                for (idx = gCheckpoints.finishPt; idx < last; idx++) {
                    gc->distance += gCheckpoints.points[idx].distToNext;
                }
                if (last != m->last_cp_index) {
                    for (idx = gCheckpoints.loopPt; idx < m2->last_cp_index; idx++) {
                        gc->distance += gCheckpoints.points[idx].distToNext;
                    }
                }
            } else {
                for (idx = 0; idx < m2->last_cp_index; idx++) {
                    gc->distance += gCheckpoints.points[idx].distToNext;
                }
            }
            gc->distance += GetPathDistance(gCheckpoints.points[m->last_cp_index].resPathPt, bestIdx);
            gc->distance += best;
        }
    }
}
