#include "game/game.h"

/* PC: GetStatLLE over the header's host layout (shots.c's StatLL, docs/port/POINTERS.md 9): takes the first free
 * element and puts it at the head of the active list (and links it back when the list is doubly linked); the m2c
 * body uses the N64 offsets. An element's links are its first two words: pointers, or window addresses in a list
 * flagged n64 (POINTERS.md 10). */
typedef struct { s8 dbl; u8 n64; s32 numElements; s32 elementSize; void *list; void *head; void *store; } PcStatLL;
void *GetStatLLE(void *arg0) {
    PcStatLL *list = (PcStatLL *) arg0;
    void *e = list->store;

    if (e != NULL) {
        list->store = pc_statll_link(list->n64, e, 0);
        pc_statll_set_link(list->n64, e, 0, list->head);
        if (list->dbl != 0) {
            pc_statll_set_link(list->n64, e, 1, NULL);
            if (list->head != NULL) {
                pc_statll_set_link(list->n64, list->head, 1, e);
            }
        }
        list->head = e;
    }
    return e;
}
