/* animate_blit_unchanged: a blit animation function that leaves its blit as it is (N64 only). A MultiBlit table
 * names it for a blit that needs a callback but has nothing to update. */
#include "game/game.h"

/* Do nothing to the blit; 1 keeps it on screen. */
s32 animate_blit_unchanged(s32 blit) {
    return 1;
}
