/* SyncMaxpathToLastCheckpoint: sends a drone back to its last checkpoint on its drone path (MAT3 maxpath.c). */
#include "game/game.h"

/* Sets the drone in car slot `slot` to jump (its maxpath control's newIdx) to the point where its drone path crosses
 * the last checkpoint it passed. */
void SyncMaxpathToLastCheckpoint(s16 slot)
{
    tCarData *car;

    car = &pc_cars[slot];
    car->mpctl.newIdx = gCheckpoints.points[pc_models[slot].last_cp_index].pathPts[car->mpctl.path + 1];
}
