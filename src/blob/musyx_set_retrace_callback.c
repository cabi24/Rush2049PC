#include "game/game.h"

VIEW(intptr_t, musyx_retrace_callback);
#define musyx_retrace_callback musyx_retrace_callback_view

s32 musyx_set_retrace_callback(intptr_t arg0) {
    musyx_retrace_callback = arg0;
    return 0;
}
