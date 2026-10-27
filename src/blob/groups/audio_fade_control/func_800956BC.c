/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800956BC: walk the singly linked node list whose head is word 3 of
 * D_80146160 and return the first node with key == `key` and kind byte == 3,
 * or NULL.  Also matches at -O2.
 * Shaping quirk: the list header is read through `volatile` -- retail builds
 * the full address (lui/addiu) and loads head with offset 12, which only a
 * volatile (address-form) read produces here.
 */
typedef unsigned char u8;
typedef int s32;
#ifndef TARGET_PC
typedef s32 intptr_t;
#endif
#ifdef TARGET_PC
/* PC: the nodes are audio_fade_control's (group.c): InsertLLE links them through two pointers, prev and next, and
 * the key is the node's entry pointer (+0x18), which the caller passes (docs/port/LAYOUT.md 5). */
typedef struct Node {
    void *word0;
    struct Node *next;
    u8 kind;
    u8 pad[3];
    intptr_t key;
} Node;
#else
typedef struct Node {
    s32 word0;
    struct Node *next;
    u8 kind;
    u8 pad[3];
    s32 key;
} Node;
#endif
typedef struct { s32 words[3]; Node *head; } List;
extern volatile List D_80146160;
Node *func_800956BC(intptr_t key) {
    Node *node;
    for (node = (Node *)D_80146160.head; node != 0; node = node->next) {
        if (node->key == key && node->kind == 3) return node;
    }
    return 0;
}
