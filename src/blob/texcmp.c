#include "game/game.h"

/* PC: this is the bsearch comparator of the TXHD and PLHD name lookups (MBOX_FindTexture_WORK, MBOX_FindPalette_WORK). As
 * void it only passed game_strcmp's result on by leaving it in the return register. */
#define THUNK_TYPE s32
#define THUNK_RETURN return
THUNK_TYPE texcmp(u8 *arg0, u8 *arg1) {
    THUNK_RETURN game_strcmp(arg0, arg1);
}
