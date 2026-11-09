#include "game/game.h"

s32 controls_validOption(s8 player, s32 item) {
    s32 var_v1;

    var_v1 = 1;
    switch (item) {
    case 8:
        var_v1 = 0;
        break;
    }
    return var_v1;
}
