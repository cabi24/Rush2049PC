/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudioInitSys (0x800A5D34, 836 B) -- historical label; really the N64 AudioInitSys
 * (MAT3 audiosysps2.c AudioInitSys(maxVoices, maxSfx), GC 0x8009ED90: the GC body is the PS2 layer,
 * only the final AudioSfxInit(maxSfx) call is shared -- AudioSfxInit = AudioSfxInit, here
 * AudioSfxInit(control)). N64 platform part reconstructed from the disassembly, building on the
 * frozen A142 reconstruction (72/209) -- credit A142.
 *
 * Calls: LoadWave = LoadWave, LockWaveInstance = display_list_alloc,
 * _ValidateWaveInstance = ValidateWaveInstance; GetWaveInstanceAddr (MAT3 waves.c, GC 0x8005B7B4:
 * `_ValidateWaveInstance(idx,0,0); return gWaveInstance[idx].ptr;`) is umerge-inlined three times, which
 * is what leaves the header pointer in v0 before each LoadWave call. gWaveInstance = D_80156D38
 * (20-byte records, ptr at +0xC). Static-library prototypes are the promoted ones (src/rom/sound/init.c,
 * groups.c, api.c): sound_stop_everything(void), sound_is_idle returns u8, sound_lib_init/sound_lib_init_shared_output
 * (u32 frequency, u8 voices, u8, u8, u16 count, u32 flags), sound_fade_volume(u8, u16, u8), sound_fade_master_volume(u8, u16, u8, u8).
 *
 * D_00039B40 is the ROM offset of an asset (retail builds it with lui 0x4 / addiu -0x64C0, i.e. as a
 * symbol address, not as the integer 0x39B40 which IDO would build with lui/ori); it is passed to
 * sound_push_group as the sample `address`. Absolute symbol allowed by the owner (2026-10-25); it needs no
 * symbol_addrs/GAME_SYMBOLS entry: the D_XXXXXXXX name encodes the address (the image splice.address_named),
 * as D_00BE4C70/D_00BDA100 in groups/heap_release/overlay_loader.c.
 * GetWaveInstanceAddr is a non-static, non-kept definition (no deleted-static stub).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned short u16;
typedef struct { u8 id,other1,mode,other3; s32 word4,word8; void *hdr; s32 word16; } Resource20;
typedef struct { u16 other,id; } Instrument4;
extern u8 music_disabled;
extern s8 audio_driver_started;
extern s32 song_handle;
extern u8 musyx_task_done_queue[],musyx_task_done_msg[];
extern u8 sample_dma_thread[],audio_retrace_thread[],sample_dma_thread_stack_top[],audio_retrace_thread_stack_top[];
extern Resource20 D_80156D38[];
extern Instrument4 boot_sfx_groups[4];   /* the four sfx groups pushed at boot; the song names' text follows */
extern void *musyx_project,*musyx_pool,*musyx_sdir;
extern intptr_t musyx_samples;
extern u8 D_00039B40[];
/* D_00039B40 is a cartridge address, 0xB0039B40: in a RUSH_NOFIXED build a name at pc_window + 0x30039B40, whose
 * 32-bit value the translation gives back (N64AS; pc_prelude.h, docs/port/NOFIXED.md 9.1). */
#define N64AS(x) (s32)PC_N64A(x)
/* the MusyX service table: 8 function addresses, which the data loader makes host functions
 * (docs/port/MUSYX_DATA.md 8.1) */
extern void (*audio_driver_services[8])(void);
extern void sample_dma_thread_entry(void *);
extern void audio_retrace_thread_entry(void *);
extern void osCreateMesgQueue(void *,void *,s32);
extern void osCreateThread(void *,s32,void (*)(void *),void *,void *,s32);
extern void osStartThread(void *);
extern void InitWaves(void);
extern s32 LoadWave(s32,s32,s32,s32,intptr_t);
extern void LockWaveInstance(s32);
extern void ValidateWaveInstance(s32,s32,s32);
extern void sound_set_services(void *);
extern void sound_stop_everything(void);
extern u8 func_800202c4(void);
extern s32 func_800154a4(void);
extern void func_800109c0(void);
extern s32 func_800108e0(u32,u8,u8,u8,u16,u32);
extern s32 sound_lib_init(u32,u8,u8,u8,u16,u32);
extern void func_8001536c(void *,u16,intptr_t,void *,void *);
extern void func_8002043c(u8,u16,u8);
extern void sound_fade_master_volume(u8,u16,u8,u8);
extern void AudioSfxInit(s32);

/* The game's own GetWaveInstanceAddr (this file had a second copy of it: the same code). */
void *GetWaveInstanceAddr(s32 idx);

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
        while(!func_800202c4()) {}
        for(i=0;i<4;i++) func_800154a4();
        func_800109c0();
        if(func_800108e0(22050,(u8)channels,(u8)enabled,(u8)control,0,memory)) {
        }
    } else {
        sound_lib_init(22050,(u8)channels,(u8)enabled,(u8)control,0,memory);
        audio_driver_started=1;
    }
    for(j=0;j<4;j++) {
        func_8001536c(musyx_project,boot_sfx_groups[j].id,musyx_samples,musyx_sdir,musyx_pool);
    }
    func_8002043c(127,0,255);
    sound_fade_master_volume(127,0,1,1);
    AudioSfxInit(control);
    song_handle=-1;
}
