/* GENERATED ROM-aligned TU — segment 0x11640 (rom/lib_11640)
 * layout map 5aa0eac4fd4c7b89e56ec8126930be97345b2a4934e8ca8af0a9af74f0efb716.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"
/* owned .bss of sound_hw_submit_task: C definitions in place of the companion */
void *sound_delay_line;
unsigned int sound_delay_length;
OSTask sound_task;
struct AudioNode *sound_codebook_nodes;
unsigned char *sound_codebook_cache;
#include "sound_hw_voice.h"

/* PROMOTED 2026-10-05 — sound_hw_run_output_thread
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_half_rate;
extern unsigned char sound_output_frozen;
extern volatile unsigned char sound_output_index;
extern unsigned short sound_output_buffer_count;
extern unsigned int sound_output_frequency;
extern void *sound_output_buffers[];
extern unsigned char sound_output_queue[];
extern void *(*sound_service_alloc)(unsigned int, unsigned int);
extern void bzero(void *, int);
extern void osWritebackDCache(void *, int);
extern int osAiSetNextBuffer(void *, unsigned int);
extern int osRecvMesg(OSMesgQueue *, void **, int);
void sound_hw_run_output_thread(void *argument)
{
    void *message;
    int i;
    unsigned int stop;
    if (sound_half_rate != 0) {
        sound_output_buffer_count = 24;
    } else {
        sound_output_buffer_count = 16;
    }
    if (sound_output_frequency <= 22050) {
        sound_output_buffer_count >>= 1;
    }
    sound_output_buffers[0] = sound_service_alloc(sound_output_buffer_count * 768, 128);
    for (i = 1; i < sound_output_buffer_count; i++) {
        sound_output_buffers[i] = (unsigned char *)sound_output_buffers[i - 1] + 768;
    }
    bzero(sound_output_buffers[0], sound_output_buffer_count * 768);
    osWritebackDCache(sound_output_buffers[0], sound_output_buffer_count * 768);
    sound_output_index = 0;
    osAiSetNextBuffer(sound_output_buffers[0], 768);
    stop = 0;
    do {
        osRecvMesg((OSMesgQueue *)sound_output_queue, &message, 1);
        if (sound_output_frozen == 0) {
            switch (*(unsigned char *) message) {
            case 1:
                sound_output_index = (sound_output_index + 1) % sound_output_buffer_count;
                osAiSetNextBuffer(sound_output_buffers[sound_output_index], 768);
                break;
            case 255:
                stop = 1;
                break;
            }
        }
    } while (stop == 0);
}

/* PROMOTED 2026-10-04 — sound_hw_start_output_thread
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_output_event;
extern unsigned char sound_output_queue[];
extern void *sound_output_messages[];
extern unsigned int sound_output_frequency;
extern unsigned char *sound_output_stack;
extern unsigned char sound_output_thread[];
extern void *(*sound_service_alloc)(unsigned int, unsigned int);
extern void osCreateMesgQueue(OSMesgQueue *, void **, int);
extern int osAiSetFrequency(unsigned int);
extern void osCreateThread(OSThread *, int, void (*)(void *), void *, void *, int);
extern void osStartThread(OSThread *);
extern void sound_hw_run_output_thread(void *);
void sound_hw_start_output_thread(unsigned int *frequency)
{
    sound_output_event = 1;
    osCreateMesgQueue((OSMesgQueue *) sound_output_queue, sound_output_messages, 4);
    osSetEventMesg(6, (OSMesgQueue *) sound_output_queue, &sound_output_event);
    sound_output_frequency = osAiSetFrequency(*frequency);
    *frequency = sound_output_frequency;
    sound_output_stack = sound_service_alloc(1024, 128);
    osCreateThread((OSThread *) sound_output_thread, 0, sound_hw_run_output_thread, 0,
                   sound_output_stack + 1024, 122);
    osStartThread((OSThread *) sound_output_thread);
}

/* PROMOTED 2026-10-04 — sound_hw_set_output_frequency
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_set_output_frequency(unsigned int *frequency)
{
    sound_output_frequency = osAiSetFrequency(*frequency);
    *frequency = sound_output_frequency;
}

/* PROMOTED 2026-10-05 — sound_hw_stop_output_thread
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int osSendMesg(OSMesgQueue *, OSMesg, int);
extern void (*sound_service_free)(void *);
extern void *sound_output_buffers[];
void sound_hw_stop_output_thread(void)
{
    unsigned char command[1];
    command[0] = 255;
    osSendMesg((OSMesgQueue *)sound_output_queue, command, 1);
    sound_service_free(sound_output_buffers[0]);
    sound_service_free(sound_output_stack);
}

/* PROMOTED 2026-10-12 — sound_hw_alloc_delay_line
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_delay_length;
extern void *sound_delay;
extern void *sound_delay_line;
void sound_hw_alloc_delay_line(unsigned int rate, unsigned short duration)
{
    unsigned int count;
    if (duration != 0) {
        count = duration * rate / 1000;
        count = 192 - count % 192 + count;
        sound_delay_length = count;
        sound_delay = 0;
        sound_delay_line = sound_service_alloc(count * 2, 0);
    } else {
        sound_delay_line = 0;
    }
}

/* PROMOTED 2026-10-11 — sound_hw_set_delay
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SoundDelayConfig { unsigned short duration; unsigned short delays[8]; unsigned char gains[8]; unsigned char count; unsigned char feedback; unsigned char mode; unsigned char unknown1D; short filter[4]; } SoundDelayConfig;
typedef struct SoundDelayState { void *buffer; unsigned int length; unsigned short count; unsigned short feedback; unsigned int delays[8]; unsigned short gains[8]; unsigned int unknown3C; short filter[4]; } SoundDelayState;
#define DELAY_STATE_SIZE sizeof(SoundDelayState)   /* 72 bytes on the N64; buffer is a host pointer */
extern void *sound_delay_line;
extern void *sound_delay;
extern int sound_delay_position;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
void sound_hw_set_delay(void *configuration)
{
    unsigned int count;
    SoundDelayState *state;
    unsigned int i;
    if (sound_delay_line != 0) {
        count = ((SoundDelayConfig *)configuration)->duration * sound_output_frequency / 1000;
        count = 192 - count % 192 + count;
        state = sound_service_alloc(DELAY_STATE_SIZE, 0);
        state->buffer = sound_delay_line;
        i = count * 2;
        bzero(state->buffer, i);
        osWritebackDCache(state->buffer, i);
        state->length = count;
        state->feedback = ((SoundDelayConfig *)configuration)->feedback << 8;
        state->count = ((SoundDelayConfig *)configuration)->count;
        for (i = 0; i < ((SoundDelayConfig *)configuration)->count; i++) {
            state->delays[i] = (((SoundDelayConfig *)configuration)->delays[i] * sound_output_frequency / 1000 + 3) & 0xFFFC;
            state->gains[i] = ((SoundDelayConfig *)configuration)->gains[i] << 8;
        }
        if (((SoundDelayConfig *)configuration)->mode == 1) {
            for (i = 0; i < 4; i++) {
                state->filter[i] = ((SoundDelayConfig *)configuration)->filter[i];
            }
        } else {
            state->filter[0] = 0;
            state->filter[1] = 0;
            state->filter[2] = 0;
            state->filter[3] = 32767;
        }
        osWritebackDCache(state, 72);
        sound_hw_lock();
        sound_delay_position = 0;
        sound_delay = state;
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_hw_remove_delay
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *sound_delay;
extern void (*sound_service_free)(void *);
extern void sound_hw_wait_task(void);
void sound_hw_remove_delay(void)
{
    if (sound_delay != 0) {
        sound_hw_wait_task();
        sound_service_free(sound_delay);
        sound_delay = 0;
    }
}

/* PROMOTED 2026-10-04 — sound_hw_free_delay_line
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *sound_delay_line;
extern void sound_hw_remove_delay(void);
void sound_hw_free_delay_line(void)
{
    sound_hw_remove_delay();
    if (sound_delay_line != 0) {
        sound_service_free(sound_delay_line);
    }
}

/* PROMOTED 2026-10-11 — sound_hw_alloc_voices
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *sound_hw_voice_block;
extern unsigned short sound_hw_voice_count;
extern unsigned char sound_sfd_set;
extern float sound_sfd_margin;
extern float sound_sfd_margin_half_rate;
extern unsigned short sound_sfds_per_task;
extern unsigned short *sound_sfd_sets[2];
extern unsigned short sound_sfd_counts[2];
extern void *sound_mix_state;
static unsigned int pc_sfd_size(void);   /* sizeof(SfdBlock) with 32 voices: 2576 bytes on the N64 */
#define SFD_SIZE pc_sfd_size()
/* PC: with more voices than 32 ([audio] voices, docs/port/AUDIO.md 10) an SFD holds a voice record a voice slot and the
 * state block their last outputs (MX_STATE_SIZE_N): pc_sfd_layout gives the interpreter the slots as the voices are
 * set up; 32 records and 664 bytes otherwise, the N64's. */
static void pc_sfd_layout(unsigned short count);
static unsigned int pc_sfd_state_size(void);
#define PC_MX_STATE(n) pc_sfd_state_size()
void sound_hw_alloc_voices(unsigned short count, unsigned int frequency)
{
    unsigned short i;
    sound_hw_voice_count = count; pc_sfd_layout(count);
    sound_hw_voice_block = sound_service_alloc(sound_hw_voice_count * sizeof(SoundHwVoice), 0);
    sound_hw_voices = sound_hw_voice_block;
    sound_sfd_set = 0;
    for (i = 0; i < sound_hw_voice_count; i++) {
        ((SoundHwVoice *)sound_hw_voice_block)[i].active = 0;
        ((SoundHwVoice *)sound_hw_voice_block)[i].release_pending = 0;
        ((SoundHwVoice *)sound_hw_voice_block)[i].unknown60 = 255;
    }
    if (sound_half_rate == 0) {
        sound_sfds_per_task = (unsigned short) (((float)frequency / 50.0f / 192.0f) * sound_sfd_margin) + 1;
    } else {
        sound_sfds_per_task = (unsigned short) (((float)frequency / 25.0f / 192.0f) * sound_sfd_margin_half_rate) + 1;
    }
    sound_sfd_sets[0] = sound_service_alloc(sound_sfds_per_task * SFD_SIZE, 0);
    sound_sfd_sets[1] = sound_service_alloc(sound_sfds_per_task * SFD_SIZE, 0);
    sound_sfd_counts[1] = 0;
    sound_sfd_counts[0] = 0;
    sound_mix_state = sound_service_alloc(PC_MX_STATE(664), 0);
    bzero(sound_mix_state, PC_MX_STATE(664));
    osWritebackDCache(sound_mix_state, PC_MX_STATE(664));
}

/* PROMOTED 2026-10-04 — sound_hw_free_voices
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 * Relocked: 2026-10-05 as the two-cell sound_sfd_sets table:
 *Byte-matched against the retail ROM, 2026-10.
 *           (score0 after reloc_addrs.us.txt names D_800382DC as sound_sfd_sets+4)
 */
extern unsigned short *sound_sfd_sets[2];
extern void *sound_hw_voice_block;
extern void *sound_mix_state;
void sound_hw_free_voices(void)
{
    sound_service_free(sound_sfd_sets[0]);
    sound_service_free(sound_sfd_sets[1]);
    sound_service_free(sound_hw_voice_block);
    sound_service_free(sound_mix_state);
}

/* PROMOTED 2026-10-11 — sound_hw_alloc_dma
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "PR/os_message.h"
extern float sound_dma_margin;
extern float sound_dma_margin_half_rate;
extern float sound_sample_cache_factor;
extern unsigned short sound_dma_capacity;
extern unsigned int sound_sample_cache_size;
extern void *sound_dma_list;
extern unsigned char sound_dma_queue[];
extern OSMesg sound_dma_message;
extern volatile unsigned char sound_dma_busy;
extern unsigned short sound_sample_blocks_per_voice[];
extern void sound_hw_init_codebook_cache(int);
extern void sound_hw_init_sample_cache(unsigned short);
#define DMA_ENTRY_SIZE 16U   /* sizeof(SoundDmaEntryCodebook): 12 bytes on the N64, the destination is a host pointer */
void sound_hw_alloc_dma(unsigned short count, unsigned int frequency, unsigned char mode)
{
    if (sound_half_rate == 0) {
        sound_dma_capacity = count * ((unsigned short) (((float)frequency / 50.0f * sound_dma_margin) / 192.0f) + 1);
    } else {
        sound_dma_capacity = count * ((unsigned short) (((float)frequency / 25.0f * sound_dma_margin_half_rate) / 192.0f) + 1);
    }
    sound_sample_cache_size = 784;
    if (sound_sample_cache_size < 192) {
        sound_sample_cache_size = 192;
    }
    sound_sample_cache_size = (unsigned int) ((float)(sound_dma_capacity * sound_sample_cache_size) * sound_sample_cache_factor);
    sound_sample_cache_size = ((((sound_sample_cache_size + 63) >> 6) * 40) + 15) & 0xFFF0;
    sound_dma_list = sound_service_alloc(sound_dma_capacity * 2 * DMA_ENTRY_SIZE, 0);
    osCreateMesgQueue((OSMesgQueue *)sound_dma_queue, &sound_dma_message, 1);
    sound_dma_busy = 0;
    sound_hw_init_codebook_cache(sound_dma_capacity);
    sound_hw_init_sample_cache((unsigned short)(sound_sample_blocks_per_voice[mode] * count));
}

/* PROMOTED 2026-10-04 — sound_hw_free_dma
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile unsigned char sound_dma_busy;
extern void *sound_dma_list;
void sound_hw_free_dma(void)
{
    while (sound_dma_busy != 0) { pc_os_spin_yield("sound_hw_free_dma");
    }
    sound_service_free(sound_dma_list);
}

/* PROMOTED 2026-10-05 — sound_hw_reset_dma_list
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_dma_list_end;
extern unsigned int sound_dma_list_cache_cursor;
extern unsigned short sound_dma_list_count;
void sound_hw_reset_dma_list(void)
{
    unsigned int value;
    value = (unsigned int)sound_dma_list;
    sound_dma_list_cache_cursor = value & ~15U;
    sound_dma_list_end = value;
    sound_dma_list_count = 0;
}

/* PROMOTED 2026-10-04 — sound_hw_wait_task
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile unsigned char sound_task_busy;
extern void (*sound_service_wait_task)(void);
extern void osYieldThread(void);
void sound_hw_wait_task(void)
{
    if (sound_task_busy != 0) {
        sound_service_wait_task();
        osYieldThread();
        sound_task_busy = 0;
    }
}

/* PROMOTED 2026-10-12 — sound_hw_submit_task
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
extern unsigned char rspbootTextStart[];
extern unsigned char rspbootTextEnd[];
extern unsigned char audio_ucode_text[];
extern unsigned char audio_ucode_data[];
extern void (*sound_services)(OSTask *);
void sound_hw_submit_task(void *commands, unsigned short count)
{
    OSTask *task = &sound_task;
    if (count != 0) {
        task->t.type = 2;
        task->t.flags = 0;
        task->t.ucode_boot = (u64 *) rspbootTextStart;
        task->t.ucode_boot_size = (unsigned int) rspbootTextEnd - (unsigned int) rspbootTextStart;
        task->t.ucode = (u64 *) audio_ucode_text;
        task->t.ucode_size = 4096;
        task->t.ucode_data = (u64 *) audio_ucode_data;
        task->t.ucode_data_size = 2048;
        task->t.data_ptr = commands;
        task->t.data_size = count;
        osWritebackDCache(commands, count * 2576);
        sound_services(task);
        sound_task_busy = 1;
    }
}

/* PROMOTED 2026-10-04 — sound_hw_submit_sfds
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned short *sound_sfd_current;
extern unsigned short *sound_sfd_count;
extern void sound_hw_submit_task(void *, unsigned short);
void sound_hw_submit_sfds(void)
{
    sound_hw_submit_task(sound_sfd_current, *sound_sfd_count);
}

/* PROMOTED 2026-10-04 — sound_hw_start_envelope
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_start_envelope(SoundHwVoice *audio)
{
    audio->state = 0;
    audio->step = 0;
    audio->saved_value = 0.0f;
    audio->value = 0.0f;
    audio->count = audio->initial_count;
    audio->scale = 1.0f;
}

/* PROMOTED 2026-10-04 — sound_hw_release_envelope
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_release_envelope(SoundHwVoice *audio)
{
    audio->state = 3;
    audio->step = 0;
    audio->saved_value = audio->value;
    audio->scale = 0.0f;
    audio->count = audio->release_count;
}

/* PROMOTED 2026-10-11 — sound_hw_advance_envelope
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_advance_envelope(SoundHwVoice *audio, unsigned short samples, unsigned char *active)
{
    unsigned int increment;
    float ratio;
    increment = ((samples * 32000U) / sound_output_frequency) << 11;
    if (audio->pending == 0 && audio->state != 3) {
        audio->state = 3;
        audio->step = 0;
        audio->saved_value = audio->value;
        audio->scale = 0.0f;
        audio->count = audio->release_count;
    } else if ((audio->step >> 16) >= audio->count) {
        switch (audio->state) {
        case 0:
            audio->state = 1;
            audio->step = 0;
            audio->saved_value = 1.0f;
            audio->scale = audio->value24;
            audio->count = audio->value22;
            break;
        case 1:
            audio->state = 2;
            audio->saved_value = audio->value24;
            audio->scale = audio->value24;
            audio->value = audio->value24;
            return;
        case 3:
            *active = 0;
            audio->value = 0.0f;
            return;
        }
    }
    if (audio->count != 0) {
        ratio = audio->step / (audio->count * 65536.0f);
    } else {
        ratio = 1.0f;
    }
    audio->value = (audio->scale - audio->saved_value) * ratio + audio->saved_value;
    audio->step += increment;
}

/* PROMOTED 2026-10-11 — sound_hw_reset_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_reset_voice(unsigned short index, unsigned short flags)
{
    SoundHwVoice *audio;
    audio = &sound_hw_voices[index];
    audio->active = 0;
    audio->flags = flags;
    audio->rate = 4096;
    audio->pending = 1;
    audio->unknown18[0] = 0;
    audio->unknown18[1] = 0;
    audio->unknown18[2] = 0;
    audio->unknown18[3] = 0;
    audio->value40 = 0;
    audio->value42 = 0;
    audio->value44 = 0;
    audio->value46 = 0;
}

/* PROMOTED 2026-10-05 — sound_hw_activate_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
void sound_hw_activate_voice(unsigned short index)
{
    SoundHwVoice *audio = &sound_hw_voices[index];
    sound_hw_start_envelope(audio);
    audio->active = 1;
}

/* PROMOTED 2026-10-04 — sound_hw_wait_dma
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int osRecvMesg(OSMesgQueue *, void **, int);
extern unsigned char sound_dma_queue[];
void sound_hw_wait_dma(void)
{
    if (sound_dma_busy != 0) {
        osRecvMesg((OSMesgQueue *)sound_dma_queue, 0, 1);
        sound_dma_busy = 0;
        osYieldThread();
    }
}

/* PROMOTED 2026-10-04 — sound_hw_start_dma
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned short sound_dma_list_count;
extern void (*sound_service_start_dma)(void *, unsigned short, void *);
void sound_hw_start_dma(void)
{
    if (sound_dma_list_count != 0) {
        sound_dma_busy = 1;
        sound_service_start_dma(sound_dma_list, sound_dma_list_count, sound_dma_queue);
    }
}

/* PROMOTED 2026-10-11 — sound_hw_advance_voice_position
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern double sound_truncate(double);
void sound_hw_advance_voice_position(SoundHwVoice *audio, unsigned short samples)
{
    double increment;
    double loop_end;
    increment = (audio->rate * samples) * 0.000244140625;
    if (audio->loop_length != 0 && (audio->pending != 0 || audio->tail == 0)
        && sound_truncate(audio->sample_position) <= audio->loop_start + audio->loop_length) {
        loop_end = audio->loop_start + audio->loop_length;
        if (audio->sample_position + increment < loop_end) {
            audio->sample_position += increment;
        } else {
            increment -= loop_end - audio->sample_position;
            increment -= sound_truncate(increment / audio->loop_length) * audio->loop_length;
            audio->sample_position = audio->loop_start + increment;
        }
    } else {
        if (audio->sample_position + increment >= audio->length) {
            audio->sample_position = audio->length - 1;
        } else {
            audio->sample_position += increment;
        }
    }
}

typedef struct AudioNode { struct AudioNode *next; struct AudioNode *previous; unsigned char *buffer; unsigned int unknown0C; unsigned short countdown; unsigned short unknown12; } AudioNode;
extern AudioNode *sound_codebook_used;
/* PROMOTED 2026-10-11 — sound_hw_init_codebook_cache
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void osInvalDCache(void *, int);
extern AudioNode *sound_codebook_nodes;
extern unsigned char *sound_codebook_cache;
extern unsigned int sound_codebook_last;
extern AudioNode *sound_codebook_free;
void sound_hw_init_codebook_cache(int count)
{
    int i;
    sound_codebook_nodes = sound_service_alloc(count * sizeof(AudioNode), 0);
    sound_codebook_cache = sound_service_alloc(count * 256, 0);
    osInvalDCache(sound_codebook_cache, count * 256); pc_mem_register_be(sound_codebook_cache, count * 256);
    sound_codebook_used = 0;
    sound_codebook_free = sound_codebook_nodes;
    sound_codebook_nodes[0].buffer = sound_codebook_cache;
    sound_codebook_nodes[0].previous = 0;
    for (i = 1; i < count; i++) {
        sound_codebook_nodes[i].previous = &sound_codebook_nodes[i - 1];
        sound_codebook_nodes[i - 1].next = &sound_codebook_nodes[i];
        sound_codebook_nodes[i].buffer = &sound_codebook_cache[i * 256];
    }
    sound_codebook_nodes[i - 1].next = 0;
    sound_codebook_last = 0;
}

/* PROMOTED 2026-10-04 — sound_hw_free_codebook_cache
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char *sound_codebook_cache;
extern AudioNode *sound_codebook_nodes;
void sound_hw_free_codebook_cache(void)
{
    pc_mem_unregister_be(sound_codebook_cache); sound_service_free(sound_codebook_cache);
    sound_service_free(sound_codebook_nodes);
}

/* PROMOTED 2026-10-04 — sound_hw_age_codebook_cache
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_age_codebook_cache(void)
{
    AudioNode *node;
    for (node = sound_codebook_used; node != 0; node = node->next) {
        if (node->countdown != 0) {
            --node->countdown;
        }
    }
}

/* PROMOTED 2026-10-12 — sound_hw_fetch_codebook
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SoundDmaEntryCodebook { int length; unsigned int source; unsigned char *destination; } SoundDmaEntryCodebook;
typedef char pc_dma_entry_check[sizeof(SoundDmaEntryCodebook) == DMA_ENTRY_SIZE ? 1 : -1];
extern unsigned int dcache_create_dirty_lines(unsigned int, unsigned int);
unsigned char *sound_hw_fetch_codebook(unsigned int source)
{
    AudioNode *node;
    for (node = sound_codebook_used; node != 0; node = node->next) {
        if (source == node->unknown0C) {
            node->countdown = 2;
            return node->buffer;
        }
    }
    if (sound_codebook_free != 0) {
        node = sound_codebook_free;
        sound_codebook_free = node->next;
        if (sound_codebook_free != 0) {
            sound_codebook_free->previous = 0;
        }
        if (sound_codebook_used != 0) {
            sound_codebook_used->previous = node;
        }
        node->next = sound_codebook_used;
        sound_codebook_used = node;
    } else {
        for (node = sound_codebook_used; node != 0; node = node->next) {
            if (node->countdown == 0) {
                break;
            }
        }
    }
    node->countdown = 2;
    node->unknown0C = source;
    sound_dma_list_end += 12;
    sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
    ((SoundDmaEntryCodebook *)sound_dma_list)[sound_dma_list_count].source = source;
    ((SoundDmaEntryCodebook *)sound_dma_list)[sound_dma_list_count].length = 0x100;
    ((SoundDmaEntryCodebook *)sound_dma_list)[sound_dma_list_count].destination = node->buffer;
    sound_dma_list_count++;
    return node->buffer;
}

typedef struct AudioCacheNode { struct AudioCacheNode *next; struct AudioCacheNode *previous; unsigned int unknown08; unsigned int unknown0C; unsigned int unknown10; unsigned char *buffer; } AudioCacheNode;
extern AudioCacheNode *sound_sample_lru_head;
extern AudioCacheNode *sound_sample_lru_tail;
extern AudioCacheNode *sound_sample_free;
/* PROMOTED 2026-10-11 — sound_hw_init_sample_cache
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern AudioCacheNode *sound_sample_nodes;
extern unsigned char *sound_sample_cache;
void sound_hw_init_sample_cache(unsigned short count)
{
    int i;
    sound_sample_cache = sound_service_alloc(count * 1536, 0);
    osInvalDCache(sound_sample_cache, count * 1536); pc_mem_register_be(sound_sample_cache, count * 1536);
    sound_sample_nodes = sound_service_alloc(count * sizeof(AudioCacheNode), 0);
    sound_sample_lru_head = 0;
    sound_sample_lru_tail = 0;
    sound_sample_free = sound_sample_nodes;
    sound_sample_nodes[0].previous = 0;
    sound_sample_nodes[0].buffer = sound_sample_cache;
    for (i = 1; i < count; i++) {
        sound_sample_nodes[i - 1].next = &sound_sample_nodes[i];
        sound_sample_nodes[i].previous = &sound_sample_nodes[i - 1];
        sound_sample_nodes[i].buffer = &sound_sample_cache[i * 1536];
    }
    sound_sample_nodes[i - 1].next = 0;
}

/* PROMOTED 2026-10-04 — sound_hw_free_sample_cache
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern AudioCacheNode *sound_sample_nodes;
extern unsigned char *sound_sample_cache;
void sound_hw_free_sample_cache(void)
{
    sound_service_free(sound_sample_nodes);
    pc_mem_unregister_be(sound_sample_cache); sound_service_free(sound_sample_cache);
}

/* PROMOTED 2026-10-04 — sound_hw_take_sample_block
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
AudioCacheNode *sound_hw_take_sample_block(unsigned int tag)
{
    AudioCacheNode *node;
    if (sound_sample_free != 0) {
        node = sound_sample_free;
        sound_sample_free = node->next;
        if (sound_sample_free != 0) {
            sound_sample_free->previous = 0;
        }
        if (sound_sample_lru_tail == 0) {
            node->next = sound_sample_lru_head;
            if (node->next != 0) {
                sound_sample_lru_head->previous = node;
            }
            sound_sample_lru_head = node;
            sound_sample_lru_tail = node;
        } else {
            node->next = 0;
            node->previous = sound_sample_lru_tail;
            sound_sample_lru_tail->next = node;
            sound_sample_lru_tail = node;
        }
        return node;
    }
    node = sound_sample_lru_head;
    node->next->previous = 0;
    sound_sample_lru_head = node->next;
    sound_sample_lru_tail->next = node;
    node->previous = sound_sample_lru_tail;
    node->next = 0;
    sound_sample_lru_tail = node;
    return node;
}

/* PROMOTED 2026-10-11 — sound_hw_fetch_sample_range
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
typedef struct SoundDmaEntrySamples { int length; unsigned int source; unsigned char *destination; } SoundDmaEntrySamples;
typedef struct SfdSampleRange { unsigned char *first; unsigned char *second; short first_length; short second_length; } SfdSampleRange;
extern AudioCacheNode *sound_hw_take_sample_block(unsigned int tag);
extern unsigned int dcache_create_dirty_lines(unsigned int, unsigned int);
void sound_hw_fetch_sample_range(SfdSampleRange *fetch, unsigned int addr, unsigned int length, unsigned int tag)
{
    unsigned int size;
    AudioCacheNode *previous;
    AudioCacheNode *node;
    unsigned char *pointers[2];
    unsigned int lengths[2];
    unsigned int starts[2];
    unsigned short count;
    unsigned int offset;
    unsigned int end;
    unsigned int low;

    end = addr + length;
    low = addr - 0x600;
    count = 0;
    for (node = sound_sample_lru_tail; node != NULL; node = previous) {
        previous = node->previous;
        if (tag != node->unknown0C || node->unknown08 >= end || low >= node->unknown08) {
            continue;
        }
        if (node->unknown10 != 0x600 && node->unknown08 + node->unknown10 < addr + length) {
            if (addr + length >= node->unknown08 + 0x600) {
                size = 0x600 - node->unknown10;
            } else {
                size = (addr + length - node->unknown08 - node->unknown10 + 0xFF) & ~0xFF;
            }
            sound_dma_list_end += 12;
            sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
            ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].source = node->unknown08 + node->unknown10;
            ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].length = size;
            ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].destination = node->buffer + node->unknown10;
            sound_dma_list_count++;
            node->unknown10 += size;
        }
        if (node != sound_sample_lru_tail) {
            if (node->next != NULL) {
                node->next->previous = node->previous;
            }
            if (node->previous != NULL) {
                node->previous->next = node->next;
            } else {
                sound_sample_lru_head = node->next;
            }
            sound_sample_lru_tail->next = node;
            node->previous = sound_sample_lru_tail;
            node->next = NULL;
            sound_sample_lru_tail = node;
        }
        if (addr < node->unknown08) {
            pointers[count] = node->buffer;
            starts[count] = node->unknown08;
            lengths[count] = length - node->unknown08 + addr;
            length -= lengths[count];
            count++;
        } else if (node->unknown08 + node->unknown10 < addr + length) {
            pointers[count] = node->buffer + addr - node->unknown08;
            starts[count] = addr;
            lengths[count] = node->unknown08 + node->unknown10 - addr;
            length -= lengths[count];
            count++;
            addr = node->unknown08 + node->unknown10;
        } else {
            pointers[count] = node->buffer + addr - node->unknown08;
            starts[count] = addr;
            lengths[count] = length;
            length = 0;
            count++;
            break;
        }
    }
    if (length != 0) {
        offset = (addr - tag) % 1536;
        do {
            node = sound_hw_take_sample_block(tag);
            node->unknown0C = tag;
            node->unknown08 = addr - offset;
            if (length + offset > 0x600) {
                node->unknown10 = 0x600;
                sound_dma_list_end += 12;
            sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
                addr = addr - offset + 0x600;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].source = node->unknown08;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].length = 0x600;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].destination = node->buffer;
                sound_dma_list_count++;
                pointers[count] = node->buffer + offset;
                starts[count] = node->unknown08 + offset;
                lengths[count] = 0x600 - offset;
                length -= 0x600 - offset;
                count++;
            } else {
                node->unknown10 = (length + offset + 0xFF) & ~0xFF;
                sound_dma_list_end += 12;
            sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].source = node->unknown08;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].length = node->unknown10;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].destination = node->buffer;
                sound_dma_list_count++;
                pointers[count] = node->buffer + offset;
                starts[count] = node->unknown08 + offset;
                lengths[count] = length;
                length = 0;
                count++;
            }
            offset = 0;
        } while (length != 0);
    }
    if (count == 1) {
        fetch->first = pointers[0];
        fetch->first_length = lengths[0];
        fetch->second_length = 0;
        return;
    }
    if (starts[0] < starts[1]) {
        fetch->first = pointers[0];
        fetch->second = pointers[1];
        fetch->first_length = lengths[0];
        fetch->second_length = lengths[1];
        return;
    }
    fetch->first = pointers[1];
    fetch->second = pointers[0];
    fetch->first_length = lengths[1];
    fetch->second_length = lengths[0];
}

extern unsigned int sound_sfd_cache_cursor;
extern unsigned int sound_sfd_write_end;
/* PROMOTED 2026-10-21 — sound_hw_build_voice_command
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SoundHwVoiceView { unsigned char active, pending; unsigned short flags; unsigned short rate; unsigned char unknown06[2]; double position; unsigned char unknown10[8]; short level[4]; unsigned char unknown20[12]; unsigned char *data; unsigned int length; unsigned int loop_start; unsigned int loop_length; unsigned int tail; short target[4]; unsigned char unknown48[4]; float gain; unsigned char unknown50[13]; unsigned char format; unsigned char unknown5E[2]; unsigned char slot; } SoundHwVoiceView;
/* PC: 0x44 is the SFD's output pointer (set in sound_hw_build_subframe), so it is pointer-sized (docs/port/AUDIO.md). */
typedef struct SfdVoice { int volume[4]; int delta[4]; unsigned short fraction; unsigned short rate; SfdSampleRange main; SfdSampleRange loop; unsigned char main_blocks, loop_blocks, main_offset, loop_offset; union { void *book; struct { short main_count, loop_count; } pcm; } u; void *unknown44; short end48, end4A, position4C, start4E; } SfdVoice;
/* PC: the view's commands[] holds the most voices the PC gives (PC_MX_VOICE_SLOTS); an SFD holds pc_sfd_slots of them,
 * then its release mask's words for slots 32 and up, so the SFDs of a task step by pc_sfd_size() (PC_SFD), not by the
 * view's size (docs/port/AUDIO.md 10). At 32 slots it is the N64's SFD, sizeof(SfdBlock) with 32 commands. */
#define PC_SFD_COMMANDS PC_MX_VOICE_SLOTS
#define PC_SFD(T, base, i) (*(T *)((unsigned char *)(base) + (__SIZE_TYPE__)(i) * pc_sfd_size()))
typedef struct SfdBlock { unsigned short count; unsigned short sample; unsigned int changed; void *work; void *loop; SfdVoice commands[PC_SFD_COMMANDS]; } SfdBlock;
extern void pc_audio_set_sfd_voices(unsigned n);
extern __SIZE_TYPE__ pc_audio_sfd_size(unsigned n, int form);
extern void pc_log(const char *fmt, ...);
static unsigned pc_sfd_slots = 32;
static unsigned int pc_sfd_words(void) { return pc_sfd_slots > 32 ? (pc_sfd_slots - 32 + 31) / 32 : 0; }
static unsigned int pc_sfd_size(void)
{
    return (unsigned int)((__builtin_offsetof(SfdBlock, commands) + pc_sfd_slots * sizeof(SfdVoice) +
                           4 * pc_sfd_words() + 7) & ~7u);
}
static unsigned int pc_sfd_state_size(void) { return 664 + (pc_sfd_slots - 32) * 8; }
static void pc_sfd_layout(unsigned short count)
{
    pc_sfd_slots = count < 32 ? 32 : count;
    pc_audio_set_sfd_voices(pc_sfd_slots);
    if (pc_audio_sfd_size(pc_sfd_slots, 0) != pc_sfd_size())
        pc_log("audio: the game's SFD of %u voices is %u bytes, the interpreter's %u", pc_sfd_slots, pc_sfd_size(),
               (unsigned)pc_audio_sfd_size(pc_sfd_slots, 0));
}
extern double sound_truncate(double value);
extern void sound_hw_advance_envelope(SoundHwVoice *audio, unsigned short samples, unsigned char *active);
extern void sound_hw_advance_voice_position(SoundHwVoice *audio, unsigned short samples);
extern unsigned char *sound_hw_fetch_codebook(unsigned int source);
extern void sound_hw_fetch_sample_range(SfdSampleRange *fetch, unsigned int addr, unsigned int length, unsigned int tag);
#define V ((SoundHwVoiceView *)audio)
void sound_hw_build_voice_command(SoundHwVoice *audio, unsigned short samples, unsigned char index)
{
    unsigned int frames;
    unsigned int end;
    unsigned int position;
    unsigned char state;
    unsigned int excess;
    unsigned int address[2];
    unsigned int length[2];
    int offset;
    unsigned int mode;
    unsigned int padded[2];
    SfdVoice *cmd;
    unsigned short mainRem;
    unsigned short loopRem;
    unsigned short probe;

    cmd = &PC_SFD(SfdBlock, sound_sfd_current, *sound_sfd_count).commands[PC_SFD(SfdBlock, sound_sfd_current, *sound_sfd_count).count];
    sound_sfd_write_end += 0x50;
    sound_sfd_cache_cursor = dcache_create_dirty_lines(sound_sfd_cache_cursor, sound_sfd_write_end);
    frames = (unsigned int)(V->rate * 192 + 0xFFF) >> 12;
    frames += 6;
    position = sound_truncate(V->position);
    end = V->loop_start + V->loop_length;
    state = 2;
    if (V->loop_length != 0 && (V->pending != 0 || V->tail == 0) && end >= position) {
        if (end >= position + frames) {
            offset = 0;
            address[0] = position;
            length[0] = frames - 6;
            length[1] = 0;
            mode = -1;
        } else if (V->loop_start >= position) {
            address[0] = position;
            length[0] = V->loop_start - position + V->loop_length;
            length[1] = 0;
            offset = 0;
            mode = V->loop_start - position;
        } else if (V->loop_length < (excess = frames - end + position) || excess >= position - V->loop_start) {
            offset = position - V->loop_start;
            address[0] = V->loop_start;
            length[0] = V->loop_length;
            length[1] = 0;
            mode = 0;
        } else {
            offset = 0;
            address[0] = position;
            length[0] = end - position;
            address[1] = V->loop_start;
            length[1] = excess - 6;
            mode = 0;
        }
    } else {
        if (position + frames >= V->length) {
            address[0] = position;
            offset = 0;
            length[0] = V->length - position + 0x20;
            length[1] = 0;
            mode = -2;
            state = 0;
        } else {
            address[0] = position;
            offset = 0;
            length[0] = frames - 6;
            length[1] = 0;
            mode = -1;
        }
    }
    padded[0] = length[0] + 6;
    if (length[1] != 0) {
        padded[1] = length[1] + 6;
    }
    if (V->flags & 1) {
        switch (V->format) {
        case 3:
            if (length[1] != 0) {
                loopRem = address[1] & 0x3F;
                sound_hw_fetch_sample_range(&cmd->loop, (address[1] >> 6) * 40 + (unsigned int)V->data + 0x100, ((((padded[1] + (loopRem & 0x1F) + 0x1F) >> 5) + (loopRem >> 5) + 1) >> 1) * 40, (unsigned int)V->data + 0x100);
                cmd->loop_offset = loopRem;
                loopRem &= 0x1F;
                cmd->loop_blocks = (padded[1] + loopRem + 0x1F) >> 5;
            } else {
                cmd->loop.first = 0;
                cmd->loop_blocks = 0;
            }
            mainRem = address[0] & 0x3F;
            sound_hw_fetch_sample_range(&cmd->main, (address[0] >> 6) * 40 + (unsigned int)V->data + 0x100, ((((padded[0] + (mainRem & 0x1F) + 0x1F) >> 5) + (mainRem >> 5) + 1) >> 1) * 40, (unsigned int)V->data + 0x100);
            cmd->u.book = sound_hw_fetch_codebook((unsigned int)V->data);
            goto adpcm_tail;
        case 0:
            if (length[1] != 0) {
                loopRem = address[1] & 3;
                sound_hw_fetch_sample_range(&cmd->loop, (unsigned int)((address[1] & 0xFFFC) * 2 + V->data), ((padded[1] + loopRem + 3) & 0xFFFC) * 2, (unsigned int)V->data);
                cmd->u.pcm.loop_count = padded[1];
                cmd->loop_offset = loopRem;
            } else {
                cmd->loop.first = 0;
                cmd->u.pcm.loop_count = 0;
            }
            mainRem = address[0] & 3;
            sound_hw_fetch_sample_range(&cmd->main, (unsigned int)((address[0] & 0xFFFC) * 2 + V->data), ((padded[0] + mainRem + 3) & 0xFFFC) * 2, (unsigned int)V->data);
            goto pcm_tail;
        }
    } else {
        switch (V->format) {
        case 3:
            if (length[1] != 0) {
                loopRem = address[1] & 0x3F;
                cmd->loop.first = (address[1] >> 6) * 40 + V->data + 0x100;
                cmd->loop.first_length = ((((padded[1] + (loopRem & 0x1F) + 0x1F) >> 5) + loopRem / 32 + 1) >> 1) * 40;
                cmd->loop.second_length = 0;
                cmd->loop_offset = loopRem;
                loopRem &= 0x1F;
                cmd->loop_blocks = (padded[1] + loopRem + 0x1F) >> 5;
            } else {
                cmd->loop.first = 0;
                cmd->loop_blocks = 0;
            }
            mainRem = address[0] & 0x3F;
            cmd->main.first = (address[0] >> 6) * 40 + V->data + 0x100;
            cmd->main.first_length = ((((padded[0] + (mainRem & 0x1F) + 0x1F) >> 5) + mainRem / 32 + 1) >> 1) * 40;
            cmd->main.second_length = 0;
            cmd->u.book = V->data;
        adpcm_tail:
            cmd->main_offset = mainRem;
            mainRem &= 0x1F;
            cmd->main_blocks = (padded[0] + mainRem + 0x1F) >> 5;
            break;
        case 0:
            if (length[1] != 0) {
                loopRem = address[1] & 3 & 0xFFFF;
                cmd->loop.first = (address[1] & 0xFFFC) * 2 + V->data;
                cmd->loop.first_length = ((padded[1] + loopRem + 3) & 0xFFFC) * 2;
                cmd->loop.second_length = 0;
                cmd->u.pcm.loop_count = padded[1];
                cmd->loop_offset = loopRem;
            } else {
                cmd->loop.first = 0;
                cmd->u.pcm.loop_count = 0;
            }
            mainRem = address[0] & 3;
            cmd->main.first = (address[0] & 0xFFFC) * 2 + V->data;
            cmd->main.first_length = ((padded[0] + mainRem + 3) & 0xFFFC) * 2;
            cmd->main.second_length = 0;
        pcm_tail:
            cmd->u.pcm.main_count = padded[0];
            cmd->main_blocks = 0;
            cmd->main_offset = mainRem;
            break;
        }
    }
    cmd->start4E = offset;
    if (mode == -1) {
        cmd->end48 = 0x4000;
    } else if (mode == -2) {
        cmd->end48 = length[0] + mainRem;
        cmd->end4A = length[0] + mainRem - 0x20;
    } else {
        cmd->end48 = length[0] + mainRem;
        if (cmd->loop.first != 0) {
            cmd->end4A = (loopRem + mode) | 0x8000;
        } else {
            cmd->end4A = mainRem + mode;
        }
    }
    V->slot = index;
    sound_hw_advance_envelope(audio, 192, &state);
    if (V->active == 1) {
        cmd->volume[0] = (V->level[0] = V->target[0] * V->gain) << 16;
        cmd->volume[1] = (V->level[1] = V->target[1] * V->gain) << 16;
        cmd->volume[2] = (V->level[2] = V->target[2] * V->gain) << 16;
        cmd->volume[3] = (V->level[3] = V->target[3] * V->gain) << 16;
        cmd->delta[0] = 0;
        cmd->delta[1] = 0;
        cmd->delta[2] = 0;
        cmd->delta[3] = 0;
    } else {
        cmd->volume[0] = V->level[0] << 16;
        cmd->volume[1] = V->level[1] << 16;
        cmd->volume[2] = V->level[2] << 16;
        cmd->volume[3] = V->level[3] << 16;
        cmd->delta[0] = (((V->level[0] = V->target[0] * V->gain) << 16) - cmd->volume[0]) / 192;
        cmd->delta[1] = (((V->level[1] = V->target[1] * V->gain) << 16) - cmd->volume[1]) / 192;
        cmd->delta[2] = (((V->level[2] = V->target[2] * V->gain) << 16) - cmd->volume[2]) / 192;
        cmd->delta[3] = (((V->level[3] = V->target[3] * V->gain) << 16) - cmd->volume[3]) / 192;
    }
    cmd->rate = V->rate;
    cmd->fraction = (V->position - sound_truncate(V->position)) * 65536.0;
    cmd->position4C = (unsigned short)(padded[0] * (V->rate * 16)) + cmd->fraction;
    cmd->unknown44 = 0;
    PC_SFD(SfdBlock, sound_sfd_current, *sound_sfd_count).count++;
    sound_hw_advance_voice_position(audio, 192);
    V->active = state;
}

/* PROMOTED 2026-10-05 — sound_hw_begin_sfd_set
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_sfd_set;
extern unsigned short sound_sfd_counts[];
void sound_hw_begin_sfd_set(void)
{
    unsigned int cursor;
    sound_sfd_count = &sound_sfd_counts[sound_sfd_set];
    sound_sfd_current = sound_sfd_sets[sound_sfd_set];
    cursor = (unsigned int)sound_sfd_current + 16;
    sound_sfd_cache_cursor = cursor & ~15U;
    sound_sfd_write_end = cursor;
    *sound_sfd_count = 0;
    *sound_sfd_current = 0;
}

/* PROMOTED 2026-10-11 — sound_hw_build_subframe
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: these are N64 byte views of the SFD (SfdBlock) and the delay state (SoundDelayState), whose
 * pointers are host-sized natively, so the fields sit at the native records' offsets (docs/port/AUDIO.md).
 * status is main.first_length, output the pointer at 0x44 (unknown44). */
typedef SoundDelayState AudioLoop;
typedef union AudioCommand {
    SfdVoice command;
    struct { unsigned char pad_status[__builtin_offsetof(SfdVoice, main.first_length)]; unsigned short status; };
    struct { unsigned char pad_output[__builtin_offsetof(SfdVoice, unknown44)]; void *output; };
} AudioCommand;
typedef struct AudioBlock { unsigned short count; unsigned short sample; unsigned int changed; void *work; AudioLoop *loop; AudioCommand commands[PC_SFD_COMMANDS]; } AudioBlock;
typedef char pc_audio_block_check[sizeof(AudioBlock) == sizeof(SfdBlock) ? 1 : -1];
/* PC: a stopped voice's slot is its bit in the release mask: slots 0-31 in the header's word, as on the N64 (whose
 * shift takes the slot's low five bits, so a voice that never had a slot, 255, is bit 31), the others in the words
 * after the SFD's last voice record, which pc_sfd_mask_out writes with the header's (docs/port/AUDIO.md 10). */
static unsigned int pc_mask_ext[8];
static void pc_release(unsigned int *mask, unsigned char slot)
{
    if (slot < 32 || slot >= pc_sfd_slots)
        *mask |= 1U << (slot & 31);
    else
        pc_mask_ext[(slot - 32) / 32] |= 1U << (slot & 31);
}
static void pc_sfd_mask_out(AudioBlock *block)
{
    unsigned int *words = (unsigned int *)((unsigned char *)block + __builtin_offsetof(AudioBlock, commands) +
                                           pc_sfd_slots * sizeof(AudioCommand));
    unsigned int k;
    for (k = 0; k < pc_sfd_words(); k++) {
        words[k] = pc_mask_ext[k];
        pc_mask_ext[k] = 0;
    }
}
#define PC_RELEASE(mask, slot) pc_release(&(mask), (slot))
extern unsigned short sound_hw_voice_count;
extern unsigned short sound_sfds_per_task;
extern int sound_delay_position;
extern void sound_hw_build_voice_command(SoundHwVoice *, unsigned short, unsigned char);
void sound_hw_build_subframe(void *output, unsigned short samples)
{
    unsigned short i;
    unsigned char active;
    unsigned int changed;
    active = 0;
    changed = 0;
    for (i = 0; i < sound_hw_voice_count; i++) {
        if (sound_hw_voices[i].release_pending != 0) {
            sound_hw_voices[i].release_pending = 0;
            PC_RELEASE(changed, sound_hw_voices[i].unknown60);
        }
        if (sound_hw_voices[i].active != 0) {
            sound_hw_build_voice_command(&sound_hw_voices[i], samples, active);
            active++;
        }
    }
    if (PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).count == 0) {
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands[0].status = 0;
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands[0].output = output;
    } else {
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands[PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).count - 1].output = output;
    }
    PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).work = sound_mix_state;
    PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).changed = changed; pc_sfd_mask_out(&PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count));
    PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).loop = sound_delay;
    if (PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).loop != 0) {
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).sample = sound_delay_position / 192;
        sound_delay_position += 192;
        if (sound_delay_position == ((AudioLoop *)sound_delay)->length) {
            sound_delay_position = 0;
        }
    }
    (*sound_sfd_count)++;
    if (*sound_sfd_count < sound_sfds_per_task) {
        sound_sfd_cache_cursor = (unsigned int)PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands & ~15U;
        sound_sfd_write_end = (unsigned int)PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands;
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).count = 0;
    }
}

