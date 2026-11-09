/* GetForceAndPeak: the peak of a hit on a part of a car, for the bump sounds (MAT3 audio.c's _GetForceAndPeak). */
#include "game/game.h"

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view
f32 sqrtf(f32);

/* Follows the force vec on part index (0-3 the body's corners, 4 its centre) of the car in slot player
 * (gBump[player].point[index]), unless a settled hit waits there for UpdateBumps. While the part has a timer but no
 * peak, a zero force for half a second clears the timer and any force restarts it. Otherwise a force above
 * threshold that beats the peak becomes the peak (and restarts the timer), and a tenth of a second after it the hit
 * is settled (bump). */
void GetForceAndPeak(s16 index, s32 player, f32 vec[3], f32 threshold)
{
    f32 force;
    if (gBump[player].point[index].bump == 0) {
        if (gBump[player].point[index].time && gBump[player].point[index].peak == 0) {
            if (vec[0] == 0 && vec[1] == 0 && vec[2] == 0) {
                if (gSeconds - gBump[player].point[index].time > 0.5f)
                    gBump[player].point[index].time = 0;
            } else
                gBump[player].point[index].time = gSeconds;
        } else {
            if (vec[0] != 0 || vec[1] != 0 || vec[2] != 0) {
                force = sqrtf(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
                if (threshold < force) {
                    if (force > gBump[player].point[index].peak) {
                        gBump[player].point[index].peak = force;
                        gBump[player].point[index].peakVec[0] = vec[0];
                        gBump[player].point[index].peakVec[1] = vec[1];
                        gBump[player].point[index].peakVec[2] = vec[2];
                        gBump[player].point[index].time = gSeconds;
                    }
                }
            }
            if (gBump[player].point[index].time && gSeconds - gBump[player].point[index].time > 0.1f) {
                gBump[player].point[index].bump = 1;
                gBump[player].point[index].time = gSeconds;
            }
        }
    }
}
