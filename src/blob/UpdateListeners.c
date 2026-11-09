#include "game/game.h"

/* PC: an input record is 80 bytes (its pointer at +0x48 is 8 bytes). The node at +0x44 stays 32 bits: it is a
 * packet's window address (a heap block, AudAddListener) or -1 (docs/port/POINTERS.md 10). */
typedef struct { u8 model; u8 pad1[67]; u32 node; void *ref; } CarRec;
#define NO_NODE 0xFFFFFFFFu
#define NODE(n) (void *)PC_N64P(n)

typedef struct View { u8 pad0[12]; f32 a[3]; f32 b[3]; u8 pad24[152 - 36]; } View;  /* 152 */

extern CarRec gPlayers[];
VIEW(f32[], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view
VIEW(View[], gCamera);
#define gCamera gCamera_view

void UpdateListeners(void)
{
    s32 i;
    void *node;

    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].node != NO_NODE) {
            node = NODE(gPlayers[i].node);
            AudUpdateListener(node, PC_MODEL_RWR(gPlayers[i].model), unitmatrix_origin,
                            gCamera[i].b, gCamera[i].a);
        }
    }
}
