/* resurrect.c: placing cars at the race start and respawning them (MAT3 resurrect.c: Resurrect(), FindHumanResPt(),
 * FindDroneResPt(), FindSurfaceUvsInPlace(), FindBestQuat()).
 *
 * Resurrect sets a car's flight up when it respawns after a crash or an abort (UpdateModel calls it when the car's
 * crash_state is CRASH_RESPAWN), and at the race start places it on the grid. A human respawns on the resurrection
 * path (FindHumanResPt), a drone on its own drone path; in racing modes the respawn point is moved on along the
 * path by the distance the car would have covered while wrecked and in flight. UpdateResurrectingCar then flies the
 * car there and FinishResurrecting drops it in. The PC adds the grid places past the N64's six (pc_pole_row,
 * pc_grid_start: docs/port/MORECARS.md 4). */
#include "game/game.h"

typedef struct tTire {          /* anon_tradius_springK_5c */
    float tradius;              /* 0x00 */
    u8 pad04[0x48 - 0x04];
    float angvel;               /* 0x48 */
    u8 pad4C[0x5C - 0x4C];
} tTire;

typedef struct tResurrect {     /* anon_pos_save_pos_70 */
    float pos[3];               /* 0x00 */
    float save_pos[3];          /* 0x0c */
    float uvs[3][3];            /* 0x18 */
    float quat_start[4];        /* 0x3c */
    float quat_end[4];          /* 0x4c */
    int velocity;               /* 0x5c */
    float crash_time;           /* 0x60 */
    s16 moving_state;           /* 0x64 */
    float resurrect_time;       /* 0x68 */
    s8 autoAbort;               /* 0x6c */
    s8 waterAbort;              /* 0x6d */
} tResurrect;

typedef struct tInitIn {        /* anon_polepos_pos_40 */
    s16 polepos;                /* 0x00 */
    float pos[3];               /* 0x04 */
    float uvs[3][3];            /* 0x10 */
    float offset[3];            /* 0x34 */
} tInitIn;

typedef struct tFastIn {        /* anon_modelIteration_modelTime_10 */
    int modelIteration;         /* 0x0 */
    float modelTime;            /* 0x4 */
    float dt;                   /* 0x8 */
    s16 modelrun;               /* 0xc */
} tFastIn;

typedef struct tReckon {        /* anon_base_rwa_base_rwv_90 */
    u8 pad00[0x60];
    float rwr[3];               /* 0x60 */
    float uvs[3][3];            /* 0x6c */
} tReckon;

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here: MAT3's names and layout. */
typedef struct tModelData {
    u8 pad000[0x40];
    float v[3];                 /* 0x040 */
    u8 pad04C[0x94 - 0x4C];
    float tireV[4][3];          /* 0x094 */
    float bodyForce[4][3];      /* 0x0c4 */
    u8 pad0F4[0x220 - 0xF4];
    float rwv[3];               /* 0x220 */
    float rwr[3];               /* 0x22c */
    u8 pad238[0x2EC - 0x238];
    float uvs[3][3];            /* 0x2ec */
    u8 pad310[0x430 - 0x310];
    tTire tires[4];             /* 0x430 */
    u8 pad5A0[0x5CC - 0x5A0];
    float suscomp[4];           /* 0x5cc */
    u8 pad5DC[0x62C - 0x5DC];
    u16 sound_flags[4];         /* 0x62c */
    u8 pad634[0x640 - 0x634];
    s8 crashflag;               /* 0x640 */
    s8 topscrape;               /* 0x641 */
    s8 thumpflag;               /* 0x642 */
    s8 mthumpflag;              /* 0x643 */
    u8 pad644[0x660 - 0x644];
    tResurrect resurrect;       /* 0x660 */
    tInitIn initin;             /* 0x6d0 */
    tFastIn fastin;             /* 0x710 */
    u8 pad720[0x734 - 0x720];
    tReckon reckon;             /* 0x734 */
    s16 obResPt;                /* 0x7c4 */
    s16 slot;                   /* 0x7c6 */
    u8 pad7C8[0x7D4 - 0x7C8];
    u32 appearance;             /* 0x7d4 */
    u8 pad7D8[0x7DF - 0x7D8];
    s8 hide_car;                /* 0x7df */
    s16 please_abort_me;        /* 0x7e0 */
    s16 last_cp_index;          /* 0x7e2 */
    s16 next_cp_index;          /* 0x7e4 */
    s16 drone_target;           /* 0x7e6 */
    s8 laps;                    /* 0x7e8 */
    s8 cpSign;                  /* 0x7e9 */
    s8 collidable;              /* 0x7ea */
    s8 solid;                   /* 0x7eb */
    u8 pad7EC[0x808 - 0x7EC];
} tModelData;

