/* port/assets/tests/test_sound.c: the audio manager's records on 64-bit (docs/port/SYNTH.md). The game's own code
 * sets up the pools (AudioInit, AudInit3D), allocates commands (GetCmd), plays effects
 * with and without an emitter (AudPlayEffect, AudEmitEffect: a command, an fx tag and an emitter) and
 * schedules a sequence with variable arguments (AudPlaySequence), then pauses it (stop_sequence). Every record
 * is checked at its PC offsets, which are the host layouts of audiomgr_thread.c's views (tFxSequence, tAudioMsg,
 * anon_ll_id_3c, anon_te_tag_c): an fx tag is 88 bytes, a command 32, a sequence 72, a sorted-tag node 24 and an
 * emitter 56. Before, AudioInit gave the fx tags 80 bytes (the last one's data pointer ran into the next heap
 * block's header), the sorted tags and commands their N64 sizes, the emitter pop cleared a byte of a link, and the
 * sequence's variable arguments were read from the stack as IDO's va_list does; the sorted-tag tree's comparator
 * (OrderFx, _ProcessPriorities' order) read the tag and its priority at their N64 offsets.
 * game: src/blob/groups/save/AudioInit.c src/blob/AudInit3D.c
 * game: src/blob/groups/audio_emitters/group.c src/blob/groups/abort_game/GetCmd.c
 * game: src/blob/NewEmitter.c src/blob/AudPlaySequence.c src/blob/NewSequence.c src/blob/stop_sequence.c
 * game: src/blob/OrderFx.c src/blob/InsertLLE.c src/blob/RemoveLLE.c
 * game: src/blob/GetAudioTime.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_game_types.h"

#include "support.h"

/* ---------------------------------------------------------------- the host layouts the views share */

typedef struct Link { struct Link *next, *prev; } Link;
typedef struct List { uint8_t indirect, doubly, pad[2]; uint32_t count; Link *head, *tail; } List;

typedef struct FxTag {          /* audiomgr_thread.c's tFxSequence */
    Link ll;
    List *list;
    uint32_t id;
    int32_t state, type;
    int8_t paused, loop;
    uint8_t numCmds, priority;
    int32_t wpriority;
    float startTime, vol, lrpan, fbpan, pitch;
    int32_t fxid, subId;
    uint32_t sysId;
    void *data;
} FxTag;
_Static_assert(sizeof(FxTag) == 88 && offsetof(FxTag, id) == 0x18 && offsetof(FxTag, data) == 0x50, "fx tag");

typedef struct Cmd {            /* tAudioMsg's playEffect */
    int16_t type;
    int8_t cmd, used;
    int32_t replace;
    float vol, lrpan, fbpan;
    FxTag *tag;
} Cmd;
_Static_assert(sizeof(Cmd) == 32 && offsetof(Cmd, tag) == 24, "command");

typedef struct Seq {            /* anon_ll_id_3c */
    Link ll;
    uint32_t id;
    int8_t free, active, stop;
    float startTime, timeout;
    uint8_t next;
    int32_t ids[4];
    int32_t subId, replace;
    uint8_t priority;
    int32_t sndTag;
} Seq;
_Static_assert(sizeof(Seq) == 72, "sequence");

typedef struct Emitter {        /* camera_target_track.c's Cam */
    Link link;
    int8_t active, queued;
    uint8_t pad[2];
    float pos[3];
    float volume, x, y, f36;
    uint32_t tag;
} Emitter;
_Static_assert(sizeof(Emitter) == 56, "emitter");

/* ---------------------------------------------------------------- the game's data */

List gFreeFxTags, gPlayingFxTags, gInterruptedFxTags, gFreeEmitters, gActiveEmitters, gFreeListeners, gActiveListeners, gFreeSequences, gActiveSequences;
FxTag *gFxTags;
void *gSortedFxTags, *gListeners;
intptr_t gEmitters;
Seq *gSequences;
int32_t gNumFxTags, gNumStartCmds, gNumStopCmds, gTagShift, gTagMask, gMaxSfx, gMaxMusic, gNumListeners, gNumEmitters;
uint32_t gNextFxId = 1, gNextSeqId = 1;
int8_t gInited_AudioMgr;
struct { void (*func)(void); int32_t unk4; intptr_t unk8; } gSortedFxTagTree;
/* The commands, 32 bytes each on the PC: the PC's pool (pc_game_types.h, docs/port/DECOUPLE.md 9), 128 walked here. */
struct pc_snd_msg pc_snd_msgs[PC_SND_MSG_SLOTS];
#define audiomgr_gCmds ((uint8_t *)pc_snd_msgs)
uint8_t audio_lock_queue[24], audio_command_queue[24], audio_manager_thread[0x1B0], audio_manager_thread_stack_top[16];
void *audio_lock_msg[1];
uint8_t gEffectLoop[64];
float gAudioSeconds = 12.5f, gSoundSpeed, gFrontDistance;
uint8_t gCalcDoppler;

