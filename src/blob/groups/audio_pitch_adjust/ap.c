typedef signed int s32;
#ifdef TARGET_PC
/* PC: n is an FX tag (MAT3 audiomgr.c's tFxSequence, audiomgr_thread.c): InsertLLE links it through two pointers, so
 * its list (next here) is at +0x10 and its state at +0x1C (docs/port/LAYOUT.md 5). */
typedef struct Node { void *pad0[2]; struct Node *next; s32 pad1[1]; s32 state; } Node;
/* The list header (tDLList) keeps its head at +8: flags and count, then the head and tail pointers. */
typedef struct NodeList { s32 pad0[2]; Node *next; Node *tail; } NodeList;
extern NodeList D_80144C50;
#define TAG_LOOP 0x25   /* the tag's loop flag (N64 +0x19) */
#else
typedef struct Node { s32 pad0[2]; struct Node *next; s32 pad1[1]; s32 state; } Node;
extern Node D_80144C50;
#define TAG_LOOP 25
#endif

extern void func_8009211C(Node *a, Node *b);
extern void func_80091FBC(Node *a, Node *b, Node *c);
extern s32 func_800987E8(s32);
extern void audio_effect_setup(Node *n);
extern s32 D_80140000;

void audio_pitch_adjust(Node *n)
{
    n->state = 3;
    func_8009211C(n->next, n);
    func_80091FBC(&D_80144C50, n, D_80144C50.next);
    n->next = &D_80144C50;
}

/* stand-in callers (real ones: entity_ai_pathfind, func_80098FB8) */
void caller_a(Node *n, s32 k)
{
    while (n) {
        Node *nx = n->next;
        if (func_800987E8(k) == 0) {
            if (*((char *)n + TAG_LOOP)) audio_pitch_adjust(n); else audio_effect_setup(n);
        }
        n = nx;
    }
}

void caller_b(Node *n, s32 k)
{
    Node *h = n;
    while (h) {
        if (func_800987E8(k) == 0) {
            if (*((char *)h + TAG_LOOP)) audio_pitch_adjust(h); else audio_effect_setup(h);
        }
        h = h->next;
    }
}
