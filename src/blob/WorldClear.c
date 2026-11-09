/* WorldClear.c: forget the track's world (MAT3 world.c, WorldClear()). */
#include "game/game.h"

/* Clear the world table and the world's part array (when the track is unloaded). */
void WorldClear(void) {
    WorldTable = 0;
    WorldBase = 0;
}