/* PROMOTED 2026-10-05 — sound_hw_latch_voice_positions
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
extern unsigned short sound_hw_voice_count;
void sound_hw_latch_voice_positions(void)
{
    int i;
    for (i = 0; i < sound_hw_voice_count; i++) {
        if (sound_hw_voices[i].active != 0) {
            sound_hw_voices[i].position = sound_hw_voices[i].current;
            sound_hw_voices[i].current = (unsigned int) sound_hw_voices[i].sample_position;
        }
    }
}

/* PROMOTED 2026-10-04 — sound_hw_render_subframe
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern unsigned int sound_tick_countdown;
extern unsigned int sound_subframes_per_tick;
extern void *sound_output_buffers[];
extern void sound_seq_tick(void);
extern void sound_synth_tick(void);
extern void sound_hw_build_subframe(void *, unsigned short);
int sound_hw_render_subframe(unsigned short index, unsigned short count)
{
    if (sound_initialized != 0) {
        if (--sound_tick_countdown == 0) {
            sound_tick_countdown = sound_subframes_per_tick;
            sound_seq_tick();
            sound_synth_tick();
        }
    }
    sound_hw_build_subframe(sound_output_buffers[index], count);
    return 1;
}

/* PROMOTED 2026-10-11 — sound_hw_service_frame
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void (*sound_dma_flush_hook)(void);
extern void (*sound_frame_hook)(void);
extern unsigned short *sound_sfd_sets[];
extern unsigned short sound_render_index;
extern volatile short sound_drain_count;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int);
extern void sound_hw_lock_frame(void);
extern void sound_hw_unlock_frame(void);
extern void sound_emitter_update_all(void);
extern void sound_hw_wait_dma(void);
extern void sound_hw_reset_dma_list(void);
extern void sound_hw_age_codebook_cache(void);
extern void sound_hw_begin_sfd_set(void);
extern void sound_hw_latch_voice_positions(void);
extern int sound_hw_render_subframe(unsigned short, unsigned short);
extern void sound_hw_start_dma(void);
extern void sound_stream_flush_wraps(void);
extern void sound_hw_submit_sfds(void);
extern void sound_stream_refill(void);
/* PC: the wrapping range's second loop starts at the ring's start only once the first loop got to its end. Stopped
 * short by the per-task cap, the N64's loop set index to 0 anyway, and the buffers between were never rendered:
 * they played their last turn's sound again. The PC's retraces come in bunches (a swap waiting for vsync), so more
 * buffers than the cap are often passed between two of them (docs/port/AUDIO.md 7). */
