/* MBGetFromWad (MAT3 mb_wad.c): the data a WAD holds under a tag (MBSetupWad.c has the WADs). */
#include "game/game.h"

/* The window address of wad's data tagged tag, and its length in *length (when length isn't 0); 0 and a length of 0
 * when wad is 0 or has no such tag.
 * PC: the address is returned as a u32, so callers widen it to a pointer without sign extension (docs/port/ASSETS.md
 * section 4 #3). */
u32 MBGetFromWad(WadHandle *wad, s32 tag, s32 *length) {
    WadEntry *entry;

    entry = NULL;
    if (wad != NULL) {
        entry = MBFindInWad(wad, tag);
    }
    if (entry == NULL) {
        if (length != NULL) {
            *length = 0;
        }
        return 0;
    }
    if (length != NULL) {
        *length = entry->length;
    }
    return entry->address;
}
