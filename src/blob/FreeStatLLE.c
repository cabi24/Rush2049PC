/* FreeStatLLE (MAT3 statll.c): gives an element back to a static list (InitStatLL.c has the header). */
#include "game/game.h"

/* Unlinks `element` from the list of elements in use (in place when doubly linked, else by a walk from the head) and
 * puts it at the head of the free elements. Nothing for NULL. An element's links are its first two pointers
 * (tDLLE): next, and prev on a doubly linked list. */
void FreeStatLLE(tStatList *list, void *element) {
    tDLLE *e = element;
    tDLLE *cur, *prev;

    if (e) {
        if (list->dbl) {
            if (e->next) ((tDLLE *) e->next)->prev = e->prev;
            cur = e->prev;
            if (cur) cur->next = e->next;
            else list->head = e->next;
        } else {
            cur = list->head; prev = NULL;
            while (cur) {
                if (e == cur) {
                    if (prev) prev->next = e->next;
                    else list->head = e->next;
                    break;
                }
                prev = cur; cur = cur->next;
            }
        }
        e->next = list->store; list->store = e;
    }
}
