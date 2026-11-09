#include "game/game.h"

typedef struct V3s { s16 x, y, z; } V3s;
typedef struct { u16 count; u16 pad; V3s *pts; } Graph;
VIEW(Graph, gResPath);
#define gResPath gResPath_view

s16 find_closest_res_path_point(V3s *p) {
    float best = res_path_search_start_dist;
    s32 pad[5];
    s16 bi;
    s32 i;
    for (i = 0; i < gResPath.count; i++) {
        float dx = gResPath.pts[i].x - p->x;
        float dy = gResPath.pts[i].y - p->y;
        float dz = gResPath.pts[i].z - p->z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < best) {
            best = d;
            bi = i;
        }
    }
    return bi;
}
