/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Initialize the 100-node free list and clear the active head and allocation count.
 * Native layout and ownership are supported by the initializer and the real
 * GrabEnvEntry consumer. No whole-function arcade donor is established.
 * Defining the actual pool array allows IDO to combine its symbol references;
 * the indexed traversal and next-pointer-before-id order follow native effects.
 * BSS address/extent and compatible consumer context are checked separately.
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned int u32;
typedef float f32;

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
extern Node *gFreeVis;
extern Node *gEnvList;
extern s16 gEnvCount;

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