#define PC_RING_START(index) ((index) >= sound_output_buffer_count ? 0 : (index))
/* PC: the last callback's task reads the SFD buffer this one builds into (sound_hw_begin_sfd_set starts it). On the N64 the RSP
 * has run it long before; on the PC an audio task can wait behind a graphics task the main loop has begun, and a
 * loaded host can hold it past the next retrace. Built over, that task renders
 * this callback's SFDs instead of its own, which are lost, and the next two render the sets out of order, one of
 * them twice. So wait for it first (sound_hw_wait_task waits once; the wait before the submit then returns at once). */
#define PC_SFD_WAIT() sound_hw_wait_task();
void sound_hw_service_frame(void)
{
    unsigned short index;
    unsigned short offset;
    unsigned short count;
    unsigned short target;
    unsigned int mask;
    if (sound_frame_hook != 0) {
        sound_frame_hook();
    }
    sound_hw_lock_frame();
    mask = __osDisableInt();
    if (sound_output_index == 0) {
        target = sound_output_buffer_count - 1;
    } else {
        target = sound_output_index - 1;
    }
    __osRestoreInt(mask);
    if (sound_initialized != 0) {
        sound_emitter_update_all();
    }
    if (sound_render_index != 0xFFFF) {
        sound_hw_wait_dma();
        sound_hw_reset_dma_list();
        if (sound_dma_flush_hook != 0) {
            sound_dma_flush_hook();
        }
        sound_hw_age_codebook_cache();
        PC_SFD_WAIT() sound_hw_begin_sfd_set();
        if (sound_render_index != target) {
            sound_hw_latch_voice_positions();
            offset = 0;
            count = 0;
            if (sound_render_index < target) {
                for (index = sound_render_index; index < target && count < sound_sfds_per_task; offset += 192, count++) {
                    sound_hw_render_subframe(index++, offset);
                }
            } else {
                for (index = sound_render_index; index < sound_output_buffer_count && count < sound_sfds_per_task; offset += 192, count++) {
                    sound_hw_render_subframe(index++, offset);
                }
                for (index = PC_RING_START(index); index < target && count < sound_sfds_per_task; offset += 192, count++) {
                    sound_hw_render_subframe(index++, offset);
                }
            }
            sound_hw_start_dma();
            sound_drain_count -= count;
            sound_render_index = index;
        }
        sound_stream_flush_wraps();
        sound_sfd_count = &sound_sfd_counts[sound_sfd_set ^ 1];
        sound_sfd_current = sound_sfd_sets[sound_sfd_set ^ 1];
        sound_hw_wait_task();
        sound_hw_submit_sfds();
        sound_sfd_set ^= 1;
        if (sound_initialized != 0) {
            sound_stream_refill();
        }
    } else {
        sound_render_index = target;
    }
    sound_hw_unlock_frame();
}

