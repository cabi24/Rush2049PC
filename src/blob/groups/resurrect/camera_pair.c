/* camera_pair.c: where a car respawns from, and how it faces there (MAT3 resurrect.c, FindSurfaceUvsFromResPoint()
 * and SetResurrectionData()).
 *
 * (The file's name is an old guess: it holds no camera code.) A car's respawn point is kept in its model record
 * (tModelData.resurrect): at the race start the grid's point, after a crash the crash point; Resurrect later
 * works out the actual respawn from it. A point on one of the paths gets an orientation from the road under it,
 * turned to face along the path. */
#include "game/game.h"

typedef s16 Position[3];
typedef s16 SearchPoint[4];     /* a drone path point: pos[3], then mph and hints in the last halfword */

/* PC: gResPath and gDronePaths are native structs and the resurrection path's branches a native array
 * (ReadPathData), so these views have host offsets and strides. */

/* A branch of the resurrection path (MAT3 anon_resurrect_firstPath_10). */
typedef struct tResPathAlt {
    u8 resurrect;               /* 0x0 2: a respawn here stays at the branch's first point */
    s8 firstPath;               /* 0x1 */
    u16 firstIdx;               /* 0x2 */
    s8 lastPath;                /* 0x4 the branch it ends in, -1 the main path */
    u8 gap5;
    u16 lastIdx;                /* 0x6 and the point it ends at there */
    u16 startSection;           /* 0x8 */
    u16 numPts;                 /* 0xA */
    Position *pts;
} tResPathAlt;
/* The resurrection path (MAT3 gResPath, anon_numPts_pts_10): the main path's points and its branches. */
typedef struct tResPath {
    u16 numPts;
    u16 gap2;
    Position *pts;
    u8 numAlt;
    u8 gap9[3];
    tResPathAlt *alt;
} tResPath;
/* A drone path (MAT3 gDronePaths[4], anon_numPts_pts_8). */
typedef struct tDronePath {
    u16 numPts, gap;
    SearchPoint *pts;
} tDronePath;

/* The drone's path state in the car record (MAT3 tPathPoint, tCarData.mpctl; struct pc_car_nav), the fields used
 * here: the point on the drone path and the path. */
typedef struct tPathPoint { u8 pad[0x24]; s16 idx; s16 newIdx; s16 path; } tPathPoint;
/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the field used here. */
typedef struct tCarData { u8 pad[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad_308[0x314 - 0x308]; tPathPoint mpctl; u8 rest[0x380 - 0x33E]; void *input; u8 rest_384[0x3B8 - 0x384]; } tCarData;

VIEW(tDronePath[], gDronePaths);
#define gDronePaths gDronePaths_view
/* Drone path `p`, through a pointer to the array's first element: the shape the code compiled from */
#define DRONE_PATH(p) (((tDronePath *)gDronePaths)[p])

extern f32 sqrtf(f32), fabsf(f32), sinf(f32), cosf(f32);

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    u8 pad0[0x66C];
    f32 resurrect_save_pos[3];  /* 0x66C resurrect.save_pos: the point to respawn from */
    f32 resurrect_uvs[9];       /* 0x678 resurrect.uvs: and the orientation there */
    u8 pad69C[0x6C0 - 0x69C];
    f32 resurrect_crash_time;   /* 0x6C0 resurrect.crash_time: the model time of the crash, 0 at the start */
    u8 pad6C4[0x714 - 0x6C4];
    f32 modelTime;              /* 0x714 fastin.modelTime: the model thread's race clock, seconds */
    u8 pad718[0x794 - 0x718];
    f32 reckon_rwr[3];          /* 0x794 reckon.rwr: the car's position */
    u8 pad7A0[0x7C6 - 0x7A0];
    s16 slot;                   /* 0x7C6 */
    u8 pad7C8[0x7E2 - 0x7C8];
    s16 last_cp_index;          /* 0x7E2 the last checkpoint passed */
    s16 next_cp_index;          /* 0x7E4 and the next one */
    u8 tail[0x808 - 0x7E6];
} tModelData;
VIEW(tResPath, gResPath);
#define gResPath gResPath_view

VIEW(f32[9], unitmatrix);
#define unitmatrix unitmatrix_view

/* The checkpoints (tCheckpoints), through a view of 80-byte records: the shape the code compiled from */
typedef struct Record80 { u8 bytes[80]; } Record80;
VIEW(Record80[], gCheckpoints);
#define gCheckpoints gCheckpoints_view
#define CHECKPOINTS (*(tCheckpoints *)gCheckpoints)
/* SetResurrectionData's reads of the same fields, in the shape its code compiled from: the list's numPoints and
 * loopPt, and checkpoint k's pos, normal and pathPts[0] as offsets in the k-th 80-byte record (the list's 12-byte
 * header before the first checkpoint makes record k + 12 checkpoint k's start). */
