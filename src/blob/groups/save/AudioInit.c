/* AudioInit (MAT3 audiomgr.c): the sound manager's start-up: the sound system (AudioInitSys), the audio manager
 * thread, the effect tags and their lists, the sequences, and the 3D sound (AudInit3D). The manager itself is
 * audiomgr/audiomgr_thread.c. */
#include "game/game.h"
#include "game/audiomgr.h"

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4), seen here as messages; PC_SND_MSG_LOOP(128) is the run's count: 128 at
 * original. */
#define audiomgr_gCmds (*(tAudioMsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)

/* InitLL (MAT3 ll.c), the sound manager's copy. */
void sound_list_init(AudioList *list, s8 handle, s8 dbl) {
    list->indirect = handle;
    list->doubly = dbl;
    ClearLL(list);
}

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
        audiomgr_gCmds[i].hdr.used = 0;
    }
    if (!queueCreated) {
        queueCreated = 1;
        queue_init(&audio_lock_queue, audio_lock_msg, 1);
        lock_give(&audio_lock_queue);
    }
    AudioInitSys(numVoices, maxSfx, maxMusic);
    if (!reinit) {
        /* The thread's storage is the N64's OSThread, 0x1B0 bytes (OSThreadBytes): the PC's osCreateThread keeps
         * only its address, as the thread's key (port/ultra/sched.c). */
        thread_create((OSThread *) &audio_manager_thread, THREAD_ID_AUDIO_MANAGER, audio_manager_thread_entry, 0,
                      audio_manager_thread_stack_top, THREAD_PRI_AUDIO_MANAGER);
        thread_start((OSThread *) &audio_manager_thread);
    }
    if (gMaxSfx * 2 < 64) {
        gNumFxTags = gMaxSfx * 2;
    } else {
        gNumFxTags = 64;
    }
    gNumFxTags = (gMaxSfx + numEmitters < gNumFxTags) ? gNumFxTags : gMaxSfx + numEmitters;
    gFxTags = AllocateMemoryHeap(0, gNumFxTags * sizeof(tFxSequence));
    PurgeableMemory(gFxTags, 0);
    sound_list_init(&gFreeFxTags, 0, 1);
    sound_list_init(&gPlayingFxTags, 0, 1);
    sound_list_init(&gInterruptedFxTags, 0, 1);
    gSortedFxTags = AllocateMemoryHeap(0, gNumFxTags * (s32) sizeof(tSortedFxTag));
    PurgeableMemory(gSortedFxTags, 0);
    gSortedFxTagTree.OrderFunc = OrderFx;
    gSortedFxTagTree.root = 0;
    gSortedFxTagTree.numElements = 0;
    gNumStartCmds = gNumStopCmds = 0;
    for (i = 0; i < gNumFxTags; i++) {
        gFxTags[i].id = i;
        InsertLLE(&gFreeFxTags, &gFxTags[i].ll, gFreeFxTags.head);
        gFxTags[i].list = &gFreeFxTags;
    }
    gTagShift = HAL_IntLog2((u32)gNumFxTags);
    if ((1 << gTagShift) < gNumFxTags) {
        gTagShift++;
    }
    gTagMask = (1 << gTagShift) - 1;
    gSequences = AllocateMemoryHeap(0, 6 * sizeof(tAudioSequence));
    PurgeableMemory(gSequences, 0);
    sound_list_init(&gFreeSequences, 0, 1);
    sound_list_init(&gActiveSequences, 0, 1);
    for (i = 0; i < 6; i++) {
        gSequences[i].id = i;
        gSequences[i].free = 1;
        gSequences[i].active = 0;
        InsertLLE(&gFreeSequences, &gSequences[i].ll, gFreeSequences.head);
    }
    AudInit3D(numListeners, numEmitters, footUnits, frontDistance, doppler);
    gInited_AudioMgr = 1;
}
