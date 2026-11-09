/* NewWorld.c: put a track's world (its static scenery) into the scene (MAT3 world.c, NewWorld() and FindWorld()).
 *
 * A track's world file holds named world trees, each a tree of model parts (tModelPart, 0x68 bytes). Their names
 * and roots are WorldTable (*gWorldData of them, sorted by name), which the world's converter builds beside the file
 * with each root a host pointer (struct NameEntry, port/assets/world.c) and LoadTrack points at. LoadTrack calls
 * NewWorld with the track's world name. */
#include "game/game.h"

/* The root part of the world called `name`, into *out: the bsearch of WorldTable by name (world_name_compare); the
 * name must be there. */
static void FindWorld(u8 *name, struct tModelPart **out) {
    *out = ((NameEntry *) game_bsearch(name, WorldTable, *gWorldData, sizeof(NameEntry), world_name_compare))->node;
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
