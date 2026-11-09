#include "game/game.h"

typedef struct VisualNode VisualNode;
/* PC: paksys' tPS_File (docs/port/POINTERS.md 9.10); enabled is its data handle. */
typedef struct VisualBody {
    VisualNode *next, *prev;
    void (*RemoveFile)(VisualNode *);
    void (*MoveFile)(VisualNode *, VisualNode *);
    unsigned char unknown10[0x48 - 0x10];
    void *enabled;
} VisualBody;
struct VisualNode { VisualBody *body; };
typedef struct VisualList {
    unsigned char indirect, doubly, unknown02[2];
    u32 count;
    VisualNode *head;
    VisualNode *tail;
} VisualList;
VIEW(VisualList[4], gPS_Files);
#define gPS_Files gPS_Files_view

void SaveFlush(s32 action)
{
    s32 i;
    VisualNode *node;

    for (i = 0; i < 4; i++) {
        for (node = gPS_Files[i].head; node; node = node->body->next) {
            if (node->body->enabled) {
                PS_FlushFile(node, action);
            }
        }
    }
}
