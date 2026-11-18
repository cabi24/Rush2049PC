/* si_unlock: give back the serial bus (MAT3 controlsys.c's SI lock). */
#include "game/game.h"


/* Releases the lock on the serial interface (the controllers and their paks) that si_lock took: its one message goes
 * back into si_lock_queue. */
void si_unlock(void) {
    lock_give(&si_lock_queue);
}
