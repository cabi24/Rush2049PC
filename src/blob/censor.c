/* censor: the high-score name filter (MAT3 highscorescr.c censor). ControlName runs it on a name the player has
 * just entered. */
#include "game/game.h"

/* The words it looks for (obscenities): 21 strings and an empty one that ends the list. */
typedef struct List { u8 *p[22]; } List;
VIEW(List, obscenities);
#define obscenities obscenities_view
u8 fixword(u8 *a, intptr_t b);

/* Overwrite with '!' every word of the list that name contains (fixword does one word, letting its letters be
 * doubled or spaced out). Returns non-zero if it changed anything. */
u8 censor(intptr_t name) {
    u8 changed;
    u8 **word;
    List list;
    list = obscenities;
    changed = 0;
    word = list.p;
    for (; **word != 0; word++) {
        changed |= fixword(*word, name);
    }
    return changed;
}
