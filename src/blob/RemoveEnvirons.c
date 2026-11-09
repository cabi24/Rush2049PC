/* RemoveEnvirons.c: release every environment visual at the end of a race (MAT3 visuals.c, RemoveEnvirons()). */
#include "game/game.h"

VIEW(void *, gEnvList);
#define gEnvList gEnvList_view

/* Release the visuals on gEnvList one by one (ReleaseEnviron unlinks each), then clear the skid marks. */
void RemoveEnvirons(void) {
    void *v;
    while ((v = gEnvList) != 0) {
        ReleaseEnviron(v, 1);
    }
    InitSkids();
}
