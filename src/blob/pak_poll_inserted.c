/* pak_poll_inserted (the N64's Controller Pak system, MAT3 paksysdc.c's role): notes paks plugged in and pulled
 * out, from the controllers' last status query (ControllerUpdate). */
#include "game/game.h"
#define OSContStatus OSContStatusView   /* this file's own OSContStatus: include/game's OSContStatus is another type */

typedef struct OSContStatus { u16 type; u8 status; u8 errno_; } OSContStatus;

/* A pak's record (gPaks[4]); the PC's layout, its OSPfs host-sized. */
typedef struct Pak772 { u8 index; s8 enabled; s8 inserted; s8 changed; u8 opaque4[8]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Pak772;

/* libultra's controller status bits: something is plugged into the controller, and it was pulled out since. */
#define CONT_CARD_ON 0x1
#define CONT_CARD_PULL 0x2

VIEW(volatile Pak772[4], gPaks);
#define gPaks gPaks_view
VIEW(volatile OSContStatus[4], cont_status);
#define cont_status cont_status_view

/* Once the pak system is up: a controller with a pak in that hasn't been pulled marks its pak inserted; one without
 * marks a pak that was inserted as gone and changed. */
void pak_poll_inserted(void)
{
    s32 i;

    if (gPakSysInited) {
        for (i = 0; i < 4; i++) {
            if ((cont_status[i].status & CONT_CARD_ON) && !(cont_status[i].status & CONT_CARD_PULL)) {
                if (!gPaks[i].inserted) {
                    gPaks[i].inserted = 1;
                }
            } else if (gPaks[i].inserted) {
                gPaks[i].inserted = 0;
                gPaks[i].changed = 1;
            }
        }
    }
}