typedef struct tPathPoint {
    float xrel;                 /* 0x00 */
    float yrel;                 /* 0x04 */
    float cyrel;                /* 0x08 */
    float len;                  /* 0x0c */
    float tgtspd;               /* 0x10 */
    float tgtpos[3];            /* 0x14 */
    float interval_time;        /* 0x20 */
    s16 idx;                    /* 0x24 */
    s16 newIdx;                 /* 0x26 */
    s16 path;                   /* 0x28 */
    s16 basePath;               /* 0x2a */
} tPathPoint;

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields used here. */
typedef struct tCarData {       /* N64 0x3B8 */
    u8 pad000[0xED];
    s8 restart;                 /* 0x0ed */
    u8 pad0EE[0x110 - 0xEE]; struct pc_visual visuals[21];
    s8 collidable;              /* 0x308 */
    s8 solid;                   /* 0x309 */
    float collide_time;         /* 0x30c */
    s8 collide_count;           /* 0x310 */
    s8 collide_state;           /* 0x311 */
    tPathPoint mpctl;           /* 0x314 */
    u8 pad340[0x344 - 0x340];
    u16 sound_flags[4];         /* 0x344 */
    u8 pad34C[0x380 - 0x34C]; void *input; u8 pad34C_384[0x3B8 - 0x384];
} tCarData;

typedef struct tDronePt {       /* anon_pos_mph_8 */
    s16 pos[3];
    u8 mph;
    s8 hints;
} tDronePt;

typedef struct tDronePath {     /* anon_numPts_pts_8 */
    u16 numPts;
    tDronePt *pts;
} tDronePath;

/* MAT3's names for the race kinds (gRace), the human owner of a car slot and the attract mode's game state (the race
 * area's values: for race.h to declare). */

#define kOneRace 0
#define kPractice 1
#define kGhost 2
#define kCircuit 3
#define kStunt 4
#define kObstacle 5
#define kBattle 6
#define LINK_OWNER_HUMAN 6
#define GS_DEMO 0x8


extern float sinf(float);
extern float cosf(float);
extern float fabsf(float);

surftyp *RoadPosition(float *ipos, float *opos, float (*uvs)[3]);

int CountdownTimeOut(void);

/* PC: a start-grid row for each of PC_MAX_LINKS places (pc_game_types.h, docs/port/MORECARS.md 4). Places 0-5 are
 * pole_pos_offset's rows, read from the loaded table when a car is placed, so the build holds none of its values.
 * The table's grid is two cars a row, 3 rows; each later place continues the places of its column parity two rows
 * before it, moved by their two-row step: row[p] = row[p - 4] + (row[4 + p % 2] - row[p % 2]). So the rows keep the
 * table's spacing and its alternation, whatever they are. A respawn, PC_RESPAWN_BASE + slot (PC_RESPAWN_BASE in a
 * ghost race), reads the table's respawn rows, 6 + slot, as the N64's index + 6 did. */
#define PC_GRID_PLACES 6
#define PC_GRID_COLUMNS 2
#define PC_GRID_PERIOD (PC_GRID_PLACES - PC_GRID_COLUMNS)
float pc_pole_offsets[PC_MAX_LINKS][3];

/* The start grid's row for `place`: its offset from the start point in the car's frame (x sideways, z back), feet.
 * (Fills pc_pole_offsets on each call.) */
float *pc_pole_row(int place)
{
    int p, k, c;
    if (place >= PC_RESPAWN_BASE)
        return pole_pos_offset[place - PC_RESPAWN_BASE + PC_GRID_PLACES];
    for (p = 0; p < PC_MAX_LINKS; p++) {
        c = p % PC_GRID_COLUMNS;
        for (k = 0; k < 3; k++)
            pc_pole_offsets[p][k] = p < PC_GRID_PLACES ? pole_pos_offset[p][k] :
                pc_pole_offsets[p - PC_GRID_PERIOD][k] + (pole_pos_offset[PC_GRID_PERIOD + c][k] - pole_pos_offset[c][k]);
    }
    return pc_pole_offsets[place];
}

/* The start of a place past the N64's grid stands on the road. Its candidates, in this order: the rows continued
 * back (places 6 to PC_MAX_LINKS - 1 of pc_pole_row), each from the point of the resurrection path its row's distance
 * behind the start point (walked back from the start checkpoint's point, the one SetResurrectionData put every car on,
 * and across the loop point; where the path has no point behind, the rest straight back), facing along the path
 * there (FindSurfaceUvsFromResPoint, as SetResurrectionData faces the start point), with its row's sideways offset; then a
 * column outside the table's grid on each side, beside each of its three rows, at the row's own sideways spacing,
 * from the start point. A candidate counts if the road is under it (RoadPosition, RoadPosition), and place
 * 6 + n takes the nth that counts (its own row if too few do). So where the road behind the start bends or dips the
 * rows follow it, and where it ends (the retail track 5 starts at the beginning of its path, with about 75 ft of
 * road behind the pole) the cars that don't fit stand beside the grid. Places 0-5, respawns, the attract demo (each
 * car on its own drone path) and the modes that give no offset keep the start point and their table row.
 * pc_grid_start(m, 0) is the point, 1 the orientation, 2 the offset. */
