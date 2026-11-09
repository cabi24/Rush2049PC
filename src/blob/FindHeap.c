#include "game/game.h"

VIEW(intptr_t, gMainHeap);
#define gMainHeap gMainHeap_view

#define HEAP_LINK u32   /* heap headers hold RDRAM-window addresses: zero-extend them */
/* ... and translate them to the heap's place (pc_prelude.h, docs/port/NOFIXED.md 2.3) */
#define HEAP_NEXT(x) ((intptr_t)PC_N64P(x))
void *FindHeap(u32 arg0) {
    intptr_t var_v0;
    intptr_t var_v1;

    var_v0 = gMainHeap;
    var_v1 = 0;
    if (var_v0 != 0) {
        do {
            if ((arg0 >= (u32) M2C_FIELD(var_v0, u32 *, 8)) && (arg0 < (u32) M2C_FIELD(var_v0, u32 *, 0x10))) {
                var_v1 = var_v0;
            }
            var_v0 = HEAP_NEXT(M2C_FIELD(var_v0, HEAP_LINK *, 4));
        } while (var_v0 != 0);
    }
    return (void *) var_v1;
}
