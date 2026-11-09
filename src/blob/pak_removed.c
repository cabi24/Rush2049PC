#include "game/game.h"

VIEW(s8, gPaks);
#define gPaks gPaks_view

/* PC: the controller/pak records gPaks are 0x310 bytes (paksys' PakRecord, docs/port/POINTERS.md 9.6); the inserted
 * flag, +1, is before the OSPfs's pointer, so it is where the N64 has it. */
#define PAK_RECORD 0x310
s32 pak_removed(s32 arg0) {
    return ((s8 *) &gPaks)[arg0 * PAK_RECORD + 1] == 0;
}
