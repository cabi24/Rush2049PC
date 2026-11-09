/* FreeStatLLE (MAT3 statll.c): gives an element back to a static list (InitStatLL.c has the header). */
#include "game/game.h"

/* An element's links: link 0 the next element, link 1 the previous one (doubly-linked lists only). */
typedef struct Node {struct Node *next,*prev;} Node;
typedef struct Pool {u8 dbl,n64;s32 numElements,elementSize;void *list;Node *head,*store;} Pool;
#define NEXT(n) ((Node *)pc_statll_link(list->n64,n,0))
#define PREV(n) ((Node *)pc_statll_link(list->n64,n,1))

/* Unlinks element from the list of elements in use (in place when doubly linked, else by a walk from the head) and
 * puts it at the head of the free elements. Nothing for 0. */
void FreeStatLLE(Pool *list, Node *element) {
    Node *cur, *prev;

    if (element) {
        if (list->dbl) {
            if (NEXT(element)) pc_statll_set_link(list->n64, NEXT(element), 1, PREV(element));
            cur = PREV(element);
            if (cur) pc_statll_set_link(list->n64, cur, 0, NEXT(element));
            else list->head = NEXT(element);
        } else {
            cur = list->head; prev = 0;
            while (cur) {
                if (element == cur) {
                    if (prev) pc_statll_set_link(list->n64, prev, 0, NEXT(element));
                    else list->head = NEXT(element);
                    break;
                }
                prev = cur; cur = NEXT(cur);
            }
        }
        pc_statll_set_link(list->n64, element, 0, list->store); list->store = element;
    }
}
