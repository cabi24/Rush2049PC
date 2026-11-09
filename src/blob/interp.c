#include "game/game.h"

s16 interp(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    return (s16) (((s32) ((arg1 - arg0) * arg2) / arg3) + arg0);
}
