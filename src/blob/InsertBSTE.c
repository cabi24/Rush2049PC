#include "game/game.h"

/* PC: arg0 is an ordered list (tOrderedList, audiomgr_thread.c): its order function is a pointer, so the root is at
 * +0x10 (docs/port/LAYOUT.md 5). */
#define OL_ROOT 0x10
void InsertBSTE(void *arg0, s32 *arg1) {
    InsertBSTEInternal(arg0, (u8 *) arg0 + OL_ROOT, arg1);
}
