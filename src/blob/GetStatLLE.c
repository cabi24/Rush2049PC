/* GetStatLLE (MAT3 statll.c): takes an element from a static list (InitStatLL.c has the header). */
#include "game/game.h"

/* Takes the first free element and puts it at the head of the list of elements in use (linking it back from the old
 * head when the list is doubly linked). Returns it, or NULL when none is free. An element's links are its first two
 * pointers (tDLLE): next, and prev on a doubly linked list. */
void *GetStatLLE(tStatList *list) {
    tDLLE *element = list->store;

    if (element != NULL) {
        list->store = element->next;
        element->next = list->head;
        if (list->dbl != 0) {
            element->prev = NULL;
            if (list->head != NULL) {
                ((tDLLE *) list->head)->prev = element;
            }
        }
        list->head = element;
    }
    return element;
}
