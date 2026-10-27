/* port/assets/tests/test_audiomgr.c: the audio manager's records on their PC layouts (docs/port/LAYOUT.md 5), through
 * the game's own code. MAT3 audiomgr.c's FX tags (tFxSequence), sequence tags and messages (tAudioMsg) are linked by
 * InsertLLE through two pointers, so on the PC everything after the links is later than on the N64; the MAT3-typed
 * audio manager (audiomgr_thread.c) has that layout, and these are the other views of the same records:
 * - an FX tag: _GetFxTag (func_80091BA8, the id at +0x18 and a 0x58-byte stride), _InterruptFxTag
 *   (audio_pitch_adjust: the state and the tag's list), _CalcPriority (func_80098710: loop, priority, start time,
 *   volume), the status query results_time_display (the state, the sound id), the 3D-sound position
 *   camera_clip_planes (the tag's data, a source), the parameter change entity_transform_calc and scheduler_recv
 *   (the messages they post), func_80091B00 (the message pool's 0x20-byte stride);
 * - a sequence tag: func_800D18D8 (the free list), car_stats_display (a sequence's start: every field),
 *   game_timer_pause (the lookup, a 0x48-byte stride) and entity_flag_check (the free, at raw offsets);
 * - a 3D-sound source: func_800AED20 (the free list and the active byte);
 * - the fade nodes' lookup func_800956BC (the prev link, the entry pointer as the key);
 * - an ordered list's insertion through pointer_offset8_call (its root at +0x10).
 * game: src/blob/groups/entity_lookup/group.c src/blob/groups/slot18_alloc/func_80091B00.c
 * game: src/blob/groups/transform/group.c src/blob/groups/audio_pitch_adjust/ap.c
 * game: src/blob/func_80091FBC.c src/blob/func_8009211C.c src/blob/groups/record_score/group.c
 * game: src/blob/results_time_display.c src/blob/groups/entity_helpers/group.c
 * game: src/blob/func_800D18D8.c src/blob/car_stats_display.c src/blob/game_timer_pause.c
 * game: src/blob/groups/entity_flag_check/group.c src/blob/func_800AED20.c
 * game: src/blob/groups/audio_fade_control/func_800956BC.c src/blob/pointer_offset8_call.c src/blob/camera_update_b.c
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
struct source {   /* engine_sound_sync.c's Source; the active and queued bytes of audio_effect_setup's Owner */
    struct source *next, *prev;
    uint8_t active, queued, pad[2];
    float position[3];
    float radius, nearVolume, farVolume, parameter;
    int32_t handle;
};
/* audio_fade_control's nodes: it names InsertLLE's next and prev the other way round, and walks from the tail. */
struct fadenode { struct link ll; uint8_t kind, busy, pad[2]; void *entry; float value; };

/* ---------------------------------------------------------------- the game's data */

#define TAGS 4
#define SEQS 4
static struct fxtag g_tags[TAGS];
struct fxtag *D_80110244 = g_tags;                  /* gFxTags */
static struct seqtag g_seqs[SEQS];
struct seqtag *D_80110270 = g_seqs;                 /* the sequence tags */
int32_t D_80146104 = 3;                             /* the handle mask */
uint32_t D_801460F8 = 2, D_80110258 = 5;            /* its width, the next serial */
union audiomsg D_80142DD8[128];                     /* the message pool */
uint8_t D_80142728[64], D_801427A8[64];             /* the lock and the manager's queue */
struct dllist D_80144C50, D_80149808, D_80149860, D_801461B0, D_80146160;
float D_80152748 = 100.0f;                          /* the audio clock */
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
ALIAS(D_80149868, D_80149860, 8);                   /* the active sequences' head */

/* ---------------------------------------------------------------- what the code under test calls */

static void *g_sent[8];
static int g_nsent;
int32_t osRecvMesg(void *mq, void *msg, int32_t flag) { return 0; }
int32_t osSendMesg(void *mq, void *msg, int32_t flag)
{
    if (mq == D_801427A8 && g_nsent < 8)
        g_sent[g_nsent++] = msg;
    return 0;
}
int32_t entity_state_check(int32_t id) { return id == 7; }
int32_t func_800987E8(int32_t k) { return 1; }
void audio_effect_setup(void *tag) {}
uint32_t entity_flags_apply(uint32_t fx, uint32_t sub, uint32_t replace, uint8_t prio) { return 0; }

