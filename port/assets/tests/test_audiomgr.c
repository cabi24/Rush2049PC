/* port/assets/tests/test_audiomgr.c: the audio manager's records on their PC layouts (docs/port/LAYOUT.md 5), through
 * the game's own code. MAT3 audiomgr.c's FX tags (tFxSequence), sequence tags and messages (tAudioMsg) are linked by
 * InsertLLE through two pointers, so on the PC everything after the links is later than on the N64; the MAT3-typed
 * audio manager (audiomgr_thread.c) has that layout, and these are the other views of the same records:
 * - an FX tag: _GetFxTag (ConvertTag, the id at +0x18 and a 0x58-byte stride), _InterruptFxTag
 *   (InterruptFxTag: the state and the tag's list), _CalcPriority (WeightedPriority: loop, priority, start time,
 *   volume), the status query AudEffectPlaying (the state, the sound id), the 3D-sound position
 *   AudUpdateEmitter (the tag's data, a source), the parameter change AudChangeEffect and AudStopEffect
 *   (the messages they post), GetCmd (the message pool's 0x20-byte stride);
 * - a sequence tag: NewSequence (the free list), AudPlaySequence (a sequence's start: every field),
 *   stop_sequence (the lookup, a 0x48-byte stride) and FreeSequence (the free, at raw offsets);
 * - a 3D-sound source: NewEmitter (the free list and the active byte);
 * - the fade nodes' lookup FindChangeCmd (the prev link, the entry pointer as the key);
 * - an ordered list's insertion through InsertBSTE (its root at +0x10).
 * game: src/blob/groups/aud_stop_effect/group.c src/blob/groups/abort_game/GetCmd.c
 * game: src/blob/groups/transform/group.c src/blob/groups/interrupt_fx_tag/ap.c
 * game: src/blob/InsertLLE.c src/blob/RemoveLLE.c src/blob/groups/weighted_priority/group.c
 * game: src/blob/AudEffectPlaying.c src/blob/groups/audiomgr_emitters/group.c
 * game: src/blob/NewSequence.c src/blob/AudPlaySequence.c src/blob/stop_sequence.c
 * game: src/blob/groups/free_sequence/group.c src/blob/NewEmitter.c
 * game: src/blob/groups/audiosfx/FindChangeCmd.c src/blob/InsertBSTE.c src/blob/InsertBSTEInternal.c
 * game: src/blob/GetAudioTime.c src/blob/AudioTimeoutPerc.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the PC layouts (audiomgr_thread.c's) */

struct link { void *next, *prev; };   /* tDLLE */
struct dllist { int8_t handle, dbl; int32_t count; void *head, *tail; };   /* tDLList */
struct fxtag {   /* tFxSequence */
    struct link ll;
    struct dllist *list;
    uint32_t id;
    int32_t state, type;
    int8_t paused, loop;
    uint8_t numCmds, priority;
    int32_t wpriority;
    float startTime, vol, lrpan, fbpan, pitch;
    int32_t fxid, subId;
    uint32_t sysId;
    void *data;
};
_Static_assert(sizeof(struct fxtag) == 0x58 && offsetof(struct fxtag, id) == 0x18 &&
               offsetof(struct fxtag, state) == 0x1C && offsetof(struct fxtag, numCmds) == 0x26 &&
               offsetof(struct fxtag, startTime) == 0x2C && offsetof(struct fxtag, sysId) == 0x48 &&
               offsetof(struct fxtag, data) == 0x50, "an FX tag's PC layout");
struct seqtag {   /* anon_ll_id_3c */
    struct link ll;
    uint32_t id;
    int8_t free, active, stop;
    float startTime, timeout;
    uint8_t next;
    int32_t ids[4];
    int32_t subId, replace;
    uint8_t priority;
    int32_t sndTag;
};
_Static_assert(sizeof(struct seqtag) == 0x48 && offsetof(struct seqtag, free) == 0x14 &&
               offsetof(struct seqtag, startTime) == 0x18 && offsetof(struct seqtag, ids) == 0x24 &&
               offsetof(struct seqtag, sndTag) == 0x40, "a sequence tag's PC layout");
struct msghdr { int16_t type; char cmd; int8_t used; };
union audiomsg {   /* tAudioMsg */
    struct msghdr hdr;
    struct { struct msghdr hdr; float vol, lrpan, fbpan, pitch; struct fxtag *tag; } change;   /* changeEffect */
    struct { struct msghdr hdr; struct fxtag *tag; } intr;                                      /* intEffect */
};
_Static_assert(sizeof(union audiomsg) == 0x20 && offsetof(union audiomsg, change.tag) == 0x18 &&
               offsetof(union audiomsg, intr.tag) == 8, "a message's PC layout");
