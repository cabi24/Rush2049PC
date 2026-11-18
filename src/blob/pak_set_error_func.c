/* pak_set_error_func (the N64's Controller Pak system): the function the pak code calls on an error. */
#include "game/game.h"

/* Sets the pak error callback (gErrorFunc) to func. Nothing calls it. func arrives as a 32-bit integer (the N64's
 * address of the function), so it is stored as the pointer that integer sign-extends to. */
void pak_set_error_func(s32 func) {
    gErrorFunc = (PakErrorFunc)(intptr_t)func;
}
