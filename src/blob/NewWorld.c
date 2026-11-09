#include "game/game.h"

typedef struct Node104 { u8 opaque[104]; } Node104;
typedef struct NameKey {
    u32 hdr;
    char name[20];
} NameKey;
/* PC: the entry is the world file's 0x14-byte WHDR record; LoadTrack has made node a window address
 * (port/assets/world.c), which converts to the Node104 pointer as it is. */
typedef struct NameEntry {
    u32 node;
    char name[16];
} NameEntry;

VIEW(Node104 *, WorldBase);
#define WorldBase WorldBase_view

/* PC: a world's name entry keeps its node's 32-bit N64 address, which the node found becomes the pointer at
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
static void wheel_find_node(u8 *name, Node104 **out) {
    NameKey key;
    game_strcpy((u8 *) key.name, name);
    *out = ((NameEntry *) game_bsearch(&key, WorldTable, *gWorldData, sizeof(NameEntry),
                                           (s32 (*)(void *, void *)) world_name_compare))->node; *out = (Node104 *)PC_N64P((u32)(uintptr_t)*out);
}

s16 NewWorld(u8 *arg0, s16 arg1) {
    Worlddlidx = -1;
    wheel_find_node(arg0, &WorldBase);
    NewWorldObject(WorldBase, arg1);
    InitSTree();
    return Worlddlidx;
}
