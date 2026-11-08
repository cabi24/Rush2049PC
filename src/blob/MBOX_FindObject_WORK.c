/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * MBOX_FindObject_WORK (historical label): look an 88-byte record up by name across a range of sorted
 * tables and return its packed handle (index | table << 10), or -1.
 * The name is copied into a local key with game_strncpy (strncpy, n = 16), or "AAANULLOBJ" when the
 * name is NULL/empty (strcpy intrinsic: retail's lwl/lwr copy of the literal at 0x80120E68). Then the
 * same clamp/loop/bsearch as the matched sibling MBOX_FindTexture_WORK: gRomObjDefPtrs[i] = { base, count },
 * element size 88, game_bsearch = bsearch, objcmp = comparator.
 *
 * STRICT MATCH. Shaping quirk: the fourth parameter `complain` (callers pass 1)
 * is read only by a compiled-out diagnostic in the not-found branch (`if (complain) DEBUG_PRINT(...);`
 * with an empty macro). That read keeps its a3 web alive from entry, so uopt cannot colour the copy
 * of `name` into a3 and picks t0 (retail `move t0,a0`); it also explains the a3 home store that is
 * never reloaded. Without it `name` lands in a3 (36 words). Found with the instrumented uopt
 * globalcolor trace (forbidden mask of the name web lacked a3). Matches in the whole-program unit too.
 */
#define DEBUG_PRINT(args)
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

extern char *strcpy(char *, const char *);
#pragma intrinsic (strcpy)

typedef struct Rec {
    char name[88];
} Rec;

typedef struct RecTable {
    Rec *base;
    u32 count;
} RecTable;

extern RecTable gRomObjDefPtrs[];
extern volatile u8 gNumModels;

s8 ModelActive(s32 arg0);
char *game_strncpy(char *dst, char *src, s32 n);
void *game_bsearch(void *key, void *base, u32 n, u32 size, s32 (*compar)(void *, void *));
s32 objcmp(void *, void *);

s32 MBOX_FindObject_WORK(char *name, s8 first, s8 last, s8 complain) {
    s32 i;
    Rec key;
    Rec *found;

    found = 0;
    if (name == 0 || *name == 0) {
        strcpy(key.name, "AAANULLOBJ");
    } else {
        game_strncpy(key.name, name, 16);
    }
    if (first < 0) {
        first = 0;
    }
    if (last < 0 || last >= gNumModels) {
        last = gNumModels - 1;
    }
    for (i = first; i <= last; i++) {
        if (ModelActive(i) == 0) {
            continue;
        }
        found = game_bsearch(&key, gRomObjDefPtrs[i].base, gRomObjDefPtrs[i].count, sizeof(Rec), objcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        if (complain) DEBUG_PRINT(("string_copy_format: %s not found\n", key.name));
        return -1;
    }
    return (found - gRomObjDefPtrs[i].base) | (i << 10);
}
