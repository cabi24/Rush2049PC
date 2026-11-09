/* ReadShifter.c: an empty function under the name of MAT3's ReadShifter (controls.c, the manual gear shift).
 *
 * The game's gear shifting is pc_ReadShifter (groups/model_controls_input/group.c), which UpdateModelControls
 * calls; this empty one keeps the symbol the N64's tables name (docs/port/PCONLY.md 4, "what the names left"). */
#include "game/game.h"

/* Does nothing. */
void ReadShifter(void) {

}
