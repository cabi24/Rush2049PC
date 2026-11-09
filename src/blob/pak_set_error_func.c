/* pak_set_error_func (the N64's Controller Pak system): the function the pak code calls on an error. */
#include "game/game.h"

VIEW(intptr_t, gErrorFunc);
#define gErrorFunc gErrorFunc_view

/* Sets the pak error callback (gErrorFunc) to func. Nothing calls it.
 * PC: func arrives as a 32-bit integer and is stored sign-extended. */
void pak_set_error_func(s32 func) {
    gErrorFunc = func;
}
