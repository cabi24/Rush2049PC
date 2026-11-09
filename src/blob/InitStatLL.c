#include "game/game.h"

/* PC: InitStatLL (head, element array, element size, count, doubly) by field; the m2c body writes the header at
 * the N64 offsets with the array as an s32. The header's host layout is shots.c's StatLL (docs/port/POINTERS.md 9).
 * Its elements are host records, linked by pointers: the n64 flag is clear (POINTERS.md 10). */
typedef struct { s8 dbl; u8 n64; s32 numElements; s32 elementSize; void *list; void *head; void *store; } PcStatLL;
void InitStatLL(void *arg0, void *arg1, s32 arg2, s32 arg3, u8 arg4) {
    PcStatLL *list = (PcStatLL *) arg0;

    list->list = arg1;
    list->numElements = arg3;
    list->elementSize = arg2;
    list->dbl = arg4;
    list->n64 = 0;
    ClearStatLL(arg0);
}