#define PC_GRID_ROWS (PC_GRID_PLACES / PC_GRID_COLUMNS)
#define PC_GRID_BACK (PC_MAX_LINKS - PC_GRID_PLACES)
#define PC_GRID_CANDIDATES (PC_GRID_BACK + PC_GRID_ROWS * PC_GRID_COLUMNS)
static float pc_grid_point[3], pc_grid_uvs[3][3], pc_grid_offset[3];

/* Candidate c's point, orientation and offset, into pc_grid_*: c below PC_GRID_BACK the row continued back for
 * place PC_GRID_PLACES + c, walked back along the resurrection path from model m's start checkpoint; from
 * PC_GRID_BACK on, the column beside the grid (side 0 left, 1 right) at row (c - PC_GRID_BACK) / 2. */
static void pc_grid_candidate(tModelData *m, int c)
{
    int s, j, q, loop, k, side;
    const float *row, *a, *b, *in;
    float need, done, seg, t, d[3], gap;
    if (c >= PC_GRID_BACK) {   /* beside the grid: the row's outer car moved out by the row's spacing */
        k = (c - PC_GRID_BACK) / PC_GRID_COLUMNS;
        side = (c - PC_GRID_BACK) % PC_GRID_COLUMNS;
        a = pole_pos_offset[k * PC_GRID_COLUMNS];
        b = pole_pos_offset[k * PC_GRID_COLUMNS + 1];
        gap = a[0] < b[0] ? b[0] - a[0] : a[0] - b[0];
        in = (a[0] < b[0]) == (side == 0) ? a : b;
        for (k = 0; k < 3; k++) {
            pc_grid_point[k] = m->resurrect.save_pos[k];
            pc_grid_uvs[0][k] = m->resurrect.uvs[0][k];
            pc_grid_uvs[1][k] = m->resurrect.uvs[1][k];
            pc_grid_uvs[2][k] = m->resurrect.uvs[2][k];
            pc_grid_offset[k] = in[k];
        }
        pc_grid_offset[0] += side == 0 ? -gap : gap;
        return;
    }
    row = pc_pole_row(PC_GRID_PLACES + c);
    s = gCheckpoints.points[m->last_cp_index].pathPts[0];
    loop = gCheckpoints.points[gCheckpoints.loopPt].pathPts[0];
    need = -row[2];
    done = 0.0f;
    j = q = s;
    for (k = 0; k < 3; k++)
        pc_grid_point[k] = gResPath.pts[s][k];
    while (done < need) {
        q = j == loop ? gResPath.numPts - 1 : j - 1;
        if (q < 0 || q == s) {   /* no point before it, or once round the loop */
            q = j;
            break;
        }
        for (k = 0; k < 3; k++)
            d[k] = gResPath.pts[q][k] - gResPath.pts[j][k];
        seg = __builtin_sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (done + seg >= need && seg > 0.0f) {
            t = (need - done) / seg;
            for (k = 0; k < 3; k++)
                pc_grid_point[k] = gResPath.pts[j][k] + t * d[k];
            done = need;
            break;
        }
        done += seg;
        j = q;
        for (k = 0; k < 3; k++)
            pc_grid_point[k] = gResPath.pts[j][k];
    }
    FindSurfaceUvsFromResPoint(1, -1, q, pc_grid_uvs);   /* the path's segment from q on, the one the point is on */
    pc_grid_offset[0] = row[0];
    pc_grid_offset[1] = row[1];
    pc_grid_offset[2] = done - need;   /* 0 unless the path ran out: the rest straight back */
}

/* Is the road under the candidate in pc_grid_*? */
static int pc_grid_on_road(void)
{
    float v[3], at[3], snap[3], uvs[3][3];
    int k;
    WorldVector(pc_grid_offset, v, pc_grid_uvs);
    for (k = 0; k < 3; k++)
        at[k] = pc_grid_point[k] + v[k];
    return RoadPosition(at, snap, uvs) != 0;
}

/* Work out model m's grid start into pc_grid_* if it is a place past the N64's grid in a mode that uses the grid
 * (not a respawn, the attract mode, a battle, stunt or obstacle race): the nth candidate with road under it for
 * place 6 + n. Returns 0 when the table's start applies instead. */
