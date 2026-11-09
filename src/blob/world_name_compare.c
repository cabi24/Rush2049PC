#include "game/game.h"

/* NewWorld's bsearch comparator (NewWorld): it gets pointers (its key is on the stack), and the
 * search reads the result game_strcmp leaves in v0 through the void return. On the PC both are explicit. */
#define PC_V0_S32 s32
#define PC_V0_RETURN return
PC_V0_S32 world_name_compare(intptr_t arg0, intptr_t arg1) {
    PC_V0_RETURN game_strcmp(arg0 + 4, arg1 + 4);
}