/* PROMOTED 2026-10-04 — sound_hw_install_callback
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned short sound_render_index;
extern void (*sound_dma_flush_hook)(void);
extern void (*sound_frame_hook)(void);
extern void sound_hw_service_frame(void);
extern void (*sound_service_add_callback)(void (*)(void));
void sound_hw_install_callback(void)
{
    sound_render_index = 0xFFFF;
    sound_dma_flush_hook = 0;
    sound_frame_hook = 0;
    sound_service_add_callback(sound_hw_service_frame);
}

/* PROMOTED 2026-10-04 — sound_hw_remove_callback
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned short sound_output_buffer_count;
extern volatile short sound_drain_count;
extern void (*sound_service_remove_callback)(void (*)(void));
void sound_hw_remove_callback(void)
{
    sound_drain_count = sound_output_buffer_count;
    while (sound_drain_count > 0) { pc_os_spin_yield("sound_hw_remove_callback");
    }
    sound_service_remove_callback(sound_hw_service_frame);
}

/* PROMOTED 2026-10-11 — sound_hw_init_synth
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct AudioConfiguration { unsigned int frequency; unsigned char mode; unsigned char sample_bits; unsigned char unknown06[256]; signed char text[256]; } AudioConfiguration;
extern unsigned int sound_samples_per_tick;
extern AudioConfiguration sound_output_info;
extern signed char sound_driver_name[];
extern signed char sound_driver_name_copy[];
extern void sound_hw_alloc_voices(unsigned short, unsigned int);
extern void sound_hw_alloc_dma(unsigned short, unsigned int, unsigned char);
extern void sound_hw_alloc_delay_line(unsigned int, unsigned short);
extern void sound_hw_install_callback(void);
#define PC_MX_CAP(n) PC_MX_VOICE_SLOTS   /* the most voices the PC gives (pc_game_types.h, docs/port/AUDIO.md 10) */
void sound_hw_init_synth(unsigned int *frequency, unsigned short count,
                   unsigned short size, unsigned char mode)
{
    unsigned char i;
    float frame_samples;
    if (count > PC_MX_CAP(32)) {
        count = PC_MX_CAP(32);
    }
    sound_task_busy = 0;
    sound_hw_alloc_voices(count, *frequency);
    sound_hw_alloc_dma(count, *frequency, mode);
    sound_hw_alloc_delay_line(*frequency, size);
    sound_hw_install_callback();
    frame_samples = (float)*frequency / 60.0f;
    sound_subframes_per_tick = (unsigned int)(frame_samples / 192.0f + 0.5f);
    sound_tick_countdown = sound_subframes_per_tick;
    sound_samples_per_tick = sound_subframes_per_tick * 192;
    sound_output_info.frequency = *frequency;
    sound_output_info.sample_bits = 16;
    sound_output_info.mode = 1;
    for (i = 0; sound_driver_name[i] != 0; i++) {
        sound_output_info.text[i] = sound_driver_name_copy[i];
    }
    sound_output_info.text[i] = 0;
}

