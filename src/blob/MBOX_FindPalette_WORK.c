/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (also MATCH at -O2) */
/*
 * Historical label sound_bank_load is misleading: this is a name lookup over
 * the per-bank sorted directories (N64-only, no arcade ancestor).
 * sound_bank_load(name, &handle, first, last): copy `name` into a local key
 * (game_strcpy is strcpy), clamp the bank range to [0, gNumModels - 1],
 * and for each bank that ModelActive reports loaded, bsearch (the function
 * labelled game_bsearch) its table D_80138670[bank] of 24-byte records
 * with comparator texcmp.  Returns the record and stores the handle
 * (record index | bank << 10) through `out`; returns 0 and stores 0 when no
 * bank has the name.
 *
 * Shaping quirk: `volatile` on the bank count gNumModels.  Retail reads it
 * twice in address form (`lui; addiu; lbu 0(reg)`); a plain u8 gives the
 * `lbu off(reg)` form (27 words differ).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct {
    char *base;
    u32 count;
} Table;

extern Table gPalDefPtrs[];
extern volatile u8 gNumModels;

char *game_strcpy(char *dst, char *src);
s8 ModelActive(s32 idx);
void *game_bsearch(void *key, void *base, u32 n, u32 size, s32 (*compar)(void *, void *));
s32 texcmp(void *, void *);

char *MBOX_FindPalette_WORK(char *name, u16 *out, s8 lo, s8 hi) {
    char key[28];
    s32 i;
    char *found = 0;

    game_strcpy(key, name);
    if (lo < 0) {
        lo = 0;
    }
    if (hi < 0 || hi >= gNumModels) {
        hi = gNumModels - 1;
    }
    for (i = lo; i <= hi; i++) {
        if (ModelActive(i) == 0) {
            continue;
        }
        found = game_bsearch(key, gPalDefPtrs[i].base, gPalDefPtrs[i].count, 24, texcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        *out = 0;
        return 0;
    }
    *out = ((found - gPalDefPtrs[i].base) / 24) | (i << 10);
    return found;
}