static int pc_grid_pose(tModelData *m)
{
    static tModelData *last;
    static float last_key[5];
    static float kept[15];   /* the point, the orientation, the offset */
    float key[5];
    int p = m->initin.polepos, n, c, k, same;
    if (p < PC_GRID_PLACES || p >= PC_RESPAWN_BASE || (gGameState & GS_DEMO) || gRace == kBattle ||
        gRace == kStunt || gRace == kObstacle)
        return 0;
    /* Resurrect asks seven times for one car: the answer is kept for its record, place and start point. */
    key[0] = p, key[1] = m->last_cp_index;
    key[2] = m->resurrect.save_pos[0], key[3] = m->resurrect.save_pos[1], key[4] = m->resurrect.save_pos[2];
    for (same = m == last, k = 0; k < 5; k++)
        same &= key[k] == last_key[k];
    if (!same) {
        n = p - PC_GRID_PLACES;
        for (c = 0; c < PC_GRID_CANDIDATES; c++) {
            pc_grid_candidate(m, c);
            if (pc_grid_on_road() && n-- == 0)
                break;
        }
        if (c == PC_GRID_CANDIDATES)   /* too few with road under them: its own row */
            pc_grid_candidate(m, p - PC_GRID_PLACES);
        for (k = 0; k < 3; k++)
            kept[k] = pc_grid_point[k], kept[12 + k] = pc_grid_offset[k];
        for (k = 0; k < 9; k++)
            kept[3 + k] = pc_grid_uvs[k / 3][k % 3];
        for (k = 0; k < 5; k++)
            last_key[k] = key[k];
        last = m;
    }
    for (k = 0; k < 3; k++)
        pc_grid_point[k] = kept[k], pc_grid_offset[k] = kept[12 + k];
    for (k = 0; k < 9; k++)
        pc_grid_uvs[k / 3][k % 3] = kept[3 + k];
    return 1;
}

/* Model m's start: `what` 0 the point, 1 the orientation (3x3), 2 the grid offset; the table's (the start point
 * and pc_pole_row) unless pc_grid_pose places it past the N64's grid. */
float *pc_grid_start(tModelData *m, int what)
{
    if (!pc_grid_pose(m))
        return what == 0 ? m->resurrect.save_pos : what == 1 ? m->resurrect.uvs[0] : pc_pole_row(m->initin.polepos);
    return what == 0 ? pc_grid_point : what == 1 ? pc_grid_uvs[0] : pc_grid_offset;
}
#define PC_START_POS(m) pc_grid_start(m, 0)
#define PC_START_UVS(m) ((float (*)[3]) pc_grid_start(m, 1))
#define PC_POLE_ROW(p) pc_grid_start(m, 2)

/* Where human car `m` respawns: into *outPath the resurrection path's branch (-1 the main path) and *outIdx the
 * point on it (MAT3 FindHumanResPt).
 *   In a battle: after InitBattlePlayer, the point of the main path furthest (on the ground, x and z) from every
 *   other player (a respawning one counts at its respawn point), searched from a random point on.
 *   In an obstacle race: the course's high-water mark (obResPt, GetResPt).
 *   Else the point nearest where the car crashed; outside practice and stunts, if that is more than 40 feet away
 *   (1,600 squared) the nearest on any branch. A branch you can't respawn on (resurrect 0) hands over to the
 *   point FindResPt picks; a branch whose respawns stay at its point (resurrect 2) is kept as it is. Otherwise the
 *   point is kept between the last checkpoint passed and the next one: on a branch through both, the nearest
 *   point between them; on a branch that begins after the last one, not before its start; a branch neither
 *   checkpoint is on (and that doesn't start at the last one) is left for the main path; on the main path, the
 *   nearest point from the last checkpoint's to the next one's (or the path's end on the last lap section). */
