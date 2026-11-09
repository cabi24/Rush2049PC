#include "game/game.h"

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Node {
    struct Node *next;
    s16 field4;
    s16 field6;
    s16 field8;
    u8 padA[2];
    u32 fieldC;
    f32 field10;
    void *field14;
} Node;

Node *GrabEnvEntry(void)
{
    Node *node;

    if (!gFreeVis) return 0;
    node = gFreeVis;
    gFreeVis = node->next;
    gEnvCount++;
    if (gEnvCount > gEnvCountMax) gEnvCountMax = gEnvCount;
    node->next = 0;
    node->field4 = 0;
    node->field6 = -1;
    node->field8 = 0;
    node->fieldC = 0;
    node->field14 = 0;
    node->field10 = 0.0f;
    return node;
}

/*
 * Pop and initialize a 24-byte free-list node, maintaining the signed-half
 * allocation and high-water counters. The empty list returns null.
 * Arcade equivalent: not established; the arcade checkout is unavailable.
 * N64 adaptation: O32 layout and halfword counter narrowing are target-specific.
 * The source reproduces all 132 target bytes at the flags above. Field names
 * other than next remain neutral; original source spellings are not claimed.
 * Statement order (unlink before increment) is required for matching.
 * This is cloud matching evidence only; image and full-ROM gates remain pending.
 */
