#include "game/game.h"

typedef struct {
    u8 pad0[4];
    f32 time;               /* +0x004 */
    f32 pos[3];             /* +0x008 */
    u8 pad14[0xEE - 0x14];
    s8 blocked;             /* +0x0EE */
    u8 padEF[0x108 - 0xEF];
    f32 distance;           /* +0x108 */
    u8 pad10C[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad10C_308[0x358 - 0x308];
    s8 crashed;             /* +0x358 */
    u8 pad359[0x380 - 0x359]; void *input; u8 pad359_384[0x3B8 - 0x384];
} CarData;                  /* 0x3B8 */

typedef struct Model {
    u8 pad0[0x6C4];
    s16 moving;             /* +0x6C4 */
    u8 pad6C6[0x7E4 - 0x6C6];
    s16 next_cp;            /* +0x7E4 */
    u8 pad7E6[3];
    s8 side;                /* +0x7E9 */
    u8 pad7EA[0x808 - 0x7EA];
} Model;                    /* 0x808 */

extern u8 gPlayers;

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

void CheckCPs(void)
{
    f32 cent_dist;
    s16 i;
    f32 diff[3];
    s16 index;
    s32 side;
    f32 plane;
    CarData *gc;
    f32 *prev;
    Model *m;
    f32 move[3];
    f32 zvec[3];

    for (index = 0; index < gNumCars; index++) {
        gc = PC_CAR(index);
        prev = PC_CP_LAST_POS(index);
        move[0] = gc->pos[0] - prev[0];
        move[1] = gc->pos[1] - prev[1];
        move[2] = gc->pos[2] - prev[2];
        gc->distance += vector_magnitude(move);
        prev[0] = gc->pos[0];
        prev[1] = gc->pos[1];
        prev[2] = gc->pos[2];
        if ((PC_LINK_OWNER(index) == 0 || PC_LINK_OWNER(index) == 6) && gc->crashed == 0) {
            m = PC_MODEL(index);
            if (m->moving == -1) {
                diff[0] = gc->pos[0] - gCheckpoints.points[m->next_cp].pos[0];
                diff[1] = gc->pos[1] - gCheckpoints.points[m->next_cp].pos[1];
                diff[2] = gc->pos[2] - gCheckpoints.points[m->next_cp].pos[2];
                plane = diff[0] * gCheckpoints.points[m->next_cp].normal[0] + diff[1] * gCheckpoints.points[m->next_cp].normal[1] + diff[2] * gCheckpoints.points[m->next_cp].normal[2];
                if (plane < 0.0f) {
                    side = -1;
                } else {
                    side = 1;
                }
                if (side != m->side) {
                    m->side = side;
                    if (diff[0] * diff[0] + diff[2] * diff[2] < gCheckpoints.points[m->next_cp].radius) {
                        cent_dist = plane / (plane - PC_CP_PLANE(index));
                        cent_dist = gFrameSeconds * cent_dist;
                        PassedCP(m, gc->time - cent_dist);
                    }
                }
                PC_CP_PLANE(index) = plane;
            }
        }
    }
    if (gRace == 1) {
        return;
    }
    FindMaxpathIntervals();
    PlaceUnlockedCars();
    if (gFirstPlaceTime != 0.0f && gSeconds - gFirstPlaceTime > 3.0f) {
        gFirstPlaceTime = 0.0f;
    }
    if (num_humans == 1 && PC_CAR_PLACE(gPlayers) == 0 && !(gGameState & 8)) {
        if (gInFirstPlace == 0 && gFirstPlaceTime == 0.0f) {
            gFirstPlaceTime = gSeconds;
            PlayGameSound(16, gPlayers, 1, 1);
        }
        gInFirstPlace = 1;
    } else {
        gInFirstPlace = 0;
    }
}
