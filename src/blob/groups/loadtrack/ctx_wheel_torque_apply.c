#include "game/game.h"

typedef struct Node104 { u8 opaque[104]; } Node104;
typedef struct NameKey {
    u32 hdr;
    char name[20];
} NameKey;
typedef struct NameEntry {
    Node104 *node;
    char name[16];
} NameEntry;

static void wheel_find_node(u8 *name, Node104 **out) {
    NameKey key;
    game_strcpy((u8 *) key.name, name);
    *out = ((NameEntry *) game_bsearch(&key, WorldTable, *gWorldData, sizeof(NameEntry),
                                           (s32 (*)(void *, void *)) world_name_compare))->node;
}

