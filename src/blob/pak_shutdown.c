/* pak_shutdown (the N64's Controller Pak system): stops the pak system's polling. */
#include "game/game.h"

/* Marks the pak system down (gPakSysInited), so the pak polls stop. Nothing calls it (MainPreNMI clears the flag
 * itself). */
void pak_shutdown(void) {
    gPakSysInited = 0;
}
