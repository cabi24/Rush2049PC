/* AudioInitSys: the start-up of the sound system under the sound manager (the N64 form of MAT3's audiosysps2.c
 * AudioInitSys): the sound threads, the MusyX library and its data, the boot sound groups and the sound server. */
#include "game/game.h"

/* A sound group to push (its id in the project, MusyX's group id). */
typedef struct { u16 other,id; } Instrument4;

VIEW(u8[], musyx_task_done_queue);
#define musyx_task_done_queue musyx_task_done_queue_view

VIEW(Instrument4[4], boot_sfx_groups);
#define boot_sfx_groups boot_sfx_groups_view

VIEW(intptr_t, musyx_samples);
#define musyx_samples musyx_samples_view
extern u8 D_00039B40[];
/* D_00039B40 is a cartridge address, 0xB0039B40: in a RUSH_NOFIXED build a name at pc_window + 0x30039B40, whose
 * 32-bit value the translation gives back (N64AS; pc_prelude.h, docs/port/NOFIXED.md 9.1). */
#define N64AS(x) (s32)PC_N64A(x)
VIEW(void (*[8])(void), audio_driver_services);
#define audio_driver_services audio_driver_services_view

extern void osCreateMesgQueue(void *,void *,s32);
extern void osCreateThread(void *,s32,void (*)(void *),void *,void *,s32);
extern void osStartThread(void *);

/* MusyX's output rate. */
#define AUDIO_FREQUENCY 22050
/* sound_lib_init's flags: the sample cache's size class (sound_hw_cache_mode_from_flags): a bigger cache when there
 * is no music to make room for. */
#define SOUND_CACHE_CLASS_LARGE 0x80000
#define SOUND_CACHE_CLASS_SMALL 0x10000
/* The waves (asset files) of MusyX's data: the project, the sample pool, the sample directory
 * (docs/port/MUSYX_DATA.md); the samples themselves stay in the cartridge. */
#define WAVE_MUSYX_PROJECT 6
#define WAVE_MUSYX_POOL 7
#define WAVE_MUSYX_SDIR 8

/* Called by AudioInit (each time the voice quotas change). maxVoices voices in all, maxSfx of them for effects,
 * maxMusic for songs (0: no music). The first time it creates the task queue and the two sound threads (sample DMA,
 * retrace), loads and locks MusyX's data and installs the library's services; then (re)starts MusyX for these
 * quotas, pushes the four boot sound groups, sets the volumes to full and starts the sound server for maxSfx
 * effects. A restart stops every sound and pops the groups first. */
void AudioInitSys(s32 maxVoices,s32 maxSfx,s32 maxMusic)
{
    u32 flags;
    s32 handle,i;
    s32 j;
    music_disabled=maxMusic<1;
    if(music_disabled) flags=SOUND_CACHE_CLASS_LARGE;
    else flags=SOUND_CACHE_CLASS_SMALL;
    if(!audio_driver_started) {
        osCreateMesgQueue(musyx_task_done_queue,musyx_task_done_msg,1);
        osCreateThread(sample_dma_thread,3,sample_dma_thread_entry,0,sample_dma_thread_stack_top,10);
        osStartThread(sample_dma_thread);
        osCreateThread(audio_retrace_thread,9,audio_retrace_thread_entry,0,audio_retrace_thread_stack_top,9);
        osStartThread(audio_retrace_thread);
        InitWaves();
        handle=LoadWave(WAVE_MUSYX_PROJECT,0,0,0,0);
        LockWaveInstance(handle);
        musyx_project=GetWaveInstanceAddr(handle);
        handle=LoadWave(WAVE_MUSYX_POOL,0,0,0,0);
        LockWaveInstance(handle);
        musyx_pool=GetWaveInstanceAddr(handle);
        handle=LoadWave(WAVE_MUSYX_SDIR,0,0,0,0);
        LockWaveInstance(handle);
        musyx_sdir=GetWaveInstanceAddr(handle);
        musyx_samples=N64AS(D_00039B40);
        sound_set_services(&audio_driver_services);
    }
    if(audio_driver_started) {
        sound_stop_everything();
        while(!sound_is_idle()) {}
        for(i=0;i<4;i++) sound_pop_group();
        sound_lib_exit_shared_output();
        if(sound_lib_init_shared_output(AUDIO_FREQUENCY,(u8)maxVoices,(u8)maxMusic,(u8)maxSfx,0,flags)) {
        }
    } else {
        sound_lib_init(AUDIO_FREQUENCY,(u8)maxVoices,(u8)maxMusic,(u8)maxSfx,0,flags);
        audio_driver_started=1;
    }
    for(j=0;j<4;j++) {
        sound_push_group(musyx_project,boot_sfx_groups[j].id,(void *)musyx_samples,musyx_sdir,musyx_pool);
    }
    sound_fade_volume(127,0,255);
    sound_fade_master_volume(127,0,1,1);
    AudioSfxInit(maxSfx);
    song_handle=-1;
}
