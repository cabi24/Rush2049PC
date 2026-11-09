#include "game/game.h"

VIEW(void *, gEnvList);
#define gEnvList gEnvList_view

void RemoveEnvirons(void) {
    void *p;
    while ((p = gEnvList) != 0) {
        ReleaseEnviron(p, 1);
    }
    InitSkids();
}
