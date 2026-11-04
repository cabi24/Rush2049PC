/* port/assets/tests/test_mgrstart.c: the audio manager's thread (MAT3 audiomgr.c's _AudioThread, entity_ai_pathfind)
 * on the starts its priority pass sends (docs/port/AUDIO.md 10.6). The pass runs on the thread at a retrace's message
 * and queues its starts behind it; when the host holds the thread back by a retrace, two retrace messages are queued
 * and the second pass sends a waiting tag's start again. The thread must start the tag once: the second start, for a
 * tag playing already, starts nothing (on the PC: the N64's thread never sees one), so the first voice isn't lost
 * playing on. A start for a waiting tag starts it, a start for a freed one frees it, and a stop and a start again
 * start it twice. The thread loops for ever: the test's osRecvMesg feeds it a script and leaves at its end. Made-up
 * values only.
 * game: src/blob/groups/engine_sound_sync/audiomgr_thread.c
 */
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pc_game_types.h"

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
_Static_assert(sizeof(struct fxtag) == 0x58 && offsetof(struct fxtag, state) == 0x1C &&
               offsetof(struct fxtag, sysId) == 0x48, "an FX tag's PC layout");
struct msghdr { int16_t type; char cmd; int8_t used; };
union audiomsg {   /* tAudioMsg */
    struct msghdr hdr;
    struct { struct msghdr hdr; struct fxtag *tag; } tag;   /* startEffect, intEffect, stopEffect */
    unsigned char pad[32];
};
_Static_assert(sizeof(union audiomsg) == 0x20 && offsetof(union audiomsg, tag.tag) == 8, "a message's PC layout");

/* ---------------------------------------------------------------- the game's data the thread names */

uint8_t D_8002E8E8[64];                               /* the scheduler */
int8_t D_8011023C;                                    /* gInited */
int32_t D_80110240 = -1;                              /* gSong */
void *D_80110248;                                     /* gSortedFxTags */
int32_t D_8011024C, D_80110250, D_80110278, D_8011027C, D_80110280;
uint8_t D_80142728[64], D_801427A8[64];               /* the lock and the manager's queue */
void *D_80142B70[PC_SND_MSG_SLOTS];                   /* the queue's messages */
uint8_t D_80143F58[32];                               /* gSortedFxTagTree */
struct dllist D_80144020, D_80144C50, D_80149860;     /* gPlayingFxTags, gInterruptedFxTags, gActiveSequences */
float D_80152748 = 7.0f;                              /* the audio clock */
void *pc_snd_start_cmds[PC_SND_MSG_SLOTS], *pc_snd_stop_cmds[PC_SND_MSG_SLOTS];

/* ---------------------------------------------------------------- what the thread calls */

static int g_starts, g_stops, g_freed;
static uint32_t g_next_id = 0x100;
int audio_state_save(int fxid, float vol, float lrpan) { (void)fxid, (void)vol, (void)lrpan; g_starts++; return (int)g_next_id++; }
void audio_timing_sync(unsigned int sysId) { (void)sysId; g_stops++; }
void audio_effect_setup(struct fxtag *tag) { g_freed++; tag->state = 0; }
void audio_pitch_adjust(struct fxtag *tag) { tag->state = 3; }
void audio_buffer_manage(unsigned int sysId, float vol) { (void)sysId, (void)vol; }
void audio_stream_control(unsigned int sysId, float lrpan) { (void)sysId, (void)lrpan; }
void sound_priority_set(unsigned int sysId, float fbpan) { (void)sysId, (void)fbpan; }
void audio_bus_route(unsigned int sysId, float pitch) { (void)sysId, (void)pitch; }
void func_800958B8(void) {}
void func_80096238(int song) { (void)song; }
void func_800979A0(int song, int loop) { (void)song, (void)loop; }
void music_track_control(float vol, unsigned int time, int music, int sfx) { (void)vol, (void)time, (void)music, (void)sfx; }
void gfx_setup_e700(int mono) { (void)mono; }
void func_8009211C(struct dllist *list, void *el) { (void)list, (void)el; }
void func_80091FBC(struct dllist *list, void *el, void *before) { (void)list, (void)el, (void)before; }
int func_80098710(struct fxtag *tag) { (void)tag; return 0; }
void camera_update_b(void *tree, void *root, void *element) { (void)tree, (void)root, (void)element; }
void camera_update_a(void *tree, void *root, int *num, void (*walk)(void *, int)) { (void)tree, (void)root, (void)num, (void)walk; }
void engine_sound_sync(void) {}
void func_800988D8(void) {}
void camera_transform(void) {}
void sound_position_update(void *element, int elNum) { (void)element, (void)elNum; }
int entity_state_check(unsigned int sysId) { (void)sysId; return 1; }
void osCreateMesgQueue(void *q, void **msgs, int32_t count) { (void)q, (void)msgs, (void)count; }
void osScAddClient(void *sc, void *client, void *q) { (void)sc, (void)client, (void)q; }
int32_t osSendMesg(void *q, void *msg, int32_t flags) { (void)q, (void)msg, (void)flags; return 0; }

