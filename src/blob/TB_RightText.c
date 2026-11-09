#include "game/game.h"

s16 TB_RightText(void *arg0, s16 arg1) {
    return (s16) (arg1 - HAL_GetTextWidth(arg0, -1));
}