/* ---------------------------------------------------------------- what the code under test calls */

static struct { void *p; uint32_t size; } g_alloc[16];
static int g_nalloc;
static __attribute__((aligned(16))) uint8_t g_heap[0x10000];
static uint32_t g_heap_used;
void *AllocateMemoryHeap(void *heap, uint32_t size)
{
    (void)heap;
    void *p = g_heap + g_heap_used;
    if (g_nalloc == 16 || size > sizeof g_heap - g_heap_used)
        abort();
    g_heap_used += (size + 15) & ~15u;
    memset(p, 0xEE, size);   /* as a fresh block's leftovers: nothing may rely on zeros */
    g_alloc[g_nalloc].p = p, g_alloc[g_nalloc++].size = size;
    return p;
}
/* The list clear (save.c's, which needs much else); AudInit3D.c's copy is weak. */
void ClearLL(List *list)
{
    list->head = list->tail = NULL;
    list->count = 0;
}
void PurgeableMemory(void *p, int32_t purgeable) { (void)p, (void)purgeable; }
static void *g_sent[8];
static int g_nsent;
int32_t osSendMesg(void *mq, void *msg, int32_t flags)
{
    (void)flags;
    if (mq == audio_command_queue && g_nsent < 8)
        g_sent[g_nsent++] = msg;
    return 0;
}
int32_t osRecvMesg(void *mq, void *msg, int32_t flags) { (void)mq, (void)msg, (void)flags; return 0; }
void osCreateMesgQueue(void *mq, void *msg, int32_t count) { (void)mq, (void)msg, (void)count; }
void osCreateThread(void *t, int32_t id, void (*entry)(void *), void *arg, void *sp, int32_t pri)
{
    (void)t, (void)id, (void)entry, (void)arg, (void)sp, (void)pri;
}
void osStartThread(void *t) { (void)t; }
void AudioShutdown(void) {}
void AudioInitSys(int32_t a, int32_t b, int32_t c) { (void)a, (void)b, (void)c; }
void audio_manager_thread_entry(void *arg) { (void)arg; }
int32_t HAL_IntLog2(int32_t n)   /* the highest set bit */
{
    int32_t b = 0;
    while (n >> (b + 1))
        b++;
    return b;
}

void AudioInit(uint8_t a0, uint8_t a1, uint8_t a2, int32_t numA, int32_t numB, float a5, float a6, int32_t a7);
uint32_t AudPlayEffect(uint32_t index, uint32_t other, uint32_t value, uint8_t mode);
int32_t AudEmitEffect(float *pos, void *a1, float volume, float a3, float x, float y, uint32_t index,
                            uint32_t other, uint32_t value, uint8_t mode);
void *GetCmd(void);
uint32_t AudPlaySequence(int type, float delay, int func, int data, int nargs, ...);
void stop_sequence(int32_t h);
int32_t OrderFx(void *a, void *b);   /* the sorted-tag tree's order: by weighted priority */

static int in_pool(const void *p, const void *pool, size_t stride, int n)
{
    uintptr_t a = (uintptr_t)p, b = (uintptr_t)pool;
    return a >= b && a < b + stride * (size_t)n && (a - b) % stride == 0;
}

