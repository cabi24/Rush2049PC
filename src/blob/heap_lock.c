/* heap_lock: an empty lock of the heap (MAT3 memsys.c's _GetSemaphore, empty there too). The heap's real lock is
 * heap_lock_queue, which the heap's functions take themselves (memsys/group.c). */
#include "game/game.h"

/* Does nothing. */
void heap_lock(void) {

}
