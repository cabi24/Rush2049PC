/* AudioInit (MAT3 audiomgr.c): the sound manager's start-up: the sound system (AudioInitSys), the audio manager
 * thread, the effect tags and their lists, the sequences, and the 3D sound (AudInit3D). The manager itself is
 * audiomgr/audiomgr_thread.c. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSMesgQueue { u8 opaque[0x18]; } OSMesgQueue;
typedef struct OSThread { u8 opaque[0x1B0]; } OSThread;

typedef struct Link {       /* MAT3 tDLLE */
    struct Link *next, *prev;
} Link;
typedef struct List {       /* MAT3 tDLList */
    u8 handle, dbl, pad[2];
    u32 numElements;
    Link *head, *tail;
} List;
/* PC: an fx tag (audiomgr_thread.c's tFxSequence, 0x44 bytes on the N64) is 88 bytes: its links, list and data
 * pointer are host-sized. The sorted-tag nodes (anon_te_tag_c: two tree links and the tag) are 24 bytes, and a
 * command (tAudioMsg) 32 (docs/port/SYNTH.md). */
typedef struct Voice {      /* an effect tag (MAT3 tFxSequence) */
    Link link;
    List *owner;            /* 0x10: the list it is in */
    s32 index;              /* 0x18: its id (the tag's number; play counts go above gTagMask) */
    u8 pad1C[0x50 - 0x1C];
    void *data;             /* 0x50 */
} Voice;
#define SORTED_TAG_SIZE 24
typedef struct Chan {       /* a sound sequence (MAT3 anon_ll_id_3c), the PC's layout */
    Link link;
    s32 index;              /* 0x10: its id */
    s8 free;                /* 0x14: in gFreeSequences */
    s8 active;              /* 0x15: in gActiveSequences */
    u8 padE[0x2E];
} Chan;
typedef struct Slot24 { u8 pad0[3]; u8 used; u8 pad4[28]; } Slot24;   /* a command (tAudioMsg): in use */

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(Slot24 (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
VIEW(OSThread, audio_manager_thread);
#define audio_manager_thread audio_manager_thread_view

VIEW(Voice *, gFxTags);
#define gFxTags gFxTags_view
VIEW(void *, gSortedFxTags);
#define gSortedFxTags gSortedFxTags_view

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void osCreateThread(OSThread *thread, s32 id, void (*entry)(void *), void *arg, void *sp, s32 priority);
void osStartThread(OSThread *thread);

/* InitLL (MAT3 ll.c), the sound manager's copy. */
void sound_list_init(List *list, s8 handle, s8 dbl) {
    list->handle = handle;
    list->dbl = dbl;
    ClearLL(list);
}

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
/* Starts or restarts the sound manager with numVoices voices, maxSfx of them for effects and maxMusic for music,
 * numListeners listeners and numEmitters emitters, footUnits units a foot, the front frontDistance away, Doppler
 * or not. A restart with the same effect and music voices does nothing; else the old set-up is shut down first.
 * The thread is made the first time (id 10, priority 10). There are twice as many effect tags as effect voices (64
 * at most), and at least effects plus emitters; the tag mask covers their count. Six sequences.
 * PC: [audio] voices (docs/port/AUDIO.md 10) gives the Expansion Pak's set-up (32 voices: 16 for the effects, 64
 * positional emitters) more voices, the effects all but the songs' 16, and 128 emitters: pc_snd_setup, before
 * anything reads the arguments. The effects' share (maxSfx) is also the sound manager's gMaxSfx (its budget is 3/4
 * of it), the fx tags' count and AudioSfxInit's entries; at original nothing changes. */
void AudioInit(u8 numVoices, u8 maxSfx, u8 maxMusic, s32 numListeners, s32 numEmitters, f32 footUnits, f32 frontDistance, s32 doppler) {
    static s8 queueCreated = 0;
    s32 i;
    s32 reinit;

    reinit = 0; pc_snd_setup(&numVoices, &maxSfx, &numEmitters);
    if (gInited_AudioMgr) {
        reinit = 1;
        if (gMaxSfx == maxSfx && gMaxMusic == maxMusic) {
            return;
        }
        AudioShutdown();
    }
    gMaxSfx = maxSfx;
    gMaxMusic = maxMusic;
    for (i = 0; i < PC_SND_MSG_LOOP(128); i++) {
        audiomgr_gCmds[i].used = 0;
    }
    if (!queueCreated) {
        queueCreated = 1;
        osCreateMesgQueue(&audio_lock_queue, audio_lock_msg, 1);
        osSendMesg(&audio_lock_queue, 0, 0);
    }
    AudioInitSys(numVoices, maxSfx, maxMusic);
    if (!reinit) {
        osCreateThread(&audio_manager_thread, 10, audio_manager_thread_entry, 0, audio_manager_thread_stack_top, 10);
        osStartThread(&audio_manager_thread);
    }
    if (gMaxSfx * 2 < 64) {
        gNumFxTags = gMaxSfx * 2;
    } else {
        gNumFxTags = 64;
    }
    gNumFxTags = (gMaxSfx + numEmitters < gNumFxTags) ? gNumFxTags : gMaxSfx + numEmitters;
    gFxTags = AllocateMemoryHeap(0, gNumFxTags * sizeof(Voice));
    PurgeableMemory(N64A(gFxTags), 0);
    sound_list_init(&gFreeFxTags, 0, 1);
    sound_list_init(&gPlayingFxTags, 0, 1);
    sound_list_init(&gInterruptedFxTags, 0, 1);
    gSortedFxTags = AllocateMemoryHeap(0, gNumFxTags * SORTED_TAG_SIZE);
    PurgeableMemory(N64A(gSortedFxTags), 0);
    gSortedFxTagTree.OrderFunc = OrderFx;
    gSortedFxTagTree.root = 0;
    gSortedFxTagTree.numElements = 0;
    gNumStartCmds = gNumStopCmds = 0;
    for (i = 0; i < gNumFxTags; i++) {
        gFxTags[i].index = i;
        InsertLLE(&gFreeFxTags, &gFxTags[i].link, gFreeFxTags.head);
        gFxTags[i].owner = &gFreeFxTags;
    }
    gTagShift = HAL_IntLog2((u32)gNumFxTags);
    if ((1 << gTagShift) < gNumFxTags) {
        gTagShift++;
    }
    gTagMask = (1 << gTagShift) - 1;
    gSequences = AllocateMemoryHeap(0, 6 * sizeof(Chan));
    PurgeableMemory(N64A(gSequences), 0);
    sound_list_init(&gFreeSequences, 0, 1);
    sound_list_init(&gActiveSequences, 0, 1);
    for (i = 0; i < 6; i++) {
        gSequences[i].index = i;
        gSequences[i].free = 1;
        gSequences[i].active = 0;
        InsertLLE(&gFreeSequences, &gSequences[i].link, gFreeSequences.head);
    }
    AudInit3D(numListeners, numEmitters, footUnits, frontDistance, doppler);
    gInited_AudioMgr = 1;
}
