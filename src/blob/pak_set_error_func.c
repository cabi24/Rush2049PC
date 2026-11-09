#include "game/game.h"

VIEW(intptr_t, gErrorFunc);
#define gErrorFunc gErrorFunc_view

void pak_set_error_func(s32 arg0) {
    gErrorFunc = arg0;
}
