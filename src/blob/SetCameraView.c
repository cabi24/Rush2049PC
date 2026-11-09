/* SetCameraView: a camera's view changed (MAT3 camera.c). */
#include "game/game.h"

/* The cameras' smoothed accelerations, as arrays. */

/* Clears camera `camera`'s smoothed acceleration (cur_acc), which the chase views lag by, so a new view starts
 * steady. */
void SetCameraView(s16 camera)
{
    cur_acc[camera][0] = 0.0f;
    cur_acc[camera][1] = 0.0f;
    cur_acc[camera][2] = 0.0f;
}
