/* NewListener: takes a free listener (MAT3 audiomgr.c's _NewListener, which returns it). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Takes the first listener off gFreeListeners and marks it taken. Nothing calls it: AudAddListener takes its
 * listener itself (and the N64's form returns nothing). It used to clear the byte at the N64's offset of free
 * (+8), which on the PC is inside the list links; it clears free itself now. */
void NewListener(void)
{
  tSfxListener *listener;
  listener = (tSfxListener *) gFreeListeners.head;
  RemoveLLE(&gFreeListeners, listener);
  listener->free = 0;
}
