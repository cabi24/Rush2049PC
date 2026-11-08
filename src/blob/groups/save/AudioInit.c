/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudioInit (historical label), 0x800C8B8C, 1,048 bytes: N64 sound-system
 * initialization. Extends SaveInit to reuse its genuine locked
 * ClearLL list-clear body; no alternate copy or stand-in is installed.
 *
 * Disclosed source-shape choices:
 * - A function-local `static s8 queueCreated = 0` owns the existing initialized
 *   once flag at 0x80110274. Its four-byte .data verification window is zero; the logical flag is one byte.
 * - The count passed to the existing s32 bit-scan helper is explicitly
 *   converted to u32; the function prototype is unchanged. This code-free
 *   integer conversion changes uopt allocation, without a new access or use.
 * - sound_list_init is the preexisting inferred nested list initializer,
 *   not an authenticated donor identity. It contains the real flag stores
 *   and calls the genuine list clear. Non-static/internal avoids an anonymous
 *   deleted-procedure tail. A named, unclaimed 8-byte empty-return residue
 *   remains and is not spliced. No unused locals, fake calls or volatile additions.
 * - Allocation/count-guard prototypes match their locked source contracts.
 * See, replay.py, and verification.json for controlscr_text/proof.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef void *OSMesg;
typedef struct OSMesgQueue { u8 opaque[0x18]; } OSMesgQueue;
typedef struct OSThread { u8 opaque[0x1B0]; } OSThread;

typedef struct Link {
    struct Link *next, *prev;
} Link;
typedef struct List {
    u8 indirect, doubly, pad[2];
    u32 count;
    Link *head, *tail;
} List;
/* PC: an fx tag (audiomgr_thread.c's tFxSequence, 0x44 bytes on the N64) is 88 bytes: its links, list and data
 * pointer are host-sized. The sorted-tag nodes (anon_te_tag_c: two tree links and the tag) are 24 bytes, and a
 * command (tAudioMsg) 32 (docs/port/SYNTH.md). */
typedef struct Voice {
    Link link;
    List *owner;            /* 0x10 */
    s32 index;              /* 0x18 */
    u8 pad1C[0x50 - 0x1C];
    void *data;             /* 0x50 */
} Voice;
#define SORTED_TAG_SIZE 24
typedef struct Chan {       /* 0x3C */
    Link link;
    s32 index;              /* 0x08 */
    s8 unkC;
    s8 unkD;
    u8 padE[0x2E];
} Chan;
typedef struct Slot24 { u8 pad0[3]; u8 active; u8 pad4[28]; } Slot24;
typedef struct Handler { void (*func)(void); s32 unk4; intptr_t unk8; } Handler;   /* tOrderedList: unk8 is its root */

extern s8 gInited_AudioMgr;

extern s32 gMaxSfx;
extern s32 gMaxMusic;
/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(Slot24 (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
extern OSMesgQueue audio_lock_queue;
extern OSMesg audio_lock_msg[1];
extern OSThread audio_manager_thread;
extern u8 audio_manager_thread_stack_top[];
extern s32 gNumFxTags;
extern Voice *gFxTags;
extern void *gSortedFxTags;
extern s32 gNumStartCmds;
extern s32 gNumStopCmds;
extern Handler gSortedFxTagTree;
extern List gFreeFxTags, gPlayingFxTags, gInterruptedFxTags;
extern s32 gTagShift;
extern s32 gTagMask;
extern Chan *gSequences;
extern List gFreeSequences, gActiveSequences;

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void osCreateThread(OSThread *thread, s32 id, void (*entry)(void *), void *arg, void *sp, s32 priority);
void osStartThread(OSThread *thread);
void AudioShutdown(void);
void AudioInitSys(s32 arg0, s32 arg1, s32 arg2);
void audio_manager_thread_entry(void *arg);
void OrderFx(void);
typedef struct Heap Heap;
void *AllocateMemoryHeap(Heap *arg0, u32 arg1);
void PurgeableMemory(u32 arg0, s32 arg1);
void InsertLLE(List *list, Link *object, Link *before);
s32 HAL_IntLog2(s32 arg0);
void AudInit3D(s32 numA, s32 numB, f32 arg2, f32 arg3, s32 arg4);

void ClearLL(List *list);

void sound_list_init(List *list, s8 indirect, s8 doubly) {
    list->indirect = indirect;
    list->doubly = doubly;
    ClearLL(list);
}

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
/* PC: [audio] voices (docs/port/AUDIO.md 10) gives the Expansion Pak's set-up (32 voices: 16 for the effects, 64
 * positional emitters) more voices, the effects all but the songs' 16, and 128 emitters: pc_snd_setup, before
 * anything reads the arguments. The effects' share (arg1) is also the sound manager's gMaxSfx (gMaxSfx: its
 * budget is 3/4 of it), the fx tags' count and AudioSfxInit's entries; at original nothing changes. */
void AudioInit(u8 arg0, u8 arg1, u8 arg2, s32 numA, s32 numB, f32 arg5, f32 arg6, s32 arg7) {
    static s8 queueCreated = 0;
    s32 i;
    s32 reinit;

    reinit = 0; pc_snd_setup(&arg0, &arg1, &numB);
    if (gInited_AudioMgr) {
        reinit = 1;
        if (gMaxSfx == arg1 && gMaxMusic == arg2) {
            return;
        }
        AudioShutdown();
    }
    gMaxSfx = arg1;
    gMaxMusic = arg2;
    for (i = 0; i < PC_SND_MSG_LOOP(128); i++) {
        audiomgr_gCmds[i].active = 0;
    }
    if (!queueCreated) {
        queueCreated = 1;
        osCreateMesgQueue(&audio_lock_queue, audio_lock_msg, 1);
        osSendMesg(&audio_lock_queue, 0, 0);
    }
    AudioInitSys(arg0, arg1, arg2);
    if (!reinit) {
        osCreateThread(&audio_manager_thread, 10, audio_manager_thread_entry, 0, audio_manager_thread_stack_top, 10);
        osStartThread(&audio_manager_thread);
    }
    if (gMaxSfx * 2 < 64) {
        gNumFxTags = gMaxSfx * 2;
    } else {
        gNumFxTags = 64;
    }
    gNumFxTags = (gMaxSfx + numB < gNumFxTags) ? gNumFxTags : gMaxSfx + numB;
    gFxTags = AllocateMemoryHeap(0, gNumFxTags * sizeof(Voice));
    PurgeableMemory(N64A(gFxTags), 0);
    sound_list_init(&gFreeFxTags, 0, 1);
    sound_list_init(&gPlayingFxTags, 0, 1);
    sound_list_init(&gInterruptedFxTags, 0, 1);
    gSortedFxTags = AllocateMemoryHeap(0, gNumFxTags * SORTED_TAG_SIZE);
    PurgeableMemory(N64A(gSortedFxTags), 0);
    gSortedFxTagTree.func = OrderFx;
    gSortedFxTagTree.unk8 = 0;
    gSortedFxTagTree.unk4 = 0;
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
        gSequences[i].unkC = 1;
        gSequences[i].unkD = 0;
        InsertLLE(&gFreeSequences, &gSequences[i].link, gFreeSequences.head);
    }
    AudInit3D(numA, numB, arg5, arg6, arg7);
    gInited_AudioMgr = 1;
}
