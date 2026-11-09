#include "game/game.h"

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view
f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

void GetForceAndPeak(s16 index, s32 player, f32 vec[3], f32 threshold)
{
    f32 force;
    if (gBump[player].point[index].flag == 0) {
        if (gBump[player].point[index].timestamp && gBump[player].point[index].force == 0) {
            if (vec[0] == 0 && vec[1] == 0 && vec[2] == 0) {
                if (gSeconds - gBump[player].point[index].timestamp > 0.5f)
                    gBump[player].point[index].timestamp = 0;
            } else
                gBump[player].point[index].timestamp = gSeconds;
        } else {
            if (vec[0] != 0 || vec[1] != 0 || vec[2] != 0) {
                force = sqrtf(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
                if (threshold < force) {
                    if (force > gBump[player].point[index].force) {
                        gBump[player].point[index].force = force;
                        gBump[player].point[index].vector[0] = vec[0];
                        gBump[player].point[index].vector[1] = vec[1];
                        gBump[player].point[index].vector[2] = vec[2];
                        gBump[player].point[index].timestamp = gSeconds;
                    }
                }
            }
            if (gBump[player].point[index].timestamp && gSeconds - gBump[player].point[index].timestamp > 0.1f) {
                gBump[player].point[index].flag = 1;
                gBump[player].point[index].timestamp = gSeconds;
            }
        }
    }
}
