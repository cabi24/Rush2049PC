/* CarReportsGameOver: a car's race is over: its place among the cars that are done (MAT3 checkpoint.c). */
#include "game/game.h"

VIEW(s16, gNumCars);
#define gNumCars gNumCars_view

/* PC: the locked cars in score order, by slot, and their scores, as many as the race's cars (up to PC_MAX_LINKS,
 * pc_game_types.h, docs/port/MORECARS.md 2.5); the N64's are 7 and 6, the 7 for its frame. */
#define PLACE_N (PC_MAX_LINKS + 1)
#define SCORES_N PC_MAX_LINKS
/* The car in slot `slot` has finished (or is out of the race) with `score`, its race time. Locks its place and
 * ranks every car that is done by score; a car out of the race (its state positive) before the last lap ranks
 * after every finisher (by distance driven) and takes a place from the end. Once every human is done (in a ghost
 * race the first player) the race ends a second later (gEndGame, the countdown cut to 1 second). The cars still
 * racing are placed after (PlaceUnlockedCars). */
void CarReportsGameOver(s32 slot, f32 score) {
    s16 i, j, index, temp, num_locked, place[PLACE_N], num_humans_locked;
    f32 scores[SCORES_N];
    f32 ftemp;

    PC_CAR_PLACE_LOCKED(slot) = 1;
    PC_CAR_SCORE(slot) = score;
    num_humans_locked = 0;
    for (i = 0, num_locked = -1; i < gNumActiveCars; i++) {
        index = PC_MODEL_SLOT(i);
        if (PC_CAR_PLACE_LOCKED(index) == 1) {
            place[++num_locked] = index;
            scores[num_locked] = PC_CAR_SCORE(index);
            if (PC_MODEL_DRONE_TYPE(index) == 2) {
                if (gRace != kGhost || PC_MODELS == PC_MODEL(index)) {
                    num_humans_locked++;
                }
            }
            if (PC_CAR_DEAD(index) > 0 && PC_MODEL_LAP(index) < gNumLaps) {
                /* out of the race: after the slowest possible finish, the further it got the better */
                scores[num_locked] = 5999.999f + (f32) (s32) (9.0f * gLapDistance) - PC_CAR_DISTANCE(index);
            }
        }
    }
    if (PC_MODEL_DRONE_TYPE(slot) == 2) {
        if ((num_humans_locked == gNumPlayers || (gRace == kGhost && num_humans_locked == 1)) && gEndGame == 0) {
            gEndGame = 1;
            AddCountdownTime(1.0f - GetCountdownTimeRemaining());
        }
    }
    /* a bubble sort of the cars that are done, by score */
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
        if (PC_CAR_DEAD(place[i]) > 0 && PC_MODEL_LAP(place[i]) < gNumLaps) {
            PC_CAR_PLACE(place[i]) = gNumCars - num_locked + i - 1;
        } else {
            PC_CAR_PLACE(place[i]) = i;
        }
    }
    PlaceUnlockedCars();
}
