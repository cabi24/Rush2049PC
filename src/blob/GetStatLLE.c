/* GetStatLLE (MAT3 statll.c): takes an element from a static list (InitStatLL.c has the header). */
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

/* Takes the first free element and puts it at the head of the list of elements in use (linking it back from the old
 * head when the list is doubly linked). Returns it, or 0 when none is free. An element's links are its first two
 * words: link 0 the next element, link 1 the previous one (pc_statll_link reads either kind of link). */
void *GetStatLLE(void *list) {
    StatList *header = (StatList *) list;
    void *element = header->store;

    if (element != NULL) {
        header->store = pc_statll_link(header->n64, element, 0);
        pc_statll_set_link(header->n64, element, 0, header->head);
        if (header->dbl != 0) {
            pc_statll_set_link(header->n64, element, 1, NULL);
            if (header->head != NULL) {
                pc_statll_set_link(header->n64, header->head, 1, element);
            }
        }
        header->head = element;
    }
    return element;
}
