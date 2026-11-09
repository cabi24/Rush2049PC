/* WorldClear.c: forget the track's world (MAT3 world.c, WorldClear()). */
#include "game/game.h"

VIEW(intptr_t, WorldTable);
#define WorldTable WorldTable_view

VIEW(intptr_t, WorldBase);
#define WorldBase WorldBase_view

/* Clear the world table and the world's part array (when the track is unloaded). */
void WorldClear(void) {
    WorldTable = 0;
    WorldBase = 0;
}
