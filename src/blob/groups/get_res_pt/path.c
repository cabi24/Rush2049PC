#include "game/game.h"

typedef struct Point {
    s16 pos[3];
} Point;
typedef struct Route {
    u16 count;
    Point *points;
} Route;
typedef struct Car {
    u8 opaque0[8];
    f32 pos[3];
    u8 opaque14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 opaque14_308[0x380 - 0x308]; void *input; u8 opaque14_384[0x3B8 - 0x384];
} Car; /* 0x3B8 */
VIEW(Route, gResPath);
#define gResPath gResPath_view
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

void GetResPt(void) {
    s32 i;
    s32 close_index;
    f32 x, y, z;
    f32 dx, dy, dz;
    f32 dist;
    f32 c_dist;
    f32 delta1[3];
    f32 delta2[3];
    s16 *p;
    Car *car = PC_CARS;

    x = PC_CAR_POS(0)[0];
    y = PC_CAR_POS(0)[1];
    z = PC_CAR_POS(0)[2];
    c_dist = 1e20;
    for (i = 0; i < gResPath.count; i++) {
        dx = gResPath.points[i].pos[0] - x;
        dy = gResPath.points[i].pos[1] - y;
        dz = gResPath.points[i].pos[2] - z;
        dist = dx * dx + dy * dy + dz * dz;
        if (dist < c_dist) {
            c_dist = dist;
            close_index = i;
        }
    }
    if (close_index & 1) {
        close_index--;
        raise_ob_res_pt(close_index);
        return;
    }
    p = gResPath.points[close_index].pos;
    delta1[0] = (f32) p[3] - (f32) p[0];
    delta1[1] = (f32) p[4] - (f32) p[1];
    delta1[2] = (f32) p[5] - (f32) p[2];
    delta2[0] = car->pos[0] - p[0];
    delta2[1] = car->pos[1] - p[1];
    delta2[2] = car->pos[2] - p[2];
    dx = delta1[0] * delta2[0] + delta1[1] * delta2[1] + delta1[2] * delta2[2];
    if (dx < 0.0f && close_index > 0) {
        close_index -= 2;
    }
    raise_ob_res_pt(close_index);
}

void raise_ob_res_pt(s32 index) {
    if (PC_MODEL_OB_RES_PT(0) < index) {
        PC_MODEL_OB_RES_PT(0) = index;
    }
}
