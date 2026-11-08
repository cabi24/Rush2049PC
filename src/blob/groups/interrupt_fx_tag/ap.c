typedef signed int s32;
/* PC: an fx tag (audiomgr_thread.c's tFxSequence) has host-sized links, so its list (`next` here) is at +0x10 and its
 * state at +0x1C; the list header gInterruptedFxTags (tDLList) keeps its head at +8 (docs/port/SYNTH.md). */
typedef struct Node { void *pad0[2]; struct Node *next; s32 pad1[1]; s32 state; } Node;
typedef struct NodeList { s32 pad0[2]; Node *head; } NodeList;

extern Node gInterruptedFxTags;
extern void RemoveLLE(Node *a, Node *b);
extern void InsertLLE(Node *a, Node *b, Node *c);
extern s32 AudioEffectPlaying(s32);
extern void FreeFxTag(Node *n);
extern s32 D_80140000;

void InterruptFxTag(Node *n)
{
    n->state = 3;
    RemoveLLE(n->next, n);
    InsertLLE(&gInterruptedFxTags, n, ((NodeList *)&gInterruptedFxTags)->head);
    n->next = &gInterruptedFxTags;
}

/* stand-in callers (real ones: audio_manager_thread_entry, PeriodicMaintenance) */

