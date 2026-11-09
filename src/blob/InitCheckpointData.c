#include "game/game.h"

/* On the PC the finish cameras' state is pc_finish_cam, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md
 * 6); this clears the first two slots', as on the N64. */
#define FINISH_CAM_FIRST PC_FINISH_CAM(0)
#define FINISH_CAM_SECOND PC_FINISH_CAM(1)

void InitCheckpointData(void) {FINISH_CAM_FIRST=0;FINISH_CAM_SECOND=0;ReadCheckpointData();}
