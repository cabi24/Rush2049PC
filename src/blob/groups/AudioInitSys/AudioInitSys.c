#include "game/game.h"

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

void AudioInitSys(s32 channels,s32 control,s32 enabled)
{
    u32 memory;
    s32 handle,i;
    s32 j;
    music_disabled=enabled<1;
    if(music_disabled) memory=0x80000;
    else memory=0x10000;
    if(!audio_driver_started) {
        osCreateMesgQueue(musyx_task_done_queue,musyx_task_done_msg,1);
        osCreateThread(sample_dma_thread,3,sample_dma_thread_entry,0,sample_dma_thread_stack_top,10);
        osStartThread(sample_dma_thread);
        osCreateThread(audio_retrace_thread,9,audio_retrace_thread_entry,0,audio_retrace_thread_stack_top,9);
        osStartThread(audio_retrace_thread);
        InitWaves();
        handle=LoadWave(6,0,0,0,0);
        LockWaveInstance(handle);
        musyx_project=GetWaveInstanceAddr(handle);
        handle=LoadWave(7,0,0,0,0);
        LockWaveInstance(handle);
        musyx_pool=GetWaveInstanceAddr(handle);
        handle=LoadWave(8,0,0,0,0);
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
        if(sound_lib_init_shared_output(22050,(u8)channels,(u8)enabled,(u8)control,0,memory)) {
        }
    } else {
        sound_lib_init(22050,(u8)channels,(u8)enabled,(u8)control,0,memory);
        audio_driver_started=1;
    }
    for(j=0;j<4;j++) {
        sound_push_group(musyx_project,boot_sfx_groups[j].id,(void *)musyx_samples,musyx_sdir,musyx_pool);
    }
    sound_fade_volume(127,0,255);
    sound_fade_master_volume(127,0,1,1);
    AudioSfxInit(control);
    song_handle=-1;
}
