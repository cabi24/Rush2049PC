/* loadtrack/ctx_wheel_torque_apply.c: a static helper nothing calls, despite the file's name.
 *
 * wheel_find_node looks a world object up by name in the loaded world's sorted name table (WorldTable, as MAT3
 * world.c's lookups do); groups/waves/NewWorld.c has the same static copy. The file defines nothing else. */
#include "game/game.h"

typedef struct Node104 { u8 opaque[104]; } Node104;
/* The search key: the name at the entry's name offset. */
typedef struct NameKey {
    u32 hdr;
    char name[20];
} NameKey;
/* An entry of WorldTable: the object and its name. */
typedef struct NameEntry {
    Node104 *node;
    char name[16];
} NameEntry;

/* *out = the world object called `name` (a binary search of WorldTable's *gWorldData entries). */
static void wheel_find_node(u8 *name, Node104 **out) {
    NameKey key;
    game_strcpy((u8 *) key.name, name);
    *out = ((NameEntry *) game_bsearch(&key, WorldTable, *gWorldData, sizeof(NameEntry),
                                           (s32 (*)(void *, void *)) world_name_compare))->node;
}
