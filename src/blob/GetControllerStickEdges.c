#include "game/game.h"

VIEW(s8, gStickEdges);
#define gStickEdges gStickEdges_view

/* lui 0x8015 base of gStickEdges[arg0]; the symbol lives in an N64-layout arena on PC */
#define N64_HI_80150000 ((intptr_t) &gStickEdges + 0x648C)

s8 GetControllerStickEdges(s32 arg0) {
    s8 *edges = (s8 *) &gStickEdges;   /* s8[4]: each controller's */
    if (arg0 == -1) {
        if (edges[0] == 0) {
            return 0;
        }
        if (edges[1] == 0) {
            return 0;
        }
        if (edges[2] == 0) {
            return 0;
        }
        if (edges[3] == 0) {
            return 0;
        }
        return 1;
    }
    return M2C_FIELD((arg0 + N64_HI_80150000), s8 *, -0x648C);
}