void FindHumanResPt(tModelData *m, int *outPath, int *outIdx)
{
    int a;
    int p;
    int k;
    int end;
    float new_dist, scale, mag1, mag2, dist1, dist2;   /* unused */
    float pos[3], delta1[3];                           /* unused */
    int path = -1;
    int idx = 0;
    float min = 1.0e20f;
    float x, y, z;
    float dx, dy, dz, dist;
    if (gRace == kBattle) {
        float min2;
        int bestfit;
        int pOffset;
        int pReal;
        InitBattlePlayer(m->slot);
        min2 = 0.0f;
        bestfit = 0;
        pOffset = Random(gResPath.numPts);
        for (p = 0; p < gResPath.numPts; p++) {
            min = 1.0e20f;
            pReal = pOffset + p;
            if (pReal >= gResPath.numPts) pReal -= gResPath.numPts;
            for (a = 0; a < gNumPlayers; a++) {
                if (m->slot == a) continue;
                if (PC_MODEL_MOVING_STATE(a) >= 0) {
                    x = PC_MODEL_RESURRECT_POS(a)[0];
                    z = PC_MODEL_RESURRECT_POS(a)[2];
                } else {
                    x = PC_MODEL_RWR(a)[0];
                    z = PC_MODEL_RWR(a)[2];
                }
                dx = gResPath.pts[pReal][0] - x;
                dz = gResPath.pts[pReal][2] - z;
                dist = dx * dx + dz * dz;
                if (dist < min) min = dist;
            }
            if (min > min2) {
                bestfit = pReal;
                min2 = min;
            }
        }
        idx = bestfit;
    } else if (gRace == kObstacle) {
        idx = m->obResPt;
    } else {
        x = m->resurrect.save_pos[0];
        y = m->resurrect.save_pos[1];
        z = m->resurrect.save_pos[2];
        for (p = 0; p < gResPath.numPts; p++) {
            dx = gResPath.pts[p][0] - x;
            dy = gResPath.pts[p][1] - y;
            dz = gResPath.pts[p][2] - z;
            dist = dx * dx + dy * dy + dz * dz;
            if (dist < min) { min = dist; idx = p; }
        }
        if (gRace != kPractice && gRace != kStunt && gRace != kObstacle) {
            if (min > 1600.0f) {
                for (a = 0; a < gResPath.numAlt; a++) {
                    for (p = 0; p < gResPath.alt[a].numPts; p++) {
                        dx = gResPath.alt[a].pts[p][0] - x;
                        dy = gResPath.alt[a].pts[p][1] - y;
                        dz = gResPath.alt[a].pts[p][2] - z;
                        dist = dx * dx + dy * dy + dz * dz;
                        if (dist < min) {
                            min = dist;
                            idx = p;
                            path = a;
                        }
                    }
                }
            }
            if (path >= 0 && gResPath.alt[path].resurrect == 0)
                FindResPt(path, idx, &path, &idx, 1);
            if (!(path >= 0 && gResPath.alt[path].resurrect == 2)) {
                if (path >= 0) {
                    k = path + 5;
                    if (gCheckpoints.points[m->last_cp_index].pathPts[k] >= 0) {
                        if (gCheckpoints.points[m->next_cp_index].pathPts[k] >= 0) {
                            if (idx < gCheckpoints.points[m->last_cp_index].pathPts[k] ||
                                idx > gCheckpoints.points[m->next_cp_index].pathPts[k]) {
                                min = 1.0e20f;
                                for (p = gCheckpoints.points[m->last_cp_index].pathPts[k];
                                     p <= gCheckpoints.points[m->next_cp_index].pathPts[k]; p++) {
                                    dx = gResPath.alt[path].pts[p][0] - x;
                                    dy = gResPath.alt[path].pts[p][1] - y;
                                    dz = gResPath.alt[path].pts[p][2] - z;
                                    dist = dx * dx + dy * dy + dz * dz;
                                    if (dist < min) { min = dist; idx = p; }
                                }
                            }
                        } else if (idx < gCheckpoints.points[m->last_cp_index].pathPts[k]) {
                            idx = gCheckpoints.points[m->last_cp_index].pathPts[k];
                        }
                    } else if (gCheckpoints.points[m->next_cp_index].pathPts[k] < 0 &&
                               m->last_cp_index != gResPath.alt[path].startSection) {
                        path = -1;
                        idx = -1;
                    }
                }
                if (path < 0) {
                    end = m->next_cp_index < m->last_cp_index ? gResPath.numPts :
                        gCheckpoints.points[m->next_cp_index].pathPts[0];
                    if (idx < gCheckpoints.points[m->last_cp_index].pathPts[0] || idx >= end) {
                        min = 1.0e20f;
                        for (p = gCheckpoints.points[m->last_cp_index].pathPts[0]; p < end; p++) {
                            dx = gResPath.pts[p][0] - x;
                            dy = gResPath.pts[p][1] - y;
                            dz = gResPath.pts[p][2] - z;
                            dist = dx * dx + dy * dy + dz * dz;
                            if (dist < min) { min = dist; idx = p; }
                        }
                    }
                }
            }
        }
    }
    *outPath = path;
    *outIdx = idx;
}

/* Start model m's respawn, or at the race start place it (MAT3 Resurrect).
 *
 * In death mode a car that has crashed (crash time set) respawns only if the game aborted it. The respawn-in-place
 * option, for a human not out of water, at most once every two seconds and never in death mode, respawns the car
 * where it is (MAT3's FindSurfaceUvsInPlace, written inline here: the road's orientation under it, turned to the
 * car's heading, ten feet up) unless the road there is a half pipe.
 *
 * At the race start (crash time 0) the car starts at its start point and grid row (pc_grid_start; no row in battle,
 * stunt and obstacle races or for a drone respawning), not respawning; in the attract mode it rolls off at half its
 * drone path's speed there. Its simulation starts at once (modelrun).
 *
 * Otherwise the car stops being solid (it blinks: BlinkResurrectingCars) and is hidden for the flight. Its respawn
 * point: a human's from FindHumanResPt (kept as it is on a branch whose respawns stay at their point), a drone's the
 * nearest point of its own drone path between its checkpoints (MAT3's FindDroneResPt, written inline). The flight
 * takes resurrectTime (0.01 seconds in single, practice, circuit, ghost and obstacle races, else 1.5). The car
 * respawns at the speed of the path there (none in practice, battle, stunt and obstacle races, or with
 * respawn-in-place on; 100 mph at most), and the point moves on by the distance at that speed over the time since
 * the crash plus the flight (10% more for a car that was solid, 20% less for one that had just respawned; about
 * 20 feet a path point), on the last lap at most half way to the finish. Passing a checkpoint on the way counts
 * (PassedCP). In racing modes the car then arrives at 58 feet a second (about 40 mph). The point is three feet
 * above the path, facing along it; a human's is moved by its slot's respawn row of the grid table (row 6 in a
 * ghost race) outside battle, stunt and obstacle races and respawn-in-place.
 *
 * Finally: the flight's start and end orientations as quaternions (the end's sign chosen for the short way round,
 * FindBestQuat), moving_state RES_START, the simulation reset at the car's current place (keeping its velocity), the
 * suspension, body forces and road sounds cleared, and the car marked restarted. */
