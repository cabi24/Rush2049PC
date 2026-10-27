/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Traverse the four N64 indirect lists at D_80144D60. For every node whose
 * body word at +72 is nonzero, call the existing operation with action, then
 * reload the body/next link so operation-driven list changes are observed.
 * No direct Rush The Rock ancestor was found. The indirect List layout is
 * corroborated by accepted func_80091FBC and func_8009211C; historical function
 * names do not establish renderer or physics semantics. Ordinary O2/O3.
 */
typedef signed int s32;
typedef unsigned int u32;
typedef struct VisualNode VisualNode;
#ifdef TARGET_PC
/* PC: paksys' tPS_File (docs/port/POINTERS.md 9.10); enabled is its data handle. */
typedef struct VisualBody {
    VisualNode *next, *prev;
    void (*RemoveFile)(VisualNode *);
    void (*MoveFile)(VisualNode *, VisualNode *);
    unsigned char unknown10[0x48 - 0x10];
    void *enabled;
} VisualBody;
#else
typedef struct VisualBody {
    VisualNode *next;
    unsigned char unknown04[68];
    u32 enabled;
} VisualBody;
#endif
struct VisualNode { VisualBody *body; };
typedef struct VisualList {
    unsigned char indirect, doubly, unknown02[2];
    u32 count;
    VisualNode *head;
    VisualNode *tail;
} VisualList;
extern VisualList D_80144D60[4];
s32 PS_FlushFile(VisualNode *, s32);

void SaveFlush(s32 action)
{
    s32 i;
    VisualNode *node;

    for (i = 0; i < 4; i++) {
        for (node = D_80144D60[i].head; node; node = node->body->next) {
            if (node->body->enabled) {
                PS_FlushFile(node, action);
            }
        }
    }
}