struct source {   /* ProcessEmitters.c's Source; the active and queued bytes of FreeFxTag's Owner */
    struct source *next, *prev;
    uint8_t active, queued, pad[2];
    float position[3];
    float radius, nearVolume, farVolume, parameter;
    int32_t handle;
};
/* SendCmd's nodes: it names InsertLLE's next and prev the other way round, and walks from the tail. */
struct fadenode { struct link ll; uint8_t kind, busy, pad[2]; void *entry; float value; };

/* ---------------------------------------------------------------- the game's data */

#define TAGS 4
#define SEQS 4
static struct fxtag g_tags[TAGS];
struct fxtag *gFxTags = g_tags;                  /* gFxTags */
static struct seqtag g_seqs[SEQS];
struct seqtag *gSequences = g_seqs;                 /* the sequence tags */
int32_t gTagMask = 3;                             /* the handle mask */
uint32_t gTagShift = 2, gNextSeqId = 5;            /* its width, the next serial */
struct pc_snd_msg pc_snd_msgs[PC_SND_MSG_SLOTS];    /* the message pool (pc_game_types.h): 128 walked here */
#define audiomgr_gCmds (*(union audiomsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
uint8_t audio_lock_queue[64], audio_command_queue[64];             /* the lock and the manager's queue */
struct dllist gInterruptedFxTags, gFreeSequences, gActiveSequences, gFreeEmitters, gCmdQ;
float gAudioSeconds = 100.0f;                          /* the audio clock */
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
ALIAS(active_sequences_head, gActiveSequences, 8);                   /* the active sequences' head */

/* ---------------------------------------------------------------- what the code under test calls */

static void *g_sent[8];
static int g_nsent, g_queue_full;
int32_t osRecvMesg(void *mq, void *msg, int32_t flag) { return 0; }
int32_t osSendMesg(void *mq, void *msg, int32_t flag)
{
    if (mq == audio_command_queue && g_queue_full)
        return -1;   /* OS_MESG_NOBLOCK on a full queue */
    if (mq == audio_command_queue && g_nsent < 8)
        g_sent[g_nsent++] = msg;
    return 0;
}
int32_t AudioEffectPlaying(int32_t id) { return id == 7; }
void FreeFxTag(void *tag) {}
uint32_t AudPlayEffect(uint32_t fx, uint32_t sub, uint32_t replace, uint8_t prio) { return 0; }

/* ---------------------------------------------------------------- the game's functions */

struct fxtag *ConvertTag(int32_t h);
union audiomsg *GetCmd(void);
void AudStopEffect(int32_t h);
void AudChangeEffect(int32_t h, float x, float y, float z, float w);
void InterruptFxTag(struct fxtag *tag);
int32_t WeightedPriority(struct fxtag *tag);
int32_t AudEffectPlaying(int32_t h);
void AudUpdateEmitter(int32_t h, float *p, intptr_t unused, float a, float b);
struct seqtag *NewSequence(void);
uint32_t AudPlaySequence(int type, float delay, int func, int data, int nargs, ...);
void stop_sequence(int32_t h);
void FreeSequence(struct seqtag *seq);
struct source *NewEmitter(void);
struct fadenode *FindChangeCmd(intptr_t key);
void InsertBSTE(void *tree, void *element);
void InsertLLE(struct dllist *list, void *object, void *before);

static void list_init(struct dllist *l) { memset(l, 0, sizeof *l); l->dbl = 1; }

static int in_list(struct dllist *l, void *x)
{
    struct link *n;
    for (n = l->head; n; n = n->next)
        if (n == x)
            return 1;
    return 0;
}