#define CP_NUM_POINTS (*(s16 *)((u8 *)gCheckpoints + 8))
#define CP_LOOP_PT (*(s16 *)((u8 *)gCheckpoints + 2))
#define CP_PATHPT0(k) (*(s16 *)((u8 *)&gCheckpoints[k] + 46))
#define CP_POS(k, n) (*(f32 *)((u8 *)&gCheckpoints[k] + 12 + 4 * (n)))
#define CP_NORMAL(k, n) (*(f32 *)((u8 *)&gCheckpoints[k] + 24 + 4 * (n)))

typedef struct Surface Surface;

/* The orientation `uvs` (3x3, by rows) for a car placed at point `idx` of a path: `res` 1 the resurrection path
 * (the main path if `path` is negative, else branch `path`), 0 drone path `path`. It faces the next point (from a
 * path's last point: the point the branch ends at, or the lap's first point at the loop checkpoint). With a road
 * under the point (RoadPosition) the road's orientation is turned to that heading: the heading's angle
 * (asin of its x, from pi for a heading backwards in z) added to the road's yaw. With no road it pitches along
 * the path's slope, then yaws to the heading. */
void FindSurfaceUvsFromResPoint(s32 res, s32 path, s32 idx, f32 *uvs)
{
    float sine, cosine, yaw, angle;
    float snapPos[3], pt[3], pos[3];
    Surface *surf;
    if (path >= 0) {
        if (res) {
            pos[0] = gResPath.alt[path].pts[idx][0];
            pos[1] = gResPath.alt[path].pts[idx][1];
            pos[2] = gResPath.alt[path].pts[idx][2];
            if (idx == gResPath.alt[path].numPts - 1) {
                if (gResPath.alt[path].lastPath >= 0) {
                    pt[0] = gResPath.alt[gResPath.alt[path].lastPath].pts[gResPath.alt[path].lastIdx][0] - pos[0];
                    pt[1] = gResPath.alt[gResPath.alt[path].lastPath].pts[gResPath.alt[path].lastIdx][1] - pos[1];
                    pt[2] = gResPath.alt[gResPath.alt[path].lastPath].pts[gResPath.alt[path].lastIdx][2] - pos[2];
                } else {
                    pt[0] = gResPath.pts[gResPath.alt[path].lastIdx][0] - pos[0];
                    pt[1] = gResPath.pts[gResPath.alt[path].lastIdx][1] - pos[1];
                    pt[2] = gResPath.pts[gResPath.alt[path].lastIdx][2] - pos[2];
                }
            } else {
                pt[0] = gResPath.alt[path].pts[idx+1][0] - pos[0];
                pt[1] = gResPath.alt[path].pts[idx+1][1] - pos[1];
                pt[2] = gResPath.alt[path].pts[idx+1][2] - pos[2];
            }
        } else {
            pos[0] = DRONE_PATH(path).pts[idx][0];
            pos[1] = DRONE_PATH(path).pts[idx][1];
            pos[2] = DRONE_PATH(path).pts[idx][2];
            if (idx == DRONE_PATH(path).numPts - 1) {
                pt[0] = DRONE_PATH(path).pts[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[path+1]][0] - DRONE_PATH(path).pts[idx][0];
                pt[1] = DRONE_PATH(path).pts[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[path+1]][1] - DRONE_PATH(path).pts[idx][1];
                pt[2] = DRONE_PATH(path).pts[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[path+1]][2] - DRONE_PATH(path).pts[idx][2];
            } else {
                pt[0] = DRONE_PATH(path).pts[idx+1][0] - DRONE_PATH(path).pts[idx][0];
                pt[1] = DRONE_PATH(path).pts[idx+1][1] - DRONE_PATH(path).pts[idx][1];
                pt[2] = DRONE_PATH(path).pts[idx+1][2] - DRONE_PATH(path).pts[idx][2];
            }
        }
    } else {
        pos[0] = gResPath.pts[idx][0];
        pos[1] = gResPath.pts[idx][1];
        pos[2] = gResPath.pts[idx][2];
        if (idx == gResPath.numPts - 1) {
            pt[0] = gResPath.pts[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[0]][0] - gResPath.pts[idx][0];
            pt[1] = gResPath.pts[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[0]][1] - gResPath.pts[idx][1];
            pt[2] = gResPath.pts[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[0]][2] - gResPath.pts[idx][2];
        } else {
            pt[0] = gResPath.pts[idx+1][0] - gResPath.pts[idx][0];
            pt[1] = gResPath.pts[idx+1][1] - gResPath.pts[idx][1];
            pt[2] = gResPath.pts[idx+1][2] - gResPath.pts[idx][2];
        }
    }
    normalize_vector_or_x(pt);
    surf = RoadPosition(pos, snapPos, uvs);
    if (surf) {
        angle = fasin(pt[0]);
        if (pt[2] < 0.0f) angle = 3.1415927410125732f - angle;   /* pi */
        ExtractYaw(uvs, &yaw);
        sine = sinf(angle + yaw);
        cosine = cosf(angle + yaw);
    } else {
        sine = pt[1];
        cosine = sqrtf(pt[0]*pt[0] + pt[2]*pt[2]);
        if (pt[2] < 0.0f) sine = -sine;
        uvs[0] = 1.0f;
        uvs[5] = -sine;
        uvs[1] = 0.0f;
        uvs[2] = 0.0f;
        uvs[3] = 0.0f;
        uvs[4] = cosine;
        uvs[6] = 0.0f;
        uvs[7] = sine;
        uvs[8] = cosine;
        sine = pt[0];
        cosine = pt[2];
    }
    fyaw(-sine, cosine, uvs);
}

