#include "game/game.h"

VIEW(intptr_t, gEnvList);
#define gEnvList gEnvList_view

/* AddToEnvList: arg0 is a visual, whose next (+0) is pointer-sized on the PC like the list head gEnvList
 * (docs/port/CARS.md 3). No C calls it; gen_data's function table has it, so ROM data could (docs/port/LAYOUT.md 6). */
void AddToEnvList(intptr_t *arg0) {
    *arg0 = gEnvList;
    gEnvList = (intptr_t) arg0;
}
