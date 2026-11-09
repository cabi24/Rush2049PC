/* si_unlock: give back the serial bus (MAT3 controlsys.c's SI lock). */
#include "game/game.h"

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

VIEW(s32, si_lock_queue);
#define si_lock_queue si_lock_queue_view

/* Releases the lock on the serial interface (the controllers and their paks) that si_lock took: its one message goes
 * back into si_lock_queue. */
void si_unlock(void) {
    osSendMesg((OSMesgQueue *) &si_lock_queue, NULL, 0);
}