/* PROMOTED 2026-10-04 — sound_hw_cache_mode_from_flags
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_hw_cache_mode_from_flags(unsigned int flags)
{
    unsigned char result;
    if (flags & 0x10000) {
        result = 2;
    } else if (flags & 0x20000) {
        result = 3;
    } else if (flags & 0x40000) {
        result = 4;
    } else if (flags & 0x80000) {
        result = 0;
    } else {
        result = 1;
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_hw_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_half_rate;
extern unsigned char sound_output_frozen;
extern void sound_hw_init_lock(void);
extern void sound_hw_start_output_thread(unsigned int *);
extern unsigned char sound_hw_cache_mode_from_flags(unsigned int);
extern void sound_hw_init_synth(unsigned int *, unsigned short, unsigned short, unsigned char);
int sound_hw_init(unsigned int *frequency, unsigned short count, unsigned short size, unsigned int flags)
{
    sound_output_frozen = 0;
    sound_hw_init_lock();
    sound_half_rate = (flags & 0x100000) != 0;
    sound_hw_start_output_thread(frequency);
    sound_hw_init_synth(frequency, count, size, sound_hw_cache_mode_from_flags(flags));
    return 0;
}

/* PROMOTED 2026-10-04 — sound_hw_init_shared_output
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_set_output_frequency(unsigned int *);
int sound_hw_init_shared_output(unsigned int *frequency, unsigned short count, unsigned short size, unsigned int flags)
{
    sound_hw_init_lock();
    sound_hw_set_output_frequency(frequency);
    sound_hw_init_synth(frequency, count, size, sound_hw_cache_mode_from_flags(flags));
    return 0;
}

/* PROMOTED 2026-10-04 — sound_hw_exit
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_remove_callback(void);
extern void sound_hw_free_delay_line(void);
extern void sound_hw_free_dma(void);
extern void sound_hw_free_codebook_cache(void);
extern void sound_hw_free_sample_cache(void);
extern void sound_hw_free_voices(void);
extern void sound_hw_stop_output_thread(void);
void sound_hw_exit(void)
{
    if (sound_initialized != 0) {
        sound_hw_remove_callback();
        sound_hw_wait_task();
        sound_hw_free_delay_line();
        sound_hw_free_dma();
        sound_hw_free_codebook_cache();
        sound_hw_free_sample_cache();
        sound_hw_free_voices();
        sound_hw_stop_output_thread();
    }
}

/* PROMOTED 2026-10-04 — sound_hw_exit_shared_output
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_exit_shared_output(void)
{
    if (sound_initialized != 0) {
        sound_hw_remove_callback();
        sound_hw_wait_task();
        sound_hw_free_delay_line();
        sound_hw_free_dma();
        sound_hw_free_codebook_cache();
        sound_hw_free_sample_cache();
        sound_hw_free_voices();
    }
}

/* PROMOTED 2026-10-04 — sound_hw_init_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct MessageQueue MessageQueue;
extern MessageQueue sound_lock_queue;
extern void *sound_lock_message;
void sound_hw_init_lock(void)
{
    osCreateMesgQueue(&sound_lock_queue, &sound_lock_message, 1);
    osSendMesg(&sound_lock_queue, 0, 0);
}

/* PROMOTED 2026-10-04 — sound_hw_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_lock_depth;
void sound_hw_lock(void)
{
    if (sound_lock_depth == 0) {
        osRecvMesg(&sound_lock_queue, 0, 1);
    }
    sound_lock_depth++;
}

/* PROMOTED 2026-10-04 — sound_hw_unlock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_unlock(void)
{
    if (sound_lock_depth > 0) {
        if (--sound_lock_depth == 0) {
            osSendMesg(&sound_lock_queue, 0, 0);
        }
    }
}

/* PROMOTED 2026-10-04 — sound_hw_lock_frame
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_lock_frame(void)
{
    osRecvMesg(&sound_lock_queue, 0, 1);
}

/* PROMOTED 2026-10-11 — sound_hw_unlock_frame
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#define NULL ((void *)0)
#define OS_MESG_NOBLOCK 0
extern s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void sound_hw_unlock_frame(void) {
    osSendMesg(&sound_lock_queue, NULL, OS_MESG_NOBLOCK);
}

/* PROMOTED 2026-10-11 — sound_hw_is_voice_active
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the synth's tick found driver voice `index` playing with its macro ended (synth.c): counted when its sample
 * loops, which then plays until the voice is taken again (--log-voices, docs/port/AUDIO.md 10.5). */
