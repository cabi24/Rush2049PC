/* game_bsearch.c: binary search in a sorted table (MAT3 bsearch.c, bsearch()).
 *
 * The game's own bsearch, used to look names up in the models' sorted name tables (MBOX_FindObject_WORK,
 * MBOX_FindTexture_WORK, MBOX_FindPalette_WORK, NewWorld). */
#include "game/game.h"

/* The element of the table base (nmemb elements of `size` bytes, sorted by compar) that compar(key, element) finds
 * equal to key, or NULL. The halving keeps nmemb the count still to search above base; when the probe is too low
 * the search goes on above it with half the count, one less when the count was even. The last candidate, base, is
 * tested once more after the loop. */
void *game_bsearch(void *key, void *base, u32 nmemb, u32 size, s32 (*compar)(void *, void *)) {
    s32 adjust;
    s32 unused;    /* the decompiled code's; it keeps the unoptimised frame as it was */
    char *limit;
    char *current;
    s32 result;

    if (nmemb == 0 || size == 0) {
        return 0;
    }
    limit = (char *) base + nmemb * size;
    while (nmemb != 0) {
        current = (char *) base + (nmemb / 2) * size;
        result = compar(key, (void *) current);
        if (result < 0) {
            nmemb = nmemb / 2;
        } else if (result > 0) {
            base = (void *) (current + size);
            adjust = (nmemb & 1) ? 0 : 1;
            nmemb = nmemb / 2 - adjust;
        } else {
            return (void *) current;
        }
    }
    if ((char *) base < limit && compar(key, (void *) base) == 0) {
        return (void *) base;
    }
    return 0;
}
