#include "game/game.h"

typedef struct P3 {
    s16 x, y, z, pad;
} P3;

typedef struct {
    u16 n;
    P3 *pts;
} PSet;

VIEW(PSet[], gDronePaths);
#define gDronePaths gDronePaths_view

s16 FindClosestPathPt(P3 *pos, s16 set)
{
    P3 *p;
    s32 i;
    s32 pad0[4];
    s16 best;
    f32 dx, dy, dz, d;
    f32 min;

    min = 1e20f;
    i = 0;
    if (gDronePaths[set].n > 0) {
        p = gDronePaths[set].pts;
    loop:
        dx = p->x - pos->x;
        dy = p->y - pos->y;
        dz = p->z - pos->z;
        d = dx * dx + dy * dy + dz * dz;
        if (d < min) {
            min = d;
            best = i;
        }
        i++;
        p++;
        if (i < gDronePaths[set].n) {
            goto loop;
        }
    }
    return best;
}
