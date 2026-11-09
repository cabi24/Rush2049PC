#include "game/game.h"

VIEW(s16, gNumCars);
#define gNumCars gNumCars_view

/* PC: the locked cars in score order, by slot, and their scores, as many as the race's cars (up to PC_MAX_LINKS,
 * pc_game_types.h, docs/port/MORECARS.md 2.5); the N64's are 7 and 6, the 7 for its frame. */
#define PLACE_N (PC_MAX_LINKS + 1)
#define SCORES_N PC_MAX_LINKS
void CarReportsGameOver(s32 slot, f32 score) {
    s16 i, j, index, temp, num_locked, place[PLACE_N], num_humans_locked;
    f32 scores[SCORES_N];
    f32 ftemp;

    PC_CAR_FINISHED(slot) = 1;
    PC_CAR_TIME(slot) = score;
    num_humans_locked = 0;
    for (i = 0, num_locked = -1; i < gNumActiveCars; i++) {
        index = PC_MODEL_SLOT(i);
        if (PC_CAR_FINISHED(index) == 1) {
            place[++num_locked] = index;
            scores[num_locked] = PC_CAR_TIME(index);
            if (PC_MODEL_DRONE_TYPE(index) == 2) {
                if (gRace != 2 || PC_MODELS == PC_MODEL(index)) {
                    num_humans_locked++;
                }
            }
            if (PC_CAR_STATE(index) > 0 && PC_MODEL_LAP(index) < gNumLaps) {
                scores[num_locked] = 5999.999f + (f32) (s32) (9.0f * gLapDistance) - PC_CAR_DISTANCE(index);
            }
        }
    }
    if (PC_MODEL_DRONE_TYPE(slot) == 2) {
        if ((num_humans_locked == gNumPlayers || (gRace == 2 && num_humans_locked == 1)) && gEndGame == 0) {
            gEndGame = 1;
            AddCountdownTime(1.0f - GetCountdownTimeRemaining());
        }
    }
    for (i = 0; i < num_locked; i++) {
        for (j = 0; j < num_locked - i; j++) {
            if (scores[j] > scores[j + 1]) {
                ftemp = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = ftemp;
                temp = place[j];
                place[j] = place[j + 1];
                place[j + 1] = temp;
            }
        }
    }
    for (i = 0; i < num_locked + 1; i++) {
        if (PC_CAR_STATE(place[i]) > 0 && PC_MODEL_LAP(place[i]) < gNumLaps) {
            PC_CAR_PLACE(place[i]) = gNumCars - num_locked + i - 1;
        } else {
            PC_CAR_PLACE(place[i]) = i;
        }
    }
    PlaceUnlockedCars();
}
