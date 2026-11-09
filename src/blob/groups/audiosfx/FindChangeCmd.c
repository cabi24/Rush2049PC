#include "game/game.h"

/* PC: the nodes are SendCmd's (group.c): InsertLLE links them through two pointers, prev and next, and
 * the key is the node's entry pointer (+0x18), which the caller passes (docs/port/LAYOUT.md 5). */
typedef struct Node {
    void *word0;
    struct Node *next;
    u8 kind;
    u8 pad[3];
    intptr_t key;
} Node;
typedef struct List { s32 words[3]; Node *head; } List;
VIEW(volatile List, gCmdQ);
#define gCmdQ gCmdQ_view
Node *FindChangeCmd(intptr_t key) {
    Node *node;
    for (node = (Node *)gCmdQ.head; node != 0; node = node->next) {
        if (node->key == key && node->kind == 3) return node;
    }
    return 0;
}
