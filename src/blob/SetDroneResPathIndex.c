/* SetDroneResPathIndex: puts a respawned drone back on its drone path (MAT3 maxpath.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the fields this reads. */
struct tModelData {
    u8 pad[0x7C6];
    s16 slot;          /* 0x7C6 the car slot */
    u8 pad7C8[0x7E2 - 0x7C8];
    s16 last_cp_index; /* 0x7E2 the last checkpoint it passed */
    s16 next_cp_index; /* 0x7E4 the next one */
};

/* Sets the drone of model `m`, respawned at point `idx` of the resurrection path (alternative path `path`, or the
 * main one when `path` is negative), on the nearest point of its drone path within its checkpoint section (from
 * its last checkpoint to its next), then that many points further on as its slot number, so drones put back at the
 * same place don't sit on the same point. */
void SetDroneResPathIndex(struct tModelData *m, s16 path, s16 idx) {
    DronePath *drone_paths = gDronePaths;
    f32 x, y, z;
    f32 min;
    f32 dx, dy, dz, dist;
    s16 *resPt;
    DronePathPt *pt;
    struct pc_car_nav *ctl;
    s32 end;
    s16 next;
    s32 i;
    min = drone_res_path_start_distance;
    if (path >= 0) {
        resPt = gResPath.alt[path].pts[idx];
        x = (f32) resPt[0];
        y = (f32) resPt[1];
        z = (f32) resPt[2];
    } else {
        x = (f32) gResPath.pts[idx][0];
        y = (f32) gResPath.pts[idx][1];
        z = (f32) gResPath.pts[idx][2];
    }
    ctl = &PC_CAR_NAV(m->slot);
    if (m->next_cp_index < m->last_cp_index) end = gDronePaths[ctl->path].numPts;
    else end = gCheckpoints.points[m->next_cp_index].pathPts[ctl->path + 1];
    i = gCheckpoints.points[m->last_cp_index].pathPts[ctl->path + 1];
    idx = 0;
    if (i < end) {
        pt = &drone_paths[ctl->path].pts[i];
        do {
            dx = (f32) pt->pos[0] - x;
            dy = (f32) pt->pos[1] - y;
            dz = (f32) pt->pos[2] - z;
            dist = dx * dx + dy * dy + dz * dz;
            if (dist < min) {
                min = dist;
                idx = i;
            }
            i++;
            pt++;
        } while (i < end);
    }
    i = 0;
    while (i < m->slot) {
        next = NextPathPt(idx, ctl->path);
        i++;
        idx = next;
    }
    SetNewPathIndex(m, idx);
}
