/* init_model_task.c: set the model clock for the race start (MAT3 mdrive.c, init_model_task()).
 *
 * The model thread counts its steps (gModelIteration) and their time (gModelTime); the race clock is 0 when the
 * countdown ends, so before it starts the clocks are set that many steps back. */
#include "game/game.h"

/* Rewind the model clock `frames` steps of gRetraceTime seconds before 0, and set each car's own clock to it. In a
 * ghost race a ghost car (any but the first) that has a step time of its own, its recording's, keeps it and gets
 * the nearest step count to the same time. */
void init_model_task(s32 frames)
{
    f32 iteration;
    s32 i;
    f32 dt;

    gModelIteration = -frames;
    dt = gRetraceTime;
    gModelTime = gModelIteration * dt;
    i = 0;
    do {
        if (gRace != 2 || i == 0 || PC_MODEL_DT(i) == 0.0f) {   /* kGhost */
            PC_MODEL_ITERATION(i) = gModelIteration;
            PC_MODEL_MODEL_TIME(i) = gModelTime;
            PC_MODEL_DT(i) = dt;
        } else {
            iteration = gModelTime / PC_MODEL_DT(i);
            if (iteration < 0.0f) {
                PC_MODEL_ITERATION(i) = iteration - 0.5f;
            } else {
                PC_MODEL_ITERATION(i) = iteration + 0.5f;
            }
            PC_MODEL_MODEL_TIME(i) = gModelTime;
        }
        i++;
    } while (i < PC_RACE_CARS);
}
