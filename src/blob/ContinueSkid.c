/* ContinueSkid.c: the retail game's empty ContinueSkid (MAT3 skids.c, _ContinueSkid()).
 *
 * The retail game keeps only an empty stub at this name: the compiler inlined the original static into its one
 * caller, UpdateSkids. The body the PC runs is pc_ContinueSkid in groups/skid_marks; the stub stays because the
 * symbol tables still name it (docs/port/PCONLY.md 4, "what the names left"). */
#include "game/game.h"

/* Does nothing. */
void ContinueSkid(void) {

}