void Resurrect(tModelData *m)
{
    int inPlace;
    float *dp;                  /* unused */
    s16 node = m->slot;
    int i;
    float new_dist, scale, mag1, mag2, dist1, dist2;           /* unused */
    int startPath, endPath, startIdx, endIdx;
    int jumpPts, numPts;
    float pos[3], delta1[3], track_width, angle;               /* unused */
    float time_scale, crash_time;
    float vel[3];
    tCarData *gc = PC_CAR(node);
    int human;
    float c_dist, dist_e, dist_d, dist_bc, dist_b1, dist_b2, half_dist;   /* unused */
    if (gDeathMode && m->resurrect.crash_time != 0.0f && !m->resurrect.autoAbort) return;
    m->resurrect.autoAbort = 0;
    human = PC_LINK_OWNER(node) == LINK_OWNER_HUMAN;
    if (human && m->fastin.modelTime < res_last_time[m->slot]) res_last_time[m->slot] = 0;
    inPlace = 0;
    if (human && !m->resurrect.waterAbort && gResurrectInPlace &&
        2.0f + res_last_time[m->slot] < m->fastin.modelTime && !gDeathMode) {
        inPlace = 1;
        res_last_time[m->slot] = m->fastin.modelTime;
    }
    m->resurrect.waterAbort = 0;
    if (m->resurrect.crash_time == 0.0f) {
        m->initin.pos[0] = PC_START_POS(m)[0];
        m->initin.pos[1] = PC_START_POS(m)[1];
        m->initin.pos[2] = PC_START_POS(m)[2];
        CopyUV(PC_START_UVS(m), m->initin.uvs);
        if (gRace == kBattle || gRace == kStunt || gRace == kObstacle || (!human && m->initin.polepos >= PC_RESPAWN_BASE)) {
            m->initin.offset[0] = 0.0f;
            m->initin.offset[1] = 0.0f;
            m->initin.offset[2] = 0.0f;
        } else {
            m->initin.offset[0] = PC_POLE_ROW(m->initin.polepos)[0];
            m->initin.offset[1] = PC_POLE_ROW(m->initin.polepos)[1];
            m->initin.offset[2] = PC_POLE_ROW(m->initin.polepos)[2];
        }
        m->resurrect.moving_state = RES_IDLE;
        syminit(m);
        if (gGameState & GS_DEMO) {
            m->resurrect.velocity = (MPH_TO_FTPS * gDronePaths[gc->mpctl.path].pts[gCheckpoints.points[m->last_cp_index].pathPts[gc->mpctl.path + 1]].mph) / 2;
            m->v[2] = m->resurrect.velocity;
            for (i = 0; i < 4; i++) {
                m->tireV[i][2] = m->resurrect.velocity;
                m->tires[i].angvel = m->tireV[i][2] / m->tires[i].tradius;
            }
        }
        PC_MODEL_MODELRUN(node) = 1;
        return;
    }
    if (inPlace) {
        m->resurrect.pos[0] = m->rwr[0];
        m->resurrect.pos[1] = m->rwr[1];
        m->resurrect.pos[2] = m->rwr[2];
        m->resurrect.velocity = 0;
        {
            /* FindSurfaceUvsInPlace(m->resurrect.pos, m->reckon.uvs, m->resurrect.uvs) */
            float sine, cosine, inYaw, outYaw;
            float snapPos[3];
            surftyp *surf;
            int res;
            int ipos, temp;     /* unused */
            ExtractYaw(m->reckon.uvs, &inYaw);
            inYaw = -inYaw;
            surf = RoadPosition(m->resurrect.pos, snapPos, m->resurrect.uvs);
            if (surf) {
                if (surf->flags & 0x2000) {   /* a half pipe: no respawn in place */
                    res = 1;
                    goto done;
                }
                ExtractYaw(m->resurrect.uvs, &outYaw);
                sine = sinf(inYaw + outYaw);
                cosine = cosf(inYaw + outYaw);
            } else {
                sine = sinf(inYaw);
                cosine = cosf(inYaw);
                CopyUV(unitmatrix, m->resurrect.uvs);
            }
            fyaw(-sine, cosine, m->resurrect.uvs);
            res = 0;
done:
            if (!res) {
                m->resurrect.pos[1] += 10.0f;
                m->appearance &= ~APPEAR_HULK;
                m->crashflag = 0;
                goto finish;
            }
        }
    }
    time_scale = gc->solid ? 1.1f : 0.8f;
    gc->collidable = m->collidable = 0;
    gc->solid = m->solid = 0;
    gc->collide_time = m->fastin.modelTime;
    gc->collide_state = -1;
    gc->collide_count = -1;
    m->hide_car = 1;
    if (human) {
        FindHumanResPt(m, &startPath, &startIdx);
        if (startPath >= 0 && gResPath.alt[startPath].resurrect == 2) {
            m->resurrect.pos[0] = gResPath.alt[startPath].pts[startIdx][0];
            m->resurrect.pos[1] = 3.0f + gResPath.alt[startPath].pts[startIdx][1];
            m->resurrect.pos[2] = gResPath.alt[startPath].pts[startIdx][2];
            m->resurrect.velocity = 0;
            FindSurfaceUvsFromResPoint(1, startPath, startIdx, m->resurrect.uvs);
            m->appearance &= ~APPEAR_HULK;
            m->crashflag = 0;
            goto finish;
        }
    } else {
        /* FindDroneResPt (written inline) */
        tCarData *car = PC_CAR(m->slot);
        int p, idx, end;
        float min, x, y, z, dx, dy, dz, dist;
        tDronePath *path;
        path = &gDronePaths[car->mpctl.path];
        min = 1.0e20f;
        x = m->resurrect.save_pos[0];
        y = m->resurrect.save_pos[1];
        z = m->resurrect.save_pos[2];
        idx = 0;
        end = m->next_cp_index < m->last_cp_index ? path->numPts : gCheckpoints.points[m->next_cp_index].pathPts[car->mpctl.path + 1];
        for (p = gCheckpoints.points[m->last_cp_index].pathPts[car->mpctl.path + 1]; p < end; p++) {
            dx = path->pts[p].pos[0] - x;
            dy = path->pts[p].pos[1] - y;
            dz = path->pts[p].pos[2] - z;
            dist = dx * dx + dy * dy + dz * dz;
            if (dist < min) { min = dist; idx = p; }
        }
        startPath = car->mpctl.path;
        startIdx = idx;
        SetNewPathIndex(m, idx);
        dp = 0;
    }
    m->resurrect.resurrect_time = m->fastin.modelTime;
    if (gRace == kObstacle || gRace == kOneRace || gRace == kPractice || gRace == kCircuit || gRace == kGhost)
        resurrectTime = 0.01f;
    else resurrectTime = 1.5f;
    crash_time = resurrectTime + (m->resurrect.resurrect_time - m->resurrect.crash_time);
    if (gRace == kPractice || gRace == kBattle || gRace == kStunt || gRace == kObstacle || gResurrectInPlace) {
        m->resurrect.velocity = 0;
    } else if (m->resurrect.velocity < 0) {
        m->resurrect.velocity = 1;
    } else if (human) {
        i = FindClosestPathPt(startPath >= 0 ? gResPath.alt[startPath].pts[startIdx] : gResPath.pts[startIdx], 0);
        m->resurrect.velocity = MPH_TO_FTPS * gDronePaths[0].pts[i].mph;
    } else {
        m->resurrect.velocity = MPH_TO_FTPS * gDronePaths[startPath].pts[startIdx].mph;
    }
    if (m->resurrect.velocity > 146.66667f) m->resurrect.velocity = 146.66667f;
    jumpPts = crash_time * (m->resurrect.velocity * time_scale) / 20.0f;
    if (m->next_cp_index == gCheckpoints.finishPt && m->laps == gNumLaps - 1 &&
        gRace != kPractice && gRace != kStunt && gRace != kBattle && gRace != kObstacle) {
        numPts = NumPtsToCheckpoint(human, startPath, startIdx, m->next_cp_index) / 2;
        if (jumpPts > numPts) jumpPts = numPts;
    }
    TracePathPtForward(human, startPath, startIdx, jumpPts, &endPath, &endIdx);
    if (endPath >= 0) {
        if (human) {
            m->resurrect.pos[0] = gResPath.alt[endPath].pts[endIdx][0];
            m->resurrect.pos[1] = 3.0f + gResPath.alt[endPath].pts[endIdx][1];
            m->resurrect.pos[2] = gResPath.alt[endPath].pts[endIdx][2];
            if (gRace != kPractice && gRace != kBattle && gRace != kStunt &&
                gCheckpoints.points[m->next_cp_index].pathPts[endPath + 5] >= 0 &&
                gCheckpoints.points[m->next_cp_index].pathPts[endPath + 5] <= endIdx && !CountdownTimeOut())
                PassedCP(m, m->fastin.modelTime);
        } else {
            m->resurrect.pos[0] = gDronePaths[endPath].pts[endIdx].pos[0];
            m->resurrect.pos[1] = 3.0f + gDronePaths[endPath].pts[endIdx].pos[1];
            m->resurrect.pos[2] = gDronePaths[endPath].pts[endIdx].pos[2];
            if (((m->next_cp_index < m->last_cp_index && gCheckpoints.points[m->last_cp_index].pathPts[endPath + 1] > endIdx) ||
                 (m->next_cp_index > m->last_cp_index && gCheckpoints.points[m->next_cp_index].pathPts[endPath + 1] <= endIdx)) &&
                !CountdownTimeOut()) PassedCP(m, m->fastin.modelTime);
        }
    } else {
        m->resurrect.pos[0] = gResPath.pts[endIdx][0];
        m->resurrect.pos[1] = 3.0f + gResPath.pts[endIdx][1];
        m->resurrect.pos[2] = gResPath.pts[endIdx][2];
        if (gRace != kPractice && gRace != kBattle && gRace != kStunt &&
            ((m->next_cp_index < m->last_cp_index && gCheckpoints.points[m->last_cp_index].pathPts[0] > endIdx) ||
             (m->next_cp_index > m->last_cp_index && gCheckpoints.points[m->next_cp_index].pathPts[0] <= endIdx)) &&
            !CountdownTimeOut()) PassedCP(m, m->fastin.modelTime);
    }
    if (gRace != kPractice && gRace != kBattle && gRace != kStunt && gRace != kObstacle)
        m->resurrect.velocity = 58;
    FindSurfaceUvsFromResPoint(human, endPath, endIdx, m->resurrect.uvs);
    if (human) {
        /* N64: humans respawn at the multiplayer grid offsets (pole_pos_offset rows 6-11) */
        if (gRace == kGhost) {
            m->initin.offset[0] = pole_pos_offset[6][0];
            m->initin.offset[1] = pole_pos_offset[6][1];
            m->initin.offset[2] = pole_pos_offset[6][2];
        } else {
            m->initin.offset[0] = pole_pos_offset[m->slot + 6][0];
            m->initin.offset[1] = pole_pos_offset[m->slot + 6][1];
            m->initin.offset[2] = pole_pos_offset[m->slot + 6][2];
        }
        if (gRace != kBattle && gRace != kStunt && gRace != kObstacle && !gResurrectInPlace) {
            WorldVector(m->initin.offset, vel, m->resurrect.uvs);
            m->resurrect.pos[0] = m->resurrect.pos[0] + vel[0];
            m->resurrect.pos[1] = m->resurrect.pos[1] + vel[1];
            m->resurrect.pos[2] = m->resurrect.pos[2] + vel[2];
        } else {
            m->initin.offset[0] = 0.0f;
            m->initin.offset[1] = 0.0f;
            m->initin.offset[2] = 0.0f;
        }
    }
finish:
    m->initin.offset[0] = 0.0f;
    m->initin.offset[1] = 0.0f;
    m->initin.offset[2] = 0.0f;
    MakeQuatFromUvs(m->uvs, m->resurrect.quat_start);
    MakeQuatFromUvs(m->resurrect.uvs, m->resurrect.quat_end);
    FindBestQuat(m->resurrect.quat_start, m->resurrect.quat_end);
    m->resurrect.moving_state = RES_START;
    CopyUV(m->reckon.uvs, m->initin.uvs);
    m->initin.pos[0] = m->reckon.rwr[0];
    m->initin.pos[1] = m->reckon.rwr[1];
    m->initin.pos[2] = m->reckon.rwr[2];
    vel[0] = m->rwv[0];
    vel[1] = m->rwv[1];
    vel[2] = m->rwv[2];
    syminit(m);
    for (i = 0; i < 4; i++) {
        m->suscomp[i] = 0.0f;
        m->bodyForce[i][0] = 0.0f;
        gc->sound_flags[i] = m->sound_flags[i] = 0;
    }
    m->topscrape = 0;
    m->mthumpflag = m->thumpflag = 0;
    m->rwv[0] = vel[0];
    m->rwv[1] = vel[1];
    m->rwv[2] = vel[2];
    gc->restart = 1;
}

/* Make quaternion q2 the one of q2 and -q2 (the same orientation) nearer q1, by the sum of the components'
 * differences, so that interpolating from q1 turns the short way round (MAT3 FindBestQuat, a static there). */
void FindBestQuat(float *q1, float *q2)
{
    s16 i;
    float dq1;
    float dq2;
    dq1 = dq2 = 0.0f;
    for (i = 0; i < 4; i++) {
        dq1 += fabsf(q1[i] - q2[i]);
        dq2 += fabsf(q1[i] + q2[i]);
    }
    if (dq2 < dq1) {
        for (i = 0; i < 4; i++) q2[i] = -q2[i];
    }
}