int main(void)
{
    /* AudioInit: voices = max(min(2 x 16, 64), 16 + 64) = 80 fx tags, as many sorted-tag nodes, 6 sequences; then
     * AudInit3D's 5 records of 0x3C and 64 emitters. */
    AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
    CHECK(gNumFxTags == 80);
    CHECK(g_nalloc == 5);
    CHECK(g_alloc[0].p == gFxTags && g_alloc[0].size == 80 * sizeof(FxTag));
    CHECK(g_alloc[1].p == gSortedFxTags && g_alloc[1].size == 80 * 24);
    CHECK(g_alloc[2].p == gSequences && g_alloc[2].size == 6 * sizeof(Seq));
    CHECK(g_alloc[3].p == gListeners && g_alloc[3].size == 5 * 72);
    CHECK(g_alloc[4].p == (void *)gEmitters && g_alloc[4].size == 64 * sizeof(Emitter));
    CHECK(gFreeFxTags.count == 80 && gFreeSequences.count == 6 && gFreeEmitters.count == 64);
    for (int i = 0; i < 80; i++)
        CHECK(gFxTags[i].list == &gFreeFxTags && (gFxTags[i].id & gTagMask) == (uint32_t)i);

    /* An effect: a command (playEffect, cmd 2) with its fx tag at +24, the tag moved to the active list. */
    uint32_t tag = AudPlayEffect(3, 7, 1, 0);
    CHECK(g_nsent == 1);
    Cmd *cmd = g_sent[0];
    CHECK(in_pool(cmd, audiomgr_gCmds, 32, 128) && cmd->used == 1 && cmd->cmd == 2 && cmd->replace == 1);
    FxTag *t = cmd->tag;
    CHECK(in_pool(t, gFxTags, sizeof(FxTag), 80));
    CHECK(t && t->id == tag && t->list == &gInterruptedFxTags && gInterruptedFxTags.head == &t->ll);
    CHECK(t && t->state == 1 && t->fxid == 3 && t->subId == 7 && t->numCmds == 1 && t->data == NULL);
    CHECK(cmd->vol == 1.0f && cmd->lrpan == 0.0f && cmd->fbpan == -2.0f);

    /* An effect at a position: an emitter taken from the free list (its active flag cleared), queued, and the fx
     * tag's data pointing at it. */
    float pos[3] = {1.0f, 2.0f, 3.0f};
    uint32_t tag2 = (uint32_t)AudEmitEffect(pos, NULL, 0.75f, 0.0f, 0.25f, 2.0f, 4, 8, 0, 1);
    CHECK(g_nsent == 2);
    cmd = g_sent[1];
    t = cmd->tag;
    Emitter *e = t ? t->data : NULL;
    CHECK(in_pool(e, (void *)gEmitters, sizeof(Emitter), 64));
    CHECK(e && e->active == 0 && e->queued == 1 && e->tag == tag2 && t->id == tag2);
    CHECK(e && e->pos[0] == 1.0f && e->pos[2] == 3.0f && e->volume == 0.75f && e->x == 0.25f && e->y == 1.0f);
    CHECK(e && gActiveEmitters.head == &e->link && gFreeEmitters.count == 63);

    /* No fx tag left, as more engines than tags can leave it in a race of 12 cars (docs/port/MORECARS.md 6): each
     * start gives its command back and is not started (-1, no handle), AudEmitEffect its emitter too. */
    List tags = gFreeFxTags;
    gFreeFxTags.head = NULL, gFreeFxTags.tail = NULL, gFreeFxTags.count = 0;
    CHECK(AudPlayEffect(3, 7, 1, 0) == (uint32_t)-1 && g_nsent == 2);
    CHECK(AudEmitEffect(pos, NULL, 0.75f, 0.0f, 0.25f, 2.0f, 4, 8, 0, 1) == -1 && g_nsent == 2);
    CHECK(gFreeEmitters.count == 63 && gActiveEmitters.head == &e->link);
    gFreeFxTags = tags;

    /* The command table: 2 taken (the two not started gave theirs back), the other 126 handed out 32 bytes apart,
     * then none. */
    uint8_t *prev = NULL;
    int n = 0;
    for (void *c; (c = GetCmd()) != NULL; n++) {
        CHECK(in_pool(c, audiomgr_gCmds, 32, 128) && (uint8_t *)c > prev);
        prev = c;
    }
    CHECK(n == 126);

    /* A sequence with three ids: the variable arguments as passed, the rest -1, on the active list; paused by its
     * handle. */
    uint32_t h = AudPlaySequence(2, 0.5f, 0x1234, 0x5678, 3, 11, 22, 33);
    Seq *s = &gSequences[h & gTagMask];
    CHECK(in_pool(s, gSequences, sizeof(Seq), 6) && s->id == h);
    CHECK(s->ids[0] == 11 && s->ids[1] == 22 && s->ids[2] == 33 && s->ids[3] == -1);
    CHECK(s->priority == 2 && s->timeout == 0.5f && s->replace == 0x1234 && s->subId == 0x5678 && s->sndTag == -1);
    CHECK(s->startTime == 12.5f && s->active == 1 && s->stop == 0 && gActiveSequences.head == &s->ll);
    stop_sequence((int32_t)h);
    CHECK(s->stop == 1);

    /* The sorted-tag tree orders its nodes {left, right, tag} by the tags' weighted priority. */
    struct { void *left, *right; FxTag *tag; } na = {0, 0, &gFxTags[5]}, nb = {0, 0, &gFxTags[9]};
    gFxTags[5].wpriority = 70, gFxTags[9].wpriority = 30;
    CHECK(OrderFx(&na, &nb) == 40 && OrderFx(&nb, &na) == -40);
    return TEST_DONE();
}
