/* ClearStatLL (MAT3 statll.c): frees every element of a static list (InitStatLL.c has the header). */
#include "game/game.h"

/* Empties the list in use and chains every element of the array, in order, as the free elements (each one's link,
 * its first pointer, to the next; the last's NULL). */
void ClearStatLL(tStatList *list) {
    u8 *array = list->list;
    s32 i;
    list->head = NULL;
    list->store = array;
    if (array != NULL) {
        for (i = 0; i < list->numElements - 1; i++) {
            ((tStatLLE *) (array + i * list->elementSize))->next = array + (i + 1) * list->elementSize;
        }
        ((tStatLLE *) (array + i * list->elementSize))->next = NULL;
    }
}
