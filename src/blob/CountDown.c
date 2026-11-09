#include "game/game.h"

void CountDown(void) {
    gCountDown = (s16) (s32) (GetCountdownTimeRemaining() + countdown_round_up);
    if (gCountDown <= 0) {
        gNewGameState = 0x100000;
    }
}
