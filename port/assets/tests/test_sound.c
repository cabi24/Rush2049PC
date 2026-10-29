/* port/assets/tests/test_sound.c: the audio manager's records on 64-bit (docs/port/SYNTH.md). The game's own code
 * sets up the pools (AudioInit = hud_setup, hud_speed_display), allocates commands (func_80091B00), plays effects
 * with and without an emitter (entity_flags_apply, camera_target_track: a command, an fx tag and an emitter) and
 * schedules a sequence with variable arguments (car_stats_display), then pauses it (game_timer_pause). Every record
 * is checked at its PC offsets, which are the host layouts of audiomgr_thread.c's views (tFxSequence, tAudioMsg,
 * anon_ll_id_3c, anon_te_tag_c): an fx tag is 88 bytes, a command 32, a sequence 72, a sorted-tag node 24 and an
 * emitter 56. Before, AudioInit gave the fx tags 80 bytes (the last one's data pointer ran into the next heap
 * block's header), the sorted tags and commands their N64 sizes, the emitter pop cleared a byte of a link, and the
 * sequence's variable arguments were read from the stack as IDO's va_list does; the sorted-tag tree's comparator
 * (draw_sprites, _ProcessPriorities' order) read the tag and its priority at their N64 offsets.
 * game: src/blob/groups/menu_save_options/hud_setup.c src/blob/hud_speed_display.c
 * game: src/blob/groups/camera_target_track/group.c src/blob/groups/slot18_alloc/func_80091B00.c
 * game: src/blob/func_800AED20.c src/blob/car_stats_display.c src/blob/func_800D18D8.c src/blob/game_timer_pause.c
 * game: src/blob/draw_sprites.c src/blob/func_80091FBC.c src/blob/func_8009211C.c
 */
#include <stddef.h>
#include <string.h>

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

