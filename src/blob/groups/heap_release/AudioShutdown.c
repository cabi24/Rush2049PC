/* AudioShutdown: the sound manager's shut-down (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(volatile tAudioMsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)

/* The three blocks as plain addresses, as this file always passed them. */
VIEW(uintptr_t, gFxTags);
#define gFxTags gFxTags_view
VIEW(uintptr_t, gSortedFxTags);
#define gSortedFxTags gSortedFxTags_view
VIEW(uintptr_t, gSequences);
#define gSequences gSequences_view

/* If the sound manager is up: frees the 3D sound, stops the song and every effect (post_type1, post_type7), waits
 * until the audio manager thread has run every message of the pool, shuts the sound system down
 * (AudioShutdownSys) and frees the fx tags, the tag tree's nodes and the sequences.
 * PC: the wait is a busy-wait that lets the manager run (docs/port/ULTRA.md 6). */
void AudioShutdown(void) {
    s32 i;

    if (gInited_AudioMgr) {
        AudFree3D();
        post_type1();
        post_type7();
        do { pc_os_spin_yield("AudioShutdown");
            for (i = 0; i < PC_SND_MSG_LOOP(128); i++) {
                if (audiomgr_gCmds[i].hdr.used) {
                    break;
                }
            }
        } while (i < PC_SND_MSG_LOOP(128));
        gInited_AudioMgr = 0;
        AudioShutdownSys();
        DeallocateMemory(gFxTags);
        DeallocateMemory(gSortedFxTags);
        DeallocateMemory(gSequences);
    }
}