/* ---------------------------------------------------------------- the game's functions */

struct fxtag *func_80091BA8(int32_t h);
union audiomsg *func_80091B00(void);
void scheduler_recv(int32_t h);
void entity_transform_calc(int32_t h, float x, float y, float z, float w);
void audio_pitch_adjust(struct fxtag *tag);
int32_t func_80098710(struct fxtag *tag);
int32_t results_time_display(int32_t h);
void camera_clip_planes(int32_t h, float *p, intptr_t unused, float a, float b);
struct seqtag *func_800D18D8(void);
uint32_t car_stats_display(int type, float delay, int func, int data, int nargs, ...);
void game_timer_pause(int32_t h);
void entity_flag_check(struct seqtag *seq);
struct source *func_800AED20(void);
struct fadenode *func_800956BC(intptr_t key);
void pointer_offset8_call(void *tree, void *element);
void func_80091FBC(struct dllist *list, void *object, void *before);   /* InsertLLE */

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
    CHECK(func_80091BA8(0x102) == &g_tags[2]);
    CHECK(func_80091BA8(0x203) == NULL && func_80091BA8(-1) == NULL);

    /* _InterruptFxTag: a looping tag that is playing moves to the interrupted tags */
    list_init(&playing);
    list_init(&D_80144C50);
    func_80091FBC(&playing, &g_tags[0], playing.head);
    func_80091FBC(&playing, &g_tags[1], playing.head);
    g_tags[1].list = &playing;
    g_tags[1].state = 2;
    audio_pitch_adjust(&g_tags[1]);
    CHECK(g_tags[1].state == 3 && g_tags[1].list == &D_80144C50);
    CHECK(D_80144C50.head == &g_tags[1] && !in_list(&playing, &g_tags[1]) && playing.head == &g_tags[0]);
    CHECK(g_tags[0].ll.prev == NULL && g_tags[1].id == 0x101);

    /* _CalcPriority: the priority, less for a loud tag, more for a looping one */
    g_tags[2].priority = 10;
    g_tags[2].vol = 0.5f;
    g_tags[2].loop = 1;
    g_tags[2].state = 1;
    g_tags[2].startTime = -1.0f;
    CHECK(func_80098710(&g_tags[2]) == 10 + 40 + 8);
    g_tags[2].loop = 0;
    CHECK(func_80098710(&g_tags[2]) == 10 + 40);

    /* the status query: playing (2) asks the synth about the sound id */
    g_tags[3].state = 1;
    CHECK(results_time_display(0x103) == 1);
    g_tags[3].state = 2;
    g_tags[3].sysId = 7;
    CHECK(results_time_display(0x103) == 1);
    g_tags[3].sysId = 8;
    CHECK(results_time_display(0x103) == 0);

    /* the 3D-sound position: the tag's data is its source */
    memset(&src, 0, sizeof src);
    g_tags[2].data = &src;
    camera_clip_planes(0x102, pos, 0, 0.75f, 9.0f);
    CHECK(src.position[0] == 1.0f && src.position[1] == 2.0f && src.position[2] == 3.0f);
    CHECK(src.nearVolume == 0.75f && src.parameter == 9.0f && src.next == NULL && src.prev == NULL);

    /* a parameter change: a changeEffect message from the pool, the tag's command count */
    memset(D_80142DD8, 0, sizeof D_80142DD8);
    D_80142DD8[0].hdr.used = 1;
    g_nsent = 0;
    g_tags[2].vol = 0.5f, g_tags[2].lrpan = 0.0f, g_tags[2].fbpan = 0.0f, g_tags[2].pitch = 1.0f;
    g_tags[2].numCmds = 0;
    entity_transform_calc(0x102, 0.25f, 0.5f, -2.0f, -2.0f);
    CHECK(g_nsent == 1 && g_sent[0] == &D_80142DD8[1]);
    m = &D_80142DD8[1];
    CHECK(m->hdr.cmd == 4 && m->hdr.type == -1 && m->hdr.used == 1 && m->change.tag == &g_tags[2]);
    CHECK(m->change.vol == 0.25f && m->change.lrpan == 0.5f && m->change.fbpan == -2.0f && m->change.pitch == -2.0f);
    CHECK(g_tags[2].numCmds == 1);
    /* a stop: the tag right after the header */
    scheduler_recv(0x102);
    CHECK(g_nsent == 2 && g_sent[1] == &D_80142DD8[2] && D_80142DD8[2].hdr.cmd == 6);
    CHECK(D_80142DD8[2].intr.tag == &g_tags[2] && g_tags[2].numCmds == 2);
    /* the pool's slots are 0x20 bytes apart */
    CHECK(func_80091B00() == &D_80142DD8[3] && D_80142DD8[3].hdr.used == 1 && D_80142DD8[3].hdr.type == -1);
}

