/* world_name_compare.c: the comparator of the world name search (NewWorld). */
#include "game/game.h"

/* Compare the names of the search key and a world table entry (each record has its name at +4: NewWorld's
 * NameKey and the WHDR entry NameEntry): game_strcmp's result. The records come as integers of pointer width. On the
 * N64 the function returned nothing and passed game_strcmp's result on by leaving it in the return register; on the
 * PC it returns it. */
s32 world_name_compare(intptr_t key, intptr_t entry) {
    return game_strcmp(key + 4, entry + 4);
}
