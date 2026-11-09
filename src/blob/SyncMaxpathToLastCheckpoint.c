/* SyncMaxpathToLastCheckpoint: sends a drone back to its last checkpoint on its drone path (MAT3 maxpath.c). */
#include "game/game.h"

/* Sets the drone in car slot `slot` to jump (its maxpath control's newIdx) to the point where its drone path crosses
 * the last checkpoint it passed. */
void SyncMaxpathToLastCheckpoint(s16 slot)
{
    struct pc_car *car;

    car = PC_CAR(slot);
    car->nav.newIdx = gCheckpoints.points[PC_MODEL_LAST_CP(slot)].pathPts[car->nav.path + 1];
}
