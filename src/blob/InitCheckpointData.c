/* InitCheckpointData: the checkpoints at a track's load (MAT3 checkpoint.c). */
#include "game/game.h"

/* Turns off the first two cars' finish cameras (MAT3 finish_cam[0] and [1]; the PC's pc_finish_cam, one per car
 * slot) and reads the track's checkpoints from its path file. */
void InitCheckpointData(void) {
    PC_FINISH_CAM(0) = 0;
    PC_FINISH_CAM(1) = 0;
    ReadCheckpointData();
}