static void fx_tags(void)
{
    struct dllist playing;
    struct source src;
    float pos[3] = {1.0f, 2.0f, 3.0f};
    union audiomsg *m;
    int i;

    memset(g_tags, 0, sizeof g_tags);
    for (i = 0; i < TAGS; i++)
        g_tags[i].id = 0x100u + (uint32_t)i;
    /* _GetFxTag: the tag at handle & mask whose id is the handle */
    CHECK(ConvertTag(0x102) == &g_tags[2]);
    CHECK(ConvertTag(0x203) == NULL && ConvertTag(-1) == NULL);

    /* _InterruptFxTag: a looping tag that is playing moves to the interrupted tags */
    list_init(&playing);
    list_init(&gInterruptedFxTags);
    InsertLLE(&playing, &g_tags[0], playing.head);
    InsertLLE(&playing, &g_tags[1], playing.head);
    g_tags[1].list = &playing;
    g_tags[1].state = 2;
    InterruptFxTag(&g_tags[1]);
    CHECK(g_tags[1].state == 3 && g_tags[1].list == &gInterruptedFxTags);
    CHECK(gInterruptedFxTags.head == &g_tags[1] && !in_list(&playing, &g_tags[1]) && playing.head == &g_tags[0]);
    CHECK(g_tags[0].ll.prev == NULL && g_tags[1].id == 0x101);

    /* _CalcPriority: the priority, less for a loud tag, more for a looping one */
    g_tags[2].priority = 10;
    g_tags[2].vol = 0.5f;
    g_tags[2].loop = 1;
    g_tags[2].state = 1;
    g_tags[2].startTime = -1.0f;
    CHECK(WeightedPriority(&g_tags[2]) == 10 + 40 + 8);
    g_tags[2].loop = 0;
    CHECK(WeightedPriority(&g_tags[2]) == 10 + 40);

    /* the status query: playing (2) asks the synth about the sound id */
    g_tags[3].state = 1;
    CHECK(AudEffectPlaying(0x103) == 1);
    g_tags[3].state = 2;
    g_tags[3].sysId = 7;
    CHECK(AudEffectPlaying(0x103) == 1);
    g_tags[3].sysId = 8;
    CHECK(AudEffectPlaying(0x103) == 0);

    /* the 3D-sound position: the tag's data is its source */
    memset(&src, 0, sizeof src);
    g_tags[2].data = &src;
    AudUpdateEmitter(0x102, pos, 0, 0.75f, 9.0f);
    CHECK(src.position[0] == 1.0f && src.position[1] == 2.0f && src.position[2] == 3.0f);
    CHECK(src.nearVolume == 0.75f && src.parameter == 9.0f && src.next == NULL && src.prev == NULL);

    /* a parameter change: a changeEffect message from the pool, the tag's command count */
    memset(audiomgr_gCmds, 0, sizeof audiomgr_gCmds);
    audiomgr_gCmds[0].hdr.used = 1;
    g_nsent = 0;
    g_tags[2].vol = 0.5f, g_tags[2].lrpan = 0.0f, g_tags[2].fbpan = 0.0f, g_tags[2].pitch = 1.0f;
    g_tags[2].numCmds = 0;
    AudChangeEffect(0x102, 0.25f, 0.5f, -2.0f, -2.0f);
    CHECK(g_nsent == 1 && g_sent[0] == &audiomgr_gCmds[1]);
    m = &audiomgr_gCmds[1];
    CHECK(m->hdr.cmd == 4 && m->hdr.type == -1 && m->hdr.used == 1 && m->change.tag == &g_tags[2]);
    CHECK(m->change.vol == 0.25f && m->change.lrpan == 0.5f && m->change.fbpan == -2.0f && m->change.pitch == -2.0f);
    CHECK(g_tags[2].numCmds == 1);
    /* a stop: the tag right after the header */
    AudStopEffect(0x102);
    CHECK(g_nsent == 2 && g_sent[1] == &audiomgr_gCmds[2] && audiomgr_gCmds[2].hdr.cmd == 6);
    CHECK(audiomgr_gCmds[2].intr.tag == &g_tags[2] && g_tags[2].numCmds == 2);
    /* the pool's slots are 0x20 bytes apart */
    CHECK(GetCmd() == &audiomgr_gCmds[3] && audiomgr_gCmds[3].hdr.used == 1 && audiomgr_gCmds[3].hdr.type == -1);

    /* The pool and the queue running out (docs/port/QA.md "Integration soak": a retail race faulted at address 2
     * when the manager thread fell behind): a change or a stop with every message in use is dropped, and one the
     * full queue refuses gives its message back, the tag's command count as it was. */
    for (int i = 0; i < 128; i++)
        audiomgr_gCmds[i].hdr.used = 1;
    g_nsent = 0;
    g_tags[2].numCmds = 0;
    AudChangeEffect(0x102, 0.75f, -2.0f, -2.0f, -2.0f);
    AudStopEffect(0x102);
    CHECK(g_nsent == 0 && g_tags[2].numCmds == 0);
    audiomgr_gCmds[77].hdr.used = 0;
    g_queue_full = 1;
    AudChangeEffect(0x102, 0.75f, -2.0f, -2.0f, -2.0f);
    CHECK(g_nsent == 0 && g_tags[2].numCmds == 0 && audiomgr_gCmds[77].hdr.used == 0);
    AudStopEffect(0x102);
    CHECK(g_nsent == 0 && g_tags[2].numCmds == 0 && audiomgr_gCmds[77].hdr.used == 0);
    g_queue_full = 0;
    AudChangeEffect(0x102, 0.75f, -2.0f, -2.0f, -2.0f);
    CHECK(g_nsent == 1 && g_sent[0] == &audiomgr_gCmds[77] && g_tags[2].numCmds == 1 && audiomgr_gCmds[77].hdr.used == 1);
}

