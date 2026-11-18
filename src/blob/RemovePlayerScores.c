/* RemovePlayerScores: take a profile's times out of the high score tables (MAT3 gamerecords.c). */
#include "game/game.h"

/* Removes every entry of the high score tables (gTotalRaceStats' scores) owned by the profile, moving the entries
 * after it up; the last of the five becomes empty (time 0, owner -1). */
void RemovePlayerScores(tProfile **profile) {
    int track, list, slot, index;
    f32 *times;
    tScoreOwner *owners;

    for (track = 0; track < 12; track++) {
        for (list = 0; list < 3; list++) {
            owners = gTotalNames[track][list];
            times = gTotalRaceStats[track].scores[list];
            for (slot = 0; slot < 5; slot++) {
                if (profile == owners[slot].profile) {
                    for (index = slot; index < 4; index++) {
                        times[index] = times[index + 1];
                        owners[index] = owners[index + 1];
                    }
                    times[index] = 0.0f;
                    owners[index].name = -1;
                    slot--;
                }
            }
        }
    }
}
