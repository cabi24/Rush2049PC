/* censor: the high-score name filter (MAT3 highscorescr.c censor). ControlName runs it on a name the player has
 * just entered. */
#include "game/game.h"

/* Overwrite with '!' every word of the list (obscenities: 21 strings and an empty one that ends it) that name
 * contains (fixword does one word, letting its letters be doubled or spaced out). name is a row of a table that ends
 * at end, where fixword's search stops. Returns non-zero if it changed anything. */
u8 censor(u8 *name, const u8 *end) {
    u8 changed;
    u8 **word;
    ObscenityList list;
    list = obscenities;
    changed = 0;
    word = list.p;
    for (; **word != 0; word++) {
        changed |= fixword(*word, name, end);
    }
    return changed;
}
