#include "game/game.h"

VIEW(WeightRecord[12], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view
VIEW(intptr_t[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view

void RemovePlayerScores(intptr_t handle) {
    int outer, row, slot, index;
    f32 *weights;
    intptr_t *handles;

    for (outer = 0; outer < 12; outer++) {
        for (row = 0; row < 3; row++) {
            handles = gTotalNames[outer][row];
            weights = gTotalRaceStats[outer].weight[row];
            for (slot = 0; slot < 5; slot++) {
                if (handle == handles[slot]) {
                    for (index = slot; index < 4; index++) {
                        weights[index] = weights[index + 1];
                        handles[index] = handles[index + 1];
                    }
                    weights[index] = 0.0f;
                    handles[index] = -1;
                    slot--;
                }
            }
        }
    }
}
