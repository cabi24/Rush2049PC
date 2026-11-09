/* SetCatchup.c: the cars' catch-up factors (MAT3 model.c, SetCatchup()).
 *
 * Called once a frame in a race: each racing car's catch-up factor (MAT3 tModelData.catchup, the model records'
 * `scale` here) follows how far it is behind the leader. The race roster is pc_links on the PC (pc_game_types.h,
 * docs/port/DECOUPLE.md). */
#include "game/game.h"

/* Without catch-up (one player below difficulty 5, or the option off: MAT3 no_catchup) every drone and human gets
 * the factor 1. Otherwise, among the cars still racing that count (humans, or every car at difficulty 5), the one
 * furthest along the track leads, and each of them aims for 1 plus 0.1 times its distance behind the leader over
 * 1500 ft (1.1 at most); with gUseCatchup 1 the aim is mirrored and halved (1 - 0.05 per 1500 ft). The factor moves
 * 0.03 of the way to its aim each frame. */
void SetCatchup(void)
{
    s16 i;
    s16 high_index;
    s32 j;
    f32 target_catchup;
    f32 lagdist;
    f32 firstdist;
    f32 k1;
    f32 k2;

    if ((gNumPlayers == 1 && gDifficulty < 5) || gUseCatchup == 0) {
        for (j = 0; j < PC_RACE_CARS; j++) {
            if (PC_LINK_OWNER(j) == 0 || PC_LINK_OWNER(j) == 6) {   /* LINK_OWNER_DRONE, LINK_OWNER_HUMAN */
                PC_MODEL_CATCHUP(j) = 1.0f;
            }
        }
    } else {
        high_index = -1;
        for (i = 0; i < PC_RACE_CARS; i++) {
            if (PC_MODEL_ACTIVE(i) != 0 && PC_CAR_DEAD(i) < CAR_DEAD &&
                (PC_MODEL_DRONE_TYPE(i) == DRONE_TYPE_HUMAN || gDifficulty == 5)) {
                if (high_index == -1) {
                    high_index = i;
                } else if (PC_CAR_DISTANCE(high_index) < PC_CAR_DISTANCE(i)) {
                    high_index = i;
                }
            }
        }
        k1 = 0.1f;       /* the most catch-up */
        k2 = 1500.0f;    /* the distance behind that earns it, feet */
        /* Original game: with no car counting, high_index stays -1 and this reads the record before the first (MAT3
         * too); nothing uses the value then. */
        firstdist = PC_CAR_DISTANCE(high_index);
        for (i = 0; i < PC_RACE_CARS; i++) {
            if (PC_MODEL_ACTIVE(i) != 0 && PC_CAR_DEAD(i) < CAR_DEAD &&
                (PC_MODEL_DRONE_TYPE(i) == DRONE_TYPE_HUMAN || gDifficulty == 5)) {
                lagdist = firstdist - PC_CAR_DISTANCE(i);
                if (lagdist > k2) {
                    target_catchup = k1 + 1.0f;
                } else {
                    target_catchup = lagdist * k1 / k2 + 1;
                }
                if (gUseCatchup == 1) {
                    target_catchup = (1 - target_catchup) * 0.5f + 1;
                }
                PC_MODEL_CATCHUP(i) = PC_MODEL_CATCHUP(i) * PC_FILTER(0.97f) + PC_FILTER_REST(0.97f, 0.03f) * target_catchup;
            }
        }
    }
}
