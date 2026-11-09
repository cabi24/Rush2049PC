#include "game/game.h"

VIEW(intptr_t, musyx_retrace_callback);
#define musyx_retrace_callback musyx_retrace_callback_view

void musyx_clear_retrace_callback(s32 arg0) {
    musyx_retrace_callback = 0;
}
