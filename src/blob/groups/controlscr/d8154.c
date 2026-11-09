/* validOption (MAT3 controlscr.c): which lines of the controls screen are offered. */
#include "game/game.h"

/* Whether line item of player's controls screen is offered: all but 8. */
s32 controls_validOption(s8 player, s32 item) {
    s32 valid;

    valid = 1;
    switch (item) {
    case 8:
        valid = 0;
        break;
    }
    return valid;
}
