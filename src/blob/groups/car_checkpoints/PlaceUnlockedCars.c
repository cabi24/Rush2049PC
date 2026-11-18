/* PlaceUnlockedCars: the places of the cars still racing, by the distance they have driven (MAT3 checkpoint.c). */
#include "game/game.h"

/* PC: the cars still racing, by slot, and their distances by slot, as many as the race's cars (up to
 * PC_MAX_LINKS, pc_game_types.h, docs/port/MORECARS.md 2.5); the N64's 6. */
#define CARS_N PC_MAX_LINKS
/* Places the cars that haven't finished, furthest first (their distance along the track), after
 * the cars that have finished and before those out of the race (state positive before the last lap), which
 * CarReportsGameOver placed at the end. A selection sort; the swap is by exclusive or. */
void PlaceUnlockedCars(void) {
    s16 i;
    s16 j;
    s16 num_left;
    s16 num_done;
    s32 high_index;
    s16 place[CARS_N];
    s32 dist_tab[CARS_N];
    s16 index;

    num_done = 0;
    num_left = 0;
    for (i = 0; i < gNumActiveCars; i++) {
        if (pc_cars[PC_MODEL_SLOT(i)].dead > 0 && PC_MODEL_LAP(i) < gNumLaps) {
            num_done++;
        } else if (pc_cars[PC_MODEL_SLOT(i)].place_locked != 1) {
            place[num_left++] = PC_MODEL_SLOT(i);
        }
    }
    if (num_left != 0) {
        for (i = 0; i < num_left; i++) {
            dist_tab[place[i]] = pc_cars[place[i]].distance;
        }
        for (i = 0; i < num_left; i++) {
            high_index = i;
            for (j = i + 1; j < num_left; j++) {
                index = place[j];
                if (dist_tab[index] > dist_tab[place[high_index]]) {
                    high_index = j;
                }
            }
            pc_cars[place[high_index]].place = i + gNumActiveCars - num_left - num_done;
            place[high_index] ^= place[i];
            place[i] ^= place[high_index];
            place[high_index] ^= place[i];
        }
    }
}

