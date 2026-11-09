/* world_name_compare.c: the comparator of the world name search (NewWorld). */
#include "game/game.h"

/* Compare the name searched for (a string) with a world table entry's (a NameEntry): game_strcmp's result. On the
 * N64 the function returned nothing and passed game_strcmp's result on by leaving it in the return register; on the
 * PC it returns it. MAT3's worldcmp compares two entries, the key a copy of the name in one; the name is compared as
 * it is here. */
s32 world_name_compare(void *name, void *entry) {
    return game_strcmp((u8 *) name, (u8 *) ((NameEntry *) entry)->name);
}