List D_80143FF8, D_80144020, D_80144C50, D_801461B0, D_801461E8, D_80146170, D_80146188, D_80149808, D_80149860;
FxTag *D_80110244;
void *D_80110248, *D_8011025C;
intptr_t D_80110260;
Seq *D_80110270;
int32_t D_80110264, D_8011024C, D_80110250, D_801460F8, D_80146104, D_80146200, D_80149410, D_80110268, D_8011026C;
uint32_t D_80110254 = 1, D_80110258 = 1;
int8_t D_8011023C;
struct { void (*func)(void); int32_t unk4; intptr_t unk8; } D_80143F58;
__attribute__((aligned(16))) uint8_t D_80142DD8[128 * 32];   /* the commands: 32 bytes each on the PC */
uint8_t D_80142728[24], D_801427A8[24], D_80034F00[0x1B0], D_800339F0[16];
void *D_80142768[1];
uint8_t D_8011F5CC[64];
float D_80152748 = 12.5f, D_80139318, D_8013C090;
uint8_t D_801497E8;

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
/* The list clear (save.c's, which needs much else); hud_speed_display.c's copy is weak. */
void func_800A370C(List *list)
{
    list->head = list->tail = NULL;
    list->count = 0;
}
void PurgeableMemory(uint32_t p, int32_t purgeable) { (void)p, (void)purgeable; }
static void *g_sent[8];
static int g_nsent;
int32_t osSendMesg(void *mq, void *msg, int32_t flags)
{
    (void)flags;
    if (mq == D_801427A8 && g_nsent < 8)
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
void func_800C8918(void) {}
void car_shadow_render(int32_t a, int32_t b, int32_t c) { (void)a, (void)b, (void)c; }
void func_800992AC(void *arg) { (void)arg; }
void func_8009539C(void) {}
int32_t func_800C8738(int32_t n)   /* the highest set bit */
{
    int32_t b = 0;
    while (n >> (b + 1))
        b++;
    return b;
}

void hud_setup(uint8_t a0, uint8_t a1, uint8_t a2, int32_t numA, int32_t numB, float a5, float a6, int32_t a7);
uint32_t entity_flags_apply(uint32_t index, uint32_t other, uint32_t value, uint8_t mode);
int32_t camera_target_track(float *pos, void *a1, float volume, float a3, float x, float y, uint32_t index,
                            uint32_t other, uint32_t value, uint8_t mode);
void *func_80091B00(void);
uint32_t car_stats_display(int type, float delay, int func, int data, int nargs, ...);
void game_timer_pause(int32_t h);
int32_t draw_sprites(void *a, void *b);   /* the sorted-tag tree's order: by weighted priority */

static int in_pool(const void *p, const void *pool, size_t stride, int n)
{
    uintptr_t a = (uintptr_t)p, b = (uintptr_t)pool;
    return a >= b && a < b + stride * (size_t)n && (a - b) % stride == 0;
}

int main(void)
{
    /* AudioInit: voices = max(min(2 x 16, 64), 16 + 64) = 80 fx tags, as many sorted-tag nodes, 6 sequences; then
     * hud_speed_display's 5 records of 0x3C and 64 emitters. */
    hud_setup(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
    CHECK(D_80110264 == 80);
    CHECK(g_nalloc == 5);
    CHECK(g_alloc[0].p == D_80110244 && g_alloc[0].size == 80 * sizeof(FxTag));
    CHECK(g_alloc[1].p == D_80110248 && g_alloc[1].size == 80 * 24);
    CHECK(g_alloc[2].p == D_80110270 && g_alloc[2].size == 6 * sizeof(Seq));
    CHECK(g_alloc[3].p == D_8011025C && g_alloc[3].size == 5 * 72);
    CHECK(g_alloc[4].p == (void *)D_80110260 && g_alloc[4].size == 64 * sizeof(Emitter));
    CHECK(D_80143FF8.count == 80 && D_80149808.count == 6 && D_801461B0.count == 64);
    for (int i = 0; i < 80; i++)
        CHECK(D_80110244[i].list == &D_80143FF8 && (D_80110244[i].id & D_80146104) == (uint32_t)i);

    /* An effect: a command (playEffect, cmd 2) with its fx tag at +24, the tag moved to the active list. */
    uint32_t tag = entity_flags_apply(3, 7, 1, 0);
    CHECK(g_nsent == 1);
    Cmd *cmd = g_sent[0];
    CHECK(in_pool(cmd, D_80142DD8, 32, 128) && cmd->used == 1 && cmd->cmd == 2 && cmd->replace == 1);
    FxTag *t = cmd->tag;
    CHECK(in_pool(t, D_80110244, sizeof(FxTag), 80));
    CHECK(t && t->id == tag && t->list == &D_80144C50 && D_80144C50.head == &t->ll);
    CHECK(t && t->state == 1 && t->fxid == 3 && t->subId == 7 && t->numCmds == 1 && t->data == NULL);
    CHECK(cmd->vol == 1.0f && cmd->lrpan == 0.0f && cmd->fbpan == -2.0f);

    /* An effect at a position: an emitter taken from the free list (its active flag cleared), queued, and the fx
     * tag's data pointing at it. */
    float pos[3] = {1.0f, 2.0f, 3.0f};
    uint32_t tag2 = (uint32_t)camera_target_track(pos, NULL, 0.75f, 0.0f, 0.25f, 2.0f, 4, 8, 0, 1);
    CHECK(g_nsent == 2);
    cmd = g_sent[1];
    t = cmd->tag;
    Emitter *e = t ? t->data : NULL;
    CHECK(in_pool(e, (void *)D_80110260, sizeof(Emitter), 64));
    CHECK(e && e->active == 0 && e->queued == 1 && e->tag == tag2 && t->id == tag2);
    CHECK(e && e->pos[0] == 1.0f && e->pos[2] == 3.0f && e->volume == 0.75f && e->x == 0.25f && e->y == 1.0f);
    CHECK(e && D_801461E8.head == &e->link && D_801461B0.count == 63);

    /* No fx tag left, as more engines than tags can leave it in a race of 12 cars (docs/port/MORECARS.md 6): each
     * start gives its command back and is not started (-1, no handle), camera_target_track its emitter too. */
    List tags = D_80143FF8;
    D_80143FF8.head = NULL, D_80143FF8.tail = NULL, D_80143FF8.count = 0;
    CHECK(entity_flags_apply(3, 7, 1, 0) == (uint32_t)-1 && g_nsent == 2);
    CHECK(camera_target_track(pos, NULL, 0.75f, 0.0f, 0.25f, 2.0f, 4, 8, 0, 1) == -1 && g_nsent == 2);
    CHECK(D_801461B0.count == 63 && D_801461E8.head == &e->link);
    D_80143FF8 = tags;

    /* The command table: 2 taken (the two not started gave theirs back), the other 126 handed out 32 bytes apart,
     * then none. */
    uint8_t *prev = NULL;
    int n = 0;
    for (void *c; (c = func_80091B00()) != NULL; n++) {
        CHECK(in_pool(c, D_80142DD8, 32, 128) && (uint8_t *)c > prev);
        prev = c;
    }
    CHECK(n == 126);

    /* A sequence with three ids: the variable arguments as passed, the rest -1, on the active list; paused by its
     * handle. */
    uint32_t h = car_stats_display(2, 0.5f, 0x1234, 0x5678, 3, 11, 22, 33);
    Seq *s = &D_80110270[h & D_80146104];
    CHECK(in_pool(s, D_80110270, sizeof(Seq), 6) && s->id == h);
    CHECK(s->ids[0] == 11 && s->ids[1] == 22 && s->ids[2] == 33 && s->ids[3] == -1);
    CHECK(s->priority == 2 && s->timeout == 0.5f && s->replace == 0x1234 && s->subId == 0x5678 && s->sndTag == -1);
    CHECK(s->startTime == 12.5f && s->active == 1 && s->stop == 0 && D_80149860.head == &s->ll);
    game_timer_pause((int32_t)h);
    CHECK(s->stop == 1);

    /* The sorted-tag tree orders its nodes {left, right, tag} by the tags' weighted priority. */
    struct { void *left, *right; FxTag *tag; } na = {0, 0, &D_80110244[5]}, nb = {0, 0, &D_80110244[9]};
    D_80110244[5].wpriority = 70, D_80110244[9].wpriority = 30;
    CHECK(draw_sprites(&na, &nb) == 40 && draw_sprites(&nb, &na) == -40);
    return TEST_DONE();
}