static void sequences(void)
{
    struct seqtag *s;
    uint32_t h;
    int i;

    memset(g_seqs, 0, sizeof g_seqs);
    list_init(&D_80149808);
    list_init(&D_80149860);
    for (i = SEQS - 1; i >= 0; i--) {
        g_seqs[i].id = (uint32_t)i;
        g_seqs[i].free = 1;
        func_80091FBC(&D_80149808, &g_seqs[i], D_80149808.head);
    }
    /* a free tag, its serial in the id above the slot */
    s = func_800D18D8();
    CHECK(s == &g_seqs[0] && s->free == 0 && s->id == (5u << 2) && D_80110258 == 6);
    CHECK(!in_list(&D_80149808, s));
    /* func_800D18D8 is the first call of car_stats_display (a sequence's start) */
    D_80110258 = 9;
    h = car_stats_display(3, 2.5f, 77, 88, 2, 11, 22);
    s = &g_seqs[1];
    CHECK(h == ((9u << 2) | 1u) && s->id == h && s->priority == 3 && s->startTime == 100.0f);
    CHECK(s->timeout == 2.5f && s->replace == 77 && s->subId == 88 && s->stop == 0 && s->next == 0);
    CHECK(s->ids[0] == 11 && s->ids[1] == 22 && s->ids[2] == -1 && s->ids[3] == -1 && s->sndTag == -1);
    CHECK(s->active == 1 && D_80149860.head == s && g_seqs[2].free == 1);
    /* a pause by handle: the tag at handle & mask */
    game_timer_pause((int32_t)h);
    CHECK(s->stop == 1 && g_seqs[0].stop == 0);
    /* the free: the id back to the slot, off the active list, onto the free one */
    entity_flag_check(s);
    CHECK(s->id == 1 && s->active == 0 && s->free == 1 && !in_list(&D_80149860, s) && D_80149808.head == s);
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
    list_init(&D_801461B0);
    func_80091FBC(&D_801461B0, &b, D_801461B0.head);
    func_80091FBC(&D_801461B0, &a, D_801461B0.head);
    a.active = 1;
    CHECK(func_800AED20() == &a && a.active == 0 && D_801461B0.head == &b && b.prev == NULL);

    /* the fade nodes' lookup by their entry pointer */
    memset(n, 0, sizeof n);
    list_init(&D_80146160);
    n[0].kind = 3, n[0].entry = &k1;
    n[1].kind = 2, n[1].entry = &k2;
    n[2].kind = 3, n[2].entry = &k2;
    func_80091FBC(&D_80146160, &n[2], D_80146160.head);
    func_80091FBC(&D_80146160, &n[1], D_80146160.head);
    func_80091FBC(&D_80146160, &n[0], D_80146160.head);
    CHECK(func_800956BC((intptr_t)&k2) == &n[2] && func_800956BC((intptr_t)&k1) == &n[0]);
    CHECK(func_800956BC((intptr_t)&n[1]) == NULL);

    /* an ordered list's insertion: its root after the order function and the count */
    memset(&tree, 0, sizeof tree);
    memset(&e1, 0, sizeof e1);
    pointer_offset8_call(&tree, &e1);
    CHECK(tree.root == &e1 && tree.count == 1);
}

int main(void)
{
    fx_tags();
    sequences();
    sources_fades_trees();
    return TEST_DONE();
}
