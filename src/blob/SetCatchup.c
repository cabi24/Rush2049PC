/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

#include "game/game.h"

/* PC: game.frame_rate 60 (pc_game_fps.h, docs/port/FRAMERATE.md 7). The catch-up scale moves 0.03 of the way to its
 * target a frame of two retraces, a filter the cars' physics reads: e^frames and its other weight, 1 - e^frames
 * (0.03, as written, at original). */
#define PC_FILTER_REST(e, rest) (pc_fps60 ? 1.0f - pc_fps_filter(e) : (rest))
void SetCatchup(void)
{
    s16 i;
    s16 best;
    s32 j;
    f32 s;
    f32 d;
    f32 bd;
    f32 k1;
    f32 k2;

    if ((gNumPlayers == 1 && gDifficulty < 5) || gUseCatchup == 0) {
        for (j = 0; j < PC_RACE_CARS; j++) {
            if (PC_LINK_OWNER(j) == 0 || PC_LINK_OWNER(j) == 6) {
                PC_MODEL_SCALE(j) = 1.0f;
            }
        }
    } else {
        best = -1;
        for (i = 0; i < PC_RACE_CARS; i++) {
            if (PC_MODEL_ACTIVE(i) != 0 && PC_CAR_STATE(i) < 2 &&
                (PC_MODEL_DRONE_TYPE(i) == 2 || gDifficulty == 5)) {
                if (best == -1) {
                    best = i;
                } else if (PC_CAR_DISTANCE(best) < PC_CAR_DISTANCE(i)) {
                    best = i;
                }
            }
        }
        k1 = 0.1f;
        k2 = 1500.0f;
        bd = PC_CAR_DISTANCE(best);
        for (i = 0; i < PC_RACE_CARS; i++) {
            if (PC_MODEL_ACTIVE(i) != 0 && PC_CAR_STATE(i) < 2 &&
                (PC_MODEL_DRONE_TYPE(i) == 2 || gDifficulty == 5)) {
                d = bd - PC_CAR_DISTANCE(i);
                if (d > k2) {
                    s = k1 + 1.0f;
                } else {
                    s = d * k1 / k2 + 1;
                }
                if (gUseCatchup == 1) {
                    s = (1 - s) * 0.5f + 1;
                }
                PC_MODEL_SCALE(i) = PC_MODEL_SCALE(i) * PC_FILTER(0.97f) + PC_FILTER_REST(0.97f, 0.03f) * s;
            }
        }
    }
}
