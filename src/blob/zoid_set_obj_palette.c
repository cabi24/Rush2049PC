/* zoid_set_obj_palette.c: give a scene node a palette of its own (the N64's; MAT3 has no palettes). */
#include "game/game.h"

/* The scene node (ObjInstanceList's record, types.h ObjectInstance) with MAT3's field names. */

/* Draw node `node` with the palette at `palette` (a palette definition) in place of its model's; 0 gives the
 * model's own back. */
void zoid_set_obj_palette(s16 node, uintptr_t palette) {
    ObjInstanceList[node].palette = (void *)palette;
}
