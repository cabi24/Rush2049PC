#include "game/game.h"

VIEW(intptr_t, WorldTable);
#define WorldTable WorldTable_view

VIEW(intptr_t, WorldBase);
#define WorldBase WorldBase_view

void WorldClear(void) {
    WorldTable = 0;
    WorldBase = 0;
}
