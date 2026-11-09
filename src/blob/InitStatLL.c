/* InitStatLL (MAT3 statll.c): a static list, a pool of fixed-size elements carved from an array, linked singly or
 * doubly. GetStatLLE takes an element, FreeStatLLE gives one back, ClearStatLL frees them all.
 *
 * The header's PC layout (MAT3 tStatList with pointer-sized pointers, as include/game's tStatList): the doubly-linked
 * flag, then n64, which says whether the elements link by 32-bit window addresses (the level objects' nodes, which
 * keep their N64 layout) or by host pointers (everything else: docs/port/POINTERS.md 9, 10). */
#include "game/game.h"

typedef struct {
    s8 dbl;             /* 0x00: doubly linked */
    u8 n64;             /* 0x01: PC: the elements link by window addresses */
    s32 numElements;    /* 0x04 */
    s32 elementSize;    /* 0x08 */
    void *list;         /* 0x10: the array */
    void *head;         /* 0x18: the first element in use */
    void *store;        /* 0x20: the first free element */
} StatList;

/* Makes a list of numElements elements of elementSize bytes each from array, all free; dbl says whether the
 * elements are doubly linked (a tDLLE at their start, else a tStatLLE). */
void InitStatLL(void *list, void *array, s32 elementSize, s32 numElements, u8 dbl) {
    StatList *header = (StatList *) list;

    header->list = array;
    header->numElements = numElements;
    header->elementSize = elementSize;
    header->dbl = dbl;
    header->n64 = 0;
    ClearStatLL(list);
}
