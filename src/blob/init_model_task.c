/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

#include "game/game.h"

void init_model_task(s32 arg0)
{
    f32 q;
    s32 i;
    f32 scale;

    gModelIteration = -arg0;
    scale = gRetraceTime;
    gModelTime = gModelIteration * scale;
    i = 0;
loop:
    if (gRace != 2 || i == 0 || PC_MODEL_DT(i) == 0.0f) {
        PC_MODEL_ITERATION(i) = gModelIteration;
        PC_MODEL_MODEL_TIME(i) = gModelTime;
        PC_MODEL_DT(i) = scale;
    } else {
        q = gModelTime / PC_MODEL_DT(i);
        if (q < 0.0f) {
            PC_MODEL_ITERATION(i) = q - 0.5f;
        } else {
            PC_MODEL_ITERATION(i) = q + 0.5f;
        }
        PC_MODEL_MODEL_TIME(i) = gModelTime;
    }
    i++;
    if (i < PC_RACE_CARS) goto loop;
}