static void sequences(void)
{
    struct seqtag *s;
    uint32_t h;
    int i;

    memset(g_seqs, 0, sizeof g_seqs);
    list_init(&gFreeSequences);
    list_init(&gActiveSequences);
    for (i = SEQS - 1; i >= 0; i--) {
        g_seqs[i].id = (uint32_t)i;
        g_seqs[i].free = 1;
        InsertLLE(&gFreeSequences, &g_seqs[i], gFreeSequences.head);
    }
    /* a free tag, its serial in the id above the slot */
    s = NewSequence();
    CHECK(s == &g_seqs[0] && s->free == 0 && s->id == (5u << 2) && gNextSeqId == 6);
    CHECK(!in_list(&gFreeSequences, s));
    /* NewSequence is the first call of AudPlaySequence (a sequence's start) */
    gNextSeqId = 9;
    h = AudPlaySequence(3, 2.5f, 77, 88, 2, 11, 22);
    s = &g_seqs[1];
    CHECK(h == ((9u << 2) | 1u) && s->id == h && s->priority == 3 && s->startTime == 100.0f);
    CHECK(s->timeout == 2.5f && s->replace == 77 && s->subId == 88 && s->stop == 0 && s->next == 0);
    CHECK(s->ids[0] == 11 && s->ids[1] == 22 && s->ids[2] == -1 && s->ids[3] == -1 && s->sndTag == -1);
    CHECK(s->active == 1 && gActiveSequences.head == s && g_seqs[2].free == 1);
    /* a pause by handle: the tag at handle & mask */
    stop_sequence((int32_t)h);
    CHECK(s->stop == 1 && g_seqs[0].stop == 0);
    /* the free: the id back to the slot, off the active list, onto the free one */
    FreeSequence(s);
    CHECK(s->id == 1 && s->active == 0 && s->free == 1 && !in_list(&gActiveSequences, s) && gFreeSequences.head == s);
}

static void sources_fades_trees(void)
{
    struct source a, b;
    struct fadenode n[3];
    int k1, k2;
    struct { int32_t (*cmp)(void *, void *); int32_t count; void *root; } tree;
    struct { void *left, *right; } e1;

    /* _GetSource: the head of the free sources, its active byte cleared */
    memset(&a, 0, sizeof a), memset(&b, 0, sizeof b);
    list_init(&gFreeEmitters);
    InsertLLE(&gFreeEmitters, &b, gFreeEmitters.head);
    InsertLLE(&gFreeEmitters, &a, gFreeEmitters.head);
    a.active = 1;
    CHECK(NewEmitter() == &a && a.active == 0 && gFreeEmitters.head == &b && b.prev == NULL);

    /* the fade nodes' lookup by their entry pointer */
    memset(n, 0, sizeof n);
    list_init(&gCmdQ);
    n[0].kind = 3, n[0].entry = &k1;
    n[1].kind = 2, n[1].entry = &k2;
    n[2].kind = 3, n[2].entry = &k2;
    InsertLLE(&gCmdQ, &n[2], gCmdQ.head);
    InsertLLE(&gCmdQ, &n[1], gCmdQ.head);
    InsertLLE(&gCmdQ, &n[0], gCmdQ.head);
    CHECK(FindChangeCmd((intptr_t)&k2) == &n[2] && FindChangeCmd((intptr_t)&k1) == &n[0]);
    CHECK(FindChangeCmd((intptr_t)&n[1]) == NULL);

    /* an ordered list's insertion: its root after the order function and the count */
    memset(&tree, 0, sizeof tree);
    memset(&e1, 0, sizeof e1);
    InsertBSTE(&tree, &e1);
    CHECK(tree.root == &e1 && tree.count == 1);
}

int main(void)
{
    fx_tags();
    sequences();
    sources_fades_trees();
    return TEST_DONE();
}