void pc_mx_note_macroless(int index)
{
    if (sound_hw_voices[index].loop_length != 0)
        pc_snd_count(PC_SND_MX_LOOP_ORPHAN);
}
unsigned char sound_hw_is_voice_active(int index)
{
    return sound_hw_voices[index].active != 0;
}

/* PROMOTED 2026-10-04 — sound_hw_is_busy
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_hw_is_busy(void)
{
    return 0;
}

/* PROMOTED 2026-10-05 — sound_hw_set_voice_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
#pragma pack(1)
typedef struct SoundHwSampleInfo { unsigned int frequency; void *data; unsigned int offset; unsigned int length; unsigned int loopStart; unsigned int loopLength; unsigned char format; } SoundHwSampleInfo;
#pragma pack()
extern void sound_hw_reset_voice(unsigned short, unsigned short);
/* PC: a host pointer (at or above 4 GiB) is RAM too; only cartridge addresses are fetched by DMA. */
void sound_hw_set_voice_sample(unsigned int index, SoundHwSampleInfo *sample, unsigned char reset)
{
    sound_hw_reset_voice(index, 0);
    if (reset) {
        sound_hw_voices[index].initial_count = 0;
        sound_hw_voices[index].value22 = 0;
        sound_hw_voices[index].value24 = 1.0f;
        sound_hw_voices[index].release_count = 20;
    }
    sound_hw_voices[index].data = sample->data;
    sound_hw_voices[index].sample_position = sample->offset;
    sound_hw_voices[index].current = sound_hw_voices[index].position = sample->offset;
    sound_hw_voices[index].length = sample->length;
    sound_hw_voices[index].loop_start = sample->loopStart;
    sound_hw_voices[index].loop_length = sample->loopLength;
    sound_hw_voices[index].format = sample->format;
    if (sound_hw_voices[index].loop_length != 0) {
        sound_hw_voices[index].tail = sound_hw_voices[index].length -
            sound_hw_voices[index].loop_length - sound_hw_voices[index].loop_start;
        if (sound_hw_voices[index].tail < 10) sound_hw_voices[index].tail = 0;
    } else {
        sound_hw_voices[index].tail = 0;
    }
    if (((uintptr_t)sample->data >> 32) == 0 && ((unsigned long)sample->data & 0xF0000000UL) != 0x80000000UL) {
        sound_hw_voices[index].flags |= 1;
    }
}

