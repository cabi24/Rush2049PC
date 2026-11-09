/* pak_poll_inserted (the N64's Controller Pak system, MAT3 paksysdc.c's role): notes paks plugged in and pulled
 * out, from the controllers' last status query (ControllerUpdate). */
#include "game/game.h"

/* libultra's controller status bits: something is plugged into the controller, and it was pulled out since. */
#define CONT_CARD_ON 0x1
#define CONT_CARD_PULL 0x2

/* Once the pak system is up: a controller with a pak in that hasn't been pulled marks its pak inserted; one without
 * marks a pak that was inserted as gone and changed. */
void pak_poll_inserted(void)
{
    s32 i;

    if (gPakSysInited) {
        for (i = 0; i < 4; i++) {
            if ((cont_status[i].status & CONT_CARD_ON) && !(cont_status[i].status & CONT_CARD_PULL)) {
                if (!gPaks[i].plugged) {
                    gPaks[i].plugged = 1;
                }
            } else if (gPaks[i].plugged) {
                gPaks[i].plugged = 0;
                gPaks[i].wasRemoved = 1;
            }
        }
    }
}
