/* CountDown: the countdown before a race (MAT3 countdown.c). */
#include "game/game.h"

/* Sets gCountDown, the whole seconds left on the countdown (rounded up by countdown_round_up); at 0 the race
 * starts (GS_GO). */
void CountDown(void) {
    gCountDown = (s16) (s32) (GetCountdownTimeRemaining() + countdown_round_up);
    if (gCountDown <= 0) {
        gNewGameState = GS_GO;
    }
}
