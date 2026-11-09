#include "game/game.h"

typedef struct Node { struct Node *child; struct Node *next; } Node;

void WalkBSTInternal(intptr_t arg0, Node *n, s32 *count, void (*fn)(Node *, s32)) {
    if (n != 0) {
        WalkBSTInternal(arg0, n->child, count, fn);
        fn(n, *count);
        (*count)++;
        WalkBSTInternal(arg0, n->next, count, fn);
    }
}
