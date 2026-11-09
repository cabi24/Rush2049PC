#include "game/game.h"
#define OSContStatus OSContStatusView   /* this file's own OSContStatus: include/game's OSContStatus is another type */

typedef struct OSContStatus { u16 type; u8 status; u8 errno_; } OSContStatus;

typedef struct Pak772 { u8 index; s8 enabled; s8 inserted; s8 changed; u8 opaque4[8]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Pak772;

VIEW(volatile Pak772[4], gPaks);
#define gPaks gPaks_view
VIEW(volatile OSContStatus[4], cont_status);
#define cont_status cont_status_view

void pak_poll_inserted(void)
{
    s32 i;

    if (gPakSysInited) {
        for (i = 0; i < 4; i++) {
            if ((cont_status[i].status & 1) && !(cont_status[i].status & 2)) {
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