/* Keep model `m`'s respawn point. `mode` INIT_MODE_START, the race start: in the attract mode the demo's start
 * point on the resurrection path (gDemoPathPt) with its orientation, the checkpoints around it, and the drone path
 * point the drone code picks for it (SetDroneResPathIndex); in a battle or stunt race the arena's start for the
 * car's slot (checkpoints 0, 2, 1, 3 for slots 0-3, then 3): its position, facing against the checkpoint's
 * normal levelled out; elsewhere the last checkpoint's point on the resurrection path. The crash time becomes 0.
 * INIT_MODE_CRASH: the car's position and the model time. `abort` is unused (MAT3's too). */
void SetResurrectionData(s16 mode, tModelData *m, u32 abort) {
    s32 i, index;
    tCarData *car;
    f32 *dp;                                /* unused */
    s32 xv, yv, zv, last_cp_index;          /* unused */
    void *chkpnt;                           /* unused */
    switch (mode) {
    case INIT_MODE_START:
        if (gGameState & GS_DEMO) {
            FindSurfaceUvsFromResPoint(1, -1, gDemoPathPt, m->resurrect_uvs);
            for (i = CP_NUM_POINTS - 1; i >= 0; i--) {
                if (gDemoPathPt >= CP_PATHPT0(i)) {
                    m->last_cp_index = i;
                    if (m->last_cp_index == CP_NUM_POINTS - 1) index = CP_LOOP_PT;
                    else index = m->last_cp_index + 1;
                    m->next_cp_index = index;
                    break;
                }
            }
            SetDroneResPathIndex((struct State *)m, -1, gDemoPathPt);
            car = PC_CAR(m->slot);
            m->resurrect_save_pos[0] = (f32)DRONE_PATH(car->mpctl.path).pts[car->mpctl.idx][0];
            m->resurrect_save_pos[1] = (f32)DRONE_PATH(car->mpctl.path).pts[car->mpctl.idx][1];
            m->resurrect_save_pos[2] = (f32)DRONE_PATH(car->mpctl.path).pts[car->mpctl.idx][2];
        } else if (gRace == kBattle || gRace == kStunt) {
#define ARENA_START(slot) ((slot) == 0 ? 0 : ((slot) == 1 ? 2 : ((slot) == 2 ? 1 : 3)))
            m->resurrect_save_pos[0] = CP_POS(ARENA_START(m->slot), 0);
            m->resurrect_save_pos[1] = CP_POS(ARENA_START(m->slot), 1);
            m->resurrect_save_pos[2] = CP_POS(ARENA_START(m->slot), 2);
            CopyUV(unitmatrix, m->resurrect_uvs);
            m->resurrect_uvs[6] = CP_NORMAL(ARENA_START(m->slot), 0);
            m->resurrect_uvs[7] = CP_NORMAL(ARENA_START(m->slot), 1);
            m->resurrect_uvs[8] = CP_NORMAL(ARENA_START(m->slot), 2);
            m->resurrect_uvs[7] = 0.0f;
            m->resurrect_uvs[6] *= -1.0f;
            m->resurrect_uvs[8] *= -1.0f;
            NormalVector(&m->resurrect_uvs[6]);
            /* the first row: the cross product of the second and third */
            m->resurrect_uvs[0] = m->resurrect_uvs[4]*m->resurrect_uvs[8] - m->resurrect_uvs[5]*m->resurrect_uvs[7];
            m->resurrect_uvs[1] = m->resurrect_uvs[5]*m->resurrect_uvs[6] - m->resurrect_uvs[3]*m->resurrect_uvs[8];
            m->resurrect_uvs[2] = m->resurrect_uvs[3]*m->resurrect_uvs[7] - m->resurrect_uvs[4]*m->resurrect_uvs[6];
            NormalVector(m->resurrect_uvs);
            SyncMaxpathToLastCheckpoint(m->slot);
        } else {
            m->resurrect_save_pos[0] = (f32)gResPath.pts[CP_PATHPT0(m->last_cp_index)][0];
            m->resurrect_save_pos[1] = (f32)gResPath.pts[CP_PATHPT0(m->last_cp_index)][1];
            m->resurrect_save_pos[2] = (f32)gResPath.pts[CP_PATHPT0(m->last_cp_index)][2];
            FindSurfaceUvsFromResPoint(1, -1, CP_PATHPT0(m->last_cp_index), m->resurrect_uvs);
            SyncMaxpathToLastCheckpoint(m->slot);
        }
        m->resurrect_crash_time = 0.0f;
        break;
    case INIT_MODE_CRASH:
        m->resurrect_save_pos[0] = m->reckon_rwr[0];
        m->resurrect_save_pos[1] = m->reckon_rwr[1];
        m->resurrect_save_pos[2] = m->reckon_rwr[2];
        m->resurrect_crash_time = m->modelTime;
        break;
    }
}
