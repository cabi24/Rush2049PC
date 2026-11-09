/* NewWorld.c: put a track's world (its static scenery) into the scene (MAT3 world.c, NewWorld() and FindWorld()).
 *
 * A track's world file holds named world trees (WHDR entries, sorted by name: WorldTable, *gWorldData of them),
 * each a tree of model parts (tModelPart, 0x68 bytes). LoadTrack calls NewWorld with the track's world name. */
#include "game/game.h"

typedef struct tModelPart { u8 opaque[104]; } tModelPart;
/* The search key: a WHDR entry with only the name filled in. */
typedef struct NameKey {
    u32 hdr;
    char name[20];
} NameKey;
/* PC: the entry is the world file's 0x14-byte WHDR record (MAT3 anon_woff_wname_14: woff, wname); LoadTrack has
 * made node a window address (port/assets/world.c), which converts to the tModelPart pointer as it is. */
typedef struct NameEntry {
    u32 node;
    char name[16];
} NameEntry;

VIEW(tModelPart *, WorldBase);
#define WorldBase WorldBase_view

/* The root part of the world called `name`, into *out: the bsearch of WorldTable by name (world_name_compare); the
 * name must be there. PC: a world's name entry keeps its node's 32-bit N64 address, which the node found becomes
 * the pointer at (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
static void FindWorld(u8 *name, tModelPart **out) {
    NameKey key;
    game_strcpy((u8 *) key.name, name);
    *out = ((NameEntry *) game_bsearch(&key, WorldTable, *gWorldData, sizeof(NameEntry),
                                           (s32 (*)(void *, void *)) world_name_compare))->node; *out = (tModelPart *)PC_N64P((u32)(uintptr_t)*out);
}

/* Add the world called `world` to the scene under the scene node `parent` (-1: at the top): find its root part
 * (WorldBase), add every part as a scene node (NewWorldObject) and build the road quadtree (InitSTree). Returns the
 * first scene node made (Worlddlidx), -1 if none. */
s16 NewWorld(u8 *world, s16 parent) {
    Worlddlidx = -1;
    FindWorld(world, &WorldBase);
    NewWorldObject(WorldBase, parent);
    InitSTree();
    return Worlddlidx;
}
