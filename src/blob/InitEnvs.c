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

Node gEnvirons[100];

void InitEnvs(void)
{
    int i;

    gFreeVis = gEnvirons;
    for (i = 0; i < 99; i++) {
        gEnvirons[i].next = &gEnvirons[i + 1];
        gEnvirons[i].field6 = -1;
    }
    gEnvirons[i].next = 0;
    gEnvirons[i].field6 = -1;
    gEnvList = 0;
    gEnvCount = 0;
}
