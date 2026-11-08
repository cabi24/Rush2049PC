/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * NewWorld (historical label; 0x800AC668, 140 bytes): look a node up by name and place it.
 * Clears the "first registered id" Worlddlidx to -1, copies the name into a search key (strcpy-style
 * game_strcpy), bsearches (game_bsearch) the 20-byte name table WorldTable (count *gWorldData,
 * compared by world_name_compare on the names at +4), stores the entry's node pointer in the pool
 * pointer WorldBase, runs NewWorldObject(WorldBase, parent) (the recursive node placement), then
 * InitSTree (table pointer rebuild) and returns Worlddlidx. No arcade ancestor identified.
 *
 * : the lookup is a static helper that umerge inlines (no stub is emitted for it):
 *  - its key local lives in the inlined-call area (24-byte key + the name parameter = 32 bytes, frame 64);
 *  - its statements carry the helper's (earlier) source lines, so as1 schedules the key address
 *    `addiu a0,sp,36` before the caller's `sh` to Worlddlidx and the sh fills the jal delay slot;
 *  - the store goes through the out parameter (*out = ...), so uopt keeps it a separate address from the
 *    caller's WorldBase load: no shared `la`, no store-to-load forwarding (retail's lui at / lui a0 pair).
 * No shaping constructs (no volatile, no compiled-out reads, no unused locals).
 */
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

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

extern s16 Worlddlidx;
extern Node104 *WorldBase;
extern u32 *gWorldData;
extern NameEntry *WorldTable;
extern void world_name_compare(s32 arg0, s32 arg1);
extern u8 *game_strcpy(u8 *arg0, u8 *arg1);
extern void *game_bsearch(void *key, void *base, u32 n, u32 size, s32 (*compar)(void *, void *));
extern void NewWorldObject(Node104 *node, s16 parent);
extern void InitSTree(void);

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
