/* CancelFunc: the controller-pak manager's test that closes a pak error box (MAT3 pakscr.c CancelFunc). */
#include "game/game.h"

/* Should the error box over pak `pak` go away by itself? Yes once the pak has been changed, or, for error 2
 * (gPakErr), once the pak reads as damaged. 1 yes, 0 no. */
s32 CancelFunc(s32 pak)
{
    if (PS_PakChanged(pak) || (gPakErr == 2 && PS_PakDamaged(pak))) {
        return 1;
    }
    return 0;
}