/* The script: the messages the queue gives the thread, in order; past its end the thread is left. */
static void *g_script[8];
static int g_at, g_len;
static jmp_buf g_out;
int32_t osRecvMesg(void *q, void **msg, int32_t flags)
{
    (void)flags;
    if (q != D_801427A8)
        return 0;   /* the lock */
    if (g_at == g_len)
        longjmp(g_out, 1);
    *msg = g_script[g_at++];
    return 0;
}
void entity_ai_pathfind(void *arg);

static union audiomsg g_msgs[8];
static struct fxtag g_tag;

static void run(const int *cmds, int n)
{
    memset(g_msgs, 0, sizeof g_msgs);
    for (int i = 0; i < n; i++) {
        g_msgs[i].hdr.type = 0;
        g_msgs[i].hdr.cmd = (char)cmds[i];
        g_msgs[i].hdr.used = 1;
        g_msgs[i].tag.tag = &g_tag;
        g_script[i] = &g_msgs[i];
        g_tag.numCmds++;
    }
    g_at = 0, g_len = n;
    g_starts = g_stops = g_freed = 0;
    memset(test_snd_count, 0, sizeof(unsigned long long) * PC_SND_COUNTERS);
    if (!setjmp(g_out))
        entity_ai_pathfind(NULL);
}

static void waiting(void)
{
    memset(&g_tag, 0, sizeof g_tag);
    g_tag.state = 3;
    g_tag.vol = 0.8f, g_tag.pitch = 1.0f, g_tag.loop = 1, g_tag.fxid = 5;
}

int main(void)
{
    /* One start: the tag plays. */
    waiting();
    run((const int[]){3}, 1);
    CHECK(g_starts == 1 && g_tag.state == 2 && g_tag.sysId == 0x100 && g_tag.numCmds == 0);

    /* Two starts queued for the same waiting tag (two passes before the thread caught up): one voice, the first
     * kept, the second start counted and dropped. */
    waiting();
    g_next_id = 0x200;
    run((const int[]){3, 3}, 2);
    CHECK(g_starts == 1 && g_tag.state == 2 && g_tag.sysId == 0x200 && g_tag.numCmds == 0);
    CHECK(test_snd_count[PC_SND_START_PLAYING] == 1);

    /* Three: still one. */
    waiting();
    g_next_id = 0x300;
    run((const int[]){3, 3, 3}, 3);
    CHECK(g_starts == 1 && g_tag.sysId == 0x300 && test_snd_count[PC_SND_START_PLAYING] == 2 && g_tag.numCmds == 0);

    /* A start, the pass's interrupt (5: stopped, waiting again), a start: two voices, one after the other. */
    waiting();
    g_next_id = 0x400;
    run((const int[]){3, 5, 3}, 3);
    CHECK(g_starts == 2 && g_stops == 1 && g_tag.state == 2 && g_tag.sysId == 0x401);
    CHECK(test_snd_count[PC_SND_START_PLAYING] == 0);

    /* A start for a freed tag (state 0) frees it and starts nothing, as on the N64. */
    waiting();
    g_tag.state = 0;
    run((const int[]){3}, 1);
    CHECK(g_starts == 0 && g_freed == 1 && test_snd_count[PC_SND_START_PLAYING] == 0);

    /* A start and the game's stop (6): one voice, stopped and freed. */
    waiting();
    g_next_id = 0x500;
    run((const int[]){3, 6}, 2);
    CHECK(g_starts == 1 && g_stops == 1 && g_freed == 1 && g_tag.state == 0);
    if (!test_failures)
        printf("assets_mgrstart: ok\n");
    return TEST_DONE();
}