/* PROMOTED 2026-10-11 — sound_hw_stop_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_stop_sample(int index, unsigned char release)
{
    sound_hw_voices[index].release_count = 20;
    if (release) {
        sound_hw_release_envelope(&sound_hw_voices[index]);
    }
}

/* PROMOTED 2026-10-04 — sound_hw_set_envelope
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#define __unaligned   /* IDO's qualifier; the PC targets load unaligned u16 natively */
void sound_hw_set_envelope(int index, __unaligned unsigned short *data)
{
    sound_hw_voices[index].value20 = data[0];
    sound_hw_voices[index].value22 = data[1];
    sound_hw_voices[index].value24 = (float)(unsigned int)data[2] * (1.0 / 4096.0);
    sound_hw_voices[index].release_count = data[3];
}

/* PROMOTED 2026-10-04 — sound_hw_start_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_activate_voice(unsigned short);
void sound_hw_start_voice(unsigned int index)
{
    sound_hw_activate_voice((unsigned short)index);
}

/* PROMOTED 2026-10-05 — sound_hw_release_hold
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
void sound_hw_release_hold(int index)
{
    sound_hw_voices[index].pending = 0;
}

/* PROMOTED 2026-10-04 — sound_hw_set_priority
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_set_priority(int unused)
{
}

/* PROMOTED 2026-10-11 — sound_hw_set_pitch
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_set_pitch(int index, unsigned short rate)
{
    if (rate <= 8192) {
        sound_hw_voices[index].rate = rate;
    } else {
        sound_hw_voices[index].rate = 8192;
    }
}

/* PROMOTED 2026-10-05 — sound_hw_set_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
extern void sound_compute_gains(unsigned short *, unsigned short *, unsigned int, unsigned int, unsigned int, unsigned short *, unsigned int, unsigned short *);
void sound_hw_set_volume(int index, unsigned int volume, unsigned int pan, unsigned int span, unsigned int aux)
{
    AudioSlot *slot;
    slot = &sound_hw_voices[index];
    sound_compute_gains(&slot->value42, &slot->value40, volume, pan, span,
                  &slot->value44, aux, &slot->value46);
}

/* PROMOTED 2026-10-05 — sound_hw_break_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
void sound_hw_break_voice(int index)
{
    if (sound_hw_voices[index].active) {
        sound_hw_voices[index].active = 0;
        sound_hw_voices[index].release_pending = 1;
    }
}

/* PROMOTED 2026-10-05 — sound_hw_key_off
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
void sound_hw_key_off(int index)
{
    if (sound_hw_voices[index].active != 0) {
        sound_hw_voices[index].release_count = 20;
        if (sound_hw_voices[index].pending != 0) {
            sound_hw_voices[index].pending = 0;
        } else {
            sound_hw_release_envelope(&sound_hw_voices[index]);
        }
    }
}

/* PROMOTED 2026-10-05 — sound_hw_kill_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
void sound_hw_kill_voice(int index)
{
    sound_hw_voices[index].active = 0;
}

/* PROMOTED 2026-10-11 — sound_hw_set_delay_effect
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_set_delay(void *state);
void sound_hw_set_delay_effect(void *state)
{
    sound_hw_set_delay(state);
}

/* PROMOTED 2026-10-11 — sound_hw_remove_delay_effect
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_remove_delay_effect(void)
{
    sound_hw_remove_delay();
}

/* PROMOTED 2026-10-05 — sound_hw_get_voice_position
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_hw_voice.h"
unsigned int sound_hw_get_voice_position(int index)
{
    return sound_hw_voices[index].position;
}

/* PROMOTED 2026-10-04 — sound_hw_flush_stream
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void osWritebackDCache(void *, int);
void sound_hw_flush_stream(void *buffer, unsigned int samples)
{
    osWritebackDCache(buffer, samples * 2);
}

/* PROMOTED 2026-10-11 — sound_hw_flush_stream_wrap
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_flush_stream_wrap(short *buffer, unsigned int samples)
{
    buffer[samples + 0] = buffer[0];
    buffer[samples + 1] = buffer[1];
    buffer[samples + 2] = buffer[2];
    buffer[samples + 3] = buffer[3];
    osWritebackDCache(&buffer[samples], 8);
}

/* PROMOTED 2026-10-04 — sound_hw_translate_address
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *(*sound_service_translate_address)(void *);
/* PC: a host pointer (at or above 4 GiB) is RAM, not a cartridge offset: returned as it is; so is a K0 address past
 * 16 MiB of a larger window (game.memory, docs/port/MEMORY.md 3), where the N64's region test sees no RAM. */
void *sound_hw_translate_address(void *address)
{
    unsigned int region;
    if ((uintptr_t)address >> 32 || pc_rdram_k0_high((unsigned int)(uintptr_t)address)) return address; region = (unsigned int)address & 0xFF000000;
    if (region != 0x80000000 && region != 0xB0000000) {
        address = sound_service_translate_address(address);
    }
    return address;
}

/* PROMOTED 2026-10-11 — sound_hw_init_sample_memory
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_init_sample_memory(void)
{
}

/* PROMOTED 2026-10-11 — sound_hw_save_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_save_sample(void **descriptor, void **data)
{
}

/* PROMOTED 2026-10-11 — sound_hw_remove_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_remove_sample(void **descriptor, void *data)
{
}

/* PROMOTED 2026-10-11 — sound_hw_commit_voices
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_commit_voices(void)
{
}

/* PROMOTED 2026-10-04 — sound_hw_freeze_output
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_hw_freeze_output(void)
{
    sound_output_frozen = 1;
}

