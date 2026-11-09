/* ClearStatLL (MAT3 statll.c): frees every element of a static list (InitStatLL.c has the header). */
#include "game/game.h"

typedef struct Pool { u8 dbl; u8 n64; u8 pad2[2]; s32 numElements; s32 elementSize; u8 *list; intptr_t head; u8 *store; } Pool;

/* Empties the list in use and chains every element of the array, in order, as the free elements. */
void ClearStatLL(Pool *list) {
    u8 *array = list->list;
    s32 i;
    list->head = 0;
    list->store = array;
    if (array != 0) {
        for (i = 0; i < list->numElements - 1; i++) {
            pc_statll_set_link(list->n64, array + i * list->elementSize, 0, array + (i + 1) * list->elementSize);
        }
        pc_statll_set_link(list->n64, array + i * list->elementSize, 0, 0);
    }
}
