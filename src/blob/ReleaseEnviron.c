#include "game/game.h"

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Node {
    struct Node *next;
    unsigned short tag;
    s16 handle;
    unsigned char opaque[12];
    intptr_t state;
} Node;

void ReleaseEnviron(Node *node,int unlink) {
    Node *previous,*current;
    if (node->handle>=0) MBOX_HideObject(node->handle,1,15);
    node->state=0;
    node->handle=-1;
    if (unlink) {
        current=gEnvList;
        if (node==current) gEnvList=current->next;
        else {
            previous=current;
            while (current && ((current=current->next),node!=current)) {
                previous=current;
            }
            if (previous && previous->next==node) previous->next=node->next;
        }
    }
    node->next=gFreeVis;
    gEnvCount--;
    gFreeVis=node;
    if (gEnvCountMax<gEnvCount) gEnvCountMax=gEnvCount;
}
