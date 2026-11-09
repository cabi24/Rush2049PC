/* src/rom/sound/hardware.c: the MusyX sound library's hardware layer.
 *
 * The bottom of the library, under the synth (synth.c, voices.c, macros.c). It keeps the hardware voices
 * (SoundHwVoice, include/sound_hw_voice.h: a sample, its play position, pitch, gains and ADSR envelope) and, once a
 * retrace, in sound_hw_service_frame (installed as the game's retrace callback), runs the library's 60 Hz tick when
 * it is due and describes the next stretch of output as subframe descriptors (SFDs): one record per playing voice,
 * 192 output frames each, for the RSP's MusyX microcode. The sample blocks and ADPCM codebooks the voices need come
 * from the cartridge into two caches by one DMA list a frame. Also here: the library lock, the AI output thread and
 * the delay (echo) effect's line, which Rush 2049 never turns on. MusyX is the N64's sound library; MAT3, the
 * GameCube build, has its own sound code, so this has no MAT3 counterpart.
 *
 * On the PC the microcode is not run: port/audio interprets the SFDs (docs/port/AUDIO.md has their layout), and the
 * AI, the RSP task, the D-cache calls and the cartridge DMA under this code are port/ultra's. The records here are in host
 * layout; an SFD grows past 32 voice records when more voices are asked for ([audio] voices, pc_sfd_layout). Still
 * N64-shaped: samples are addressed by 32-bit cartridge or RDRAM-window addresses, and the cache-line bookkeeping
 * (dcache_create_dirty_lines, the *_cache_cursor and *_end words) counts the N64's record sizes. */
#include "../rom_tu.h"
#include "sound_hw_voice.h"

/* This unit's own BSS: the delay line, the audio task and the codebook cache. */
void *sound_delay_line;
unsigned int sound_delay_length;
OSTask sound_task;
struct AudioNode *sound_codebook_nodes;
unsigned char *sound_codebook_cache;

/* ---- Constants */

/* A subframe: one SFD renders 192 stereo output frames, one 768-byte AI buffer (192 x 2 x s16). */
#define SUBFRAME_FRAMES 192
#define OUTPUT_BUFFER_BYTES 768
/* The output thread: its stack and priority, and its messages (the AI's "buffer taken" event, and stop). */
#define OUTPUT_THREAD_STACK_BYTES 1024
#define OUTPUT_THREAD_PRIORITY 122
#define OUTPUT_MSG_BUFFER_DONE 1
#define OUTPUT_MSG_STOP 255
#define OS_EVENT_AI 6                  /* libultra's event number for the AI */

/* SoundHwVoice.active */
enum { VOICE_OFF, VOICE_STARTING, VOICE_PLAYING };
/* SoundHwVoice.envelope_stage: the ADSR stages (the times are in milliseconds) */
enum { ENVELOPE_ATTACK, ENVELOPE_DECAY, ENVELOPE_SUSTAIN, ENVELOPE_RELEASE };
/* SoundHwVoice.format */
#define SAMPLE_FORMAT_PCM16 0
#define SAMPLE_FORMAT_ADPCM 3
/* SoundHwVoice.flags: the sample is in the cartridge, so its data is fetched into the caches by DMA */
#define VOICE_FLAG_CARTRIDGE 1
/* The release time a key-off or a stop uses (ms) */
#define DEFAULT_RELEASE_MS 20
/* SoundHwVoice.rate: 1.0 (the sample's own rate at the output rate) and the most it may be */
#define PITCH_UNITY 4096
#define PITCH_MAX 8192
/* sound_hw_build_voice_command: the subframe's input window has no end point, or the sample ends in it */
#define WINDOW_NO_END (-1)
#define WINDOW_SAMPLE_ENDS (-2)

/* An ADPCM sample: a 256-byte codebook, then blocks of 64 samples in 40 bytes (two 32-sample frames) */
#define ADPCM_CODEBOOK_BYTES 0x100
#define ADPCM_BLOCK_BYTES 40
/* The sample cache's blocks, and how many frames an unused codebook stays cached */
#define SAMPLE_BLOCK_BYTES 0x600
#define CODEBOOK_KEEP_FRAMES 2
/* sound_render_index before the first callback, which only notes where the AI is */
#define RENDER_INDEX_NONE 0xFFFF

/* sound_hw_init's flags: half-rate output (the sample-cache classes are sound_hw_cache_mode_from_flags') */
#define SOUND_INIT_HALF_RATE 0x100000

/* The N64's record sizes, which the D-cache bookkeeping and writebacks still count by */
#define DMA_ENTRY_N64_BYTES 12          /* a DMA list entry */
#define SFD_N64_BYTES 2576              /* an SFD: a 16-byte header and 32 voice records */
#define SFD_HEADER_N64_BYTES 16
#define SFD_VOICE_N64_BYTES 0x50
#define DELAY_STATE_N64_BYTES 72
#define AUDIO_UCODE_TEXT_BYTES 4096

/* sizeof(SoundDmaEntryCodebook): 12 bytes on the N64, where the destination is a 32-bit pointer */
#define DMA_ENTRY_SIZE 16U

/* ---- Types */

/* The delay effect: the configuration sound_hw_set_delay is given (times in ms), and the parameter block it builds
 * for the microcode (times in samples). Rush 2049 never sets one. */
typedef struct SoundDelayConfig { unsigned short duration; unsigned short tap_delays[8]; unsigned char tap_gains[8]; unsigned char tap_count; unsigned char feedback; unsigned char mode; unsigned char pad1D; short filter[4]; } SoundDelayConfig;
typedef struct SoundDelayState { void *buffer; unsigned int length; unsigned short tap_count; unsigned short feedback; unsigned int tap_delays[8]; unsigned short tap_gains[8]; unsigned int unused3C; short filter[4]; } SoundDelayState;
#define DELAY_STATE_SIZE sizeof(SoundDelayState)   /* 72 bytes on the N64; buffer is a host pointer */

/* A codebook cache entry: 256 bytes of buffer, the cartridge address it holds, frames left before it may be reused */
typedef struct AudioNode { struct AudioNode *next; struct AudioNode *previous; unsigned char *buffer; unsigned int source; unsigned short countdown; unsigned short pad12; } AudioNode;
/* A sample cache block: 0x600 bytes of buffer holding a sample's bytes from address on; loaded of them have
 * arrived. sample_base (the sample's first byte) says whose block it is. */
typedef struct AudioCacheNode { struct AudioCacheNode *next; struct AudioCacheNode *previous; unsigned int address; unsigned int sample_base; unsigned int loaded; unsigned char *buffer; } AudioCacheNode;

/* A DMA list entry, as the codebook and the sample fetches write them (the game's DMA service reads the list) */
typedef struct SoundDmaEntryCodebook { int length; unsigned int source; unsigned char *destination; } SoundDmaEntryCodebook;
typedef char pc_dma_entry_check[sizeof(SoundDmaEntryCodebook) == DMA_ENTRY_SIZE ? 1 : -1];
typedef struct SoundDmaEntrySamples { int length; unsigned int source; unsigned char *destination; } SoundDmaEntrySamples;

/* One segment of a voice's input for a subframe, in memory: one range, or two when it spans two cache blocks */
typedef struct SfdSampleRange { unsigned char *first; unsigned char *second; short first_length; short second_length; } SfdSampleRange;

/* An SFD's voice record (port/audio/musyx_abi.h, docs/port/AUDIO.md 2): gains per channel (left, right, surround,
 * aux) and their per-frame steps, the input position and rate, the main and loop segments, ADPCM frames and first
 * samples, the codebook (or PCM16 sample counts), the output buffer (on the last record only), the end, restart and
 * start points.
 * PC: output is the SFD's output pointer (set in sound_hw_build_subframe), so it is pointer-sized (docs/port/AUDIO.md). */
typedef struct SfdVoice { int volume[4]; int delta[4]; unsigned short fraction; unsigned short rate; SfdSampleRange main; SfdSampleRange loop; unsigned char main_blocks, loop_blocks, main_offset, loop_offset; union { void *book; struct { short main_count, loop_count; } pcm; } u; void *output; short end, restart, unused4C, start; } SfdVoice;
/* PC: the view's commands[] holds the most voices the PC gives (PC_MX_VOICE_SLOTS); an SFD holds pc_sfd_slots of them,
 * then its release mask's words for slots 32 and up, so the SFDs of a task step by pc_sfd_size() (PC_SFD), not by the
 * view's size (docs/port/AUDIO.md 10). At 32 slots it is the N64's SFD, sizeof(SfdBlock) with 32 commands. */
#define PC_SFD_COMMANDS PC_MX_VOICE_SLOTS
#define PC_SFD(T, base, i) (*(T *)((unsigned char *)(base) + (__SIZE_TYPE__)(i) * pc_sfd_size()))
/* An SFD: its voice count, the delay line's subframe index, the release mask (a bit per slot whose voice stopped
 * short), the mix state block, the delay parameters (or 0), the voice records. */
typedef struct SfdBlock { unsigned short count; unsigned short sample; unsigned int changed; void *work; void *loop; SfdVoice commands[PC_SFD_COMMANDS]; } SfdBlock;

/* sound_hw_build_subframe's view of an SFD: the same record, with the delay parameters typed, the first voice's
 * main.first_length read as a status word and the output pointer by name. */
typedef SoundDelayState AudioLoop;
typedef union AudioCommand {
    SfdVoice command;
    struct { unsigned char pad_status[__builtin_offsetof(SfdVoice, main.first_length)]; unsigned short status; };
    struct { unsigned char pad_output[__builtin_offsetof(SfdVoice, output)]; void *output; };
} AudioCommand;
typedef struct AudioBlock { unsigned short count; unsigned short sample; unsigned int changed; void *work; AudioLoop *loop; AudioCommand commands[PC_SFD_COMMANDS]; } AudioBlock;
typedef char pc_audio_block_check[sizeof(AudioBlock) == sizeof(SfdBlock) ? 1 : -1];

/* What sound_get_output_info reports: the output rate, mode, sample bits and the driver's name */
typedef struct AudioConfiguration { unsigned int frequency; unsigned char mode; unsigned char sample_bits; unsigned char unknown06[256]; signed char text[256]; } AudioConfiguration;

/* A sample, as the synth hands it to sound_hw_set_voice_sample (offset, lengths and loop in samples) */
#pragma pack(1)
typedef struct SoundHwSampleInfo { unsigned int frequency; void *data; unsigned int offset; unsigned int length; unsigned int loopStart; unsigned int loopLength; unsigned char format; } SoundHwSampleInfo;
#pragma pack()

/* ---- What the unit uses from elsewhere */

/* The library's state */
extern unsigned char sound_initialized;
extern int sound_lock_depth;
extern OSMesgQueue sound_lock_queue;
extern void *sound_lock_message;
extern unsigned int sound_tick_countdown;
extern unsigned int sound_subframes_per_tick;
extern unsigned int sound_samples_per_tick;
extern AudioConfiguration sound_output_info;
extern signed char sound_driver_name[];
extern signed char sound_driver_name_copy[];
/* The output and its thread */
extern unsigned char sound_half_rate;
extern unsigned char sound_output_frozen;
extern volatile unsigned char sound_output_index;
extern unsigned short sound_output_buffer_count;
extern unsigned int sound_output_frequency;
extern void *sound_output_buffers[];
extern unsigned char sound_output_queue[];
extern unsigned char sound_output_event;
extern void *sound_output_messages[];
extern unsigned char *sound_output_stack;
extern unsigned char sound_output_thread[];
/* The delay effect */
extern unsigned int sound_delay_length;
extern void *sound_delay;
extern void *sound_delay_line;
extern int sound_delay_position;
/* The voices and the SFDs */
extern void *sound_hw_voice_block;
extern unsigned short sound_hw_voice_count;
extern unsigned char sound_sfd_set;
extern float sound_sfd_margin;
extern float sound_sfd_margin_half_rate;
extern unsigned short sound_sfds_per_task;
extern unsigned short *sound_sfd_sets[2];
extern unsigned short sound_sfd_counts[2];
extern unsigned short *sound_sfd_current;
extern unsigned short *sound_sfd_count;
extern unsigned int sound_sfd_cache_cursor;
extern unsigned int sound_sfd_write_end;
extern void *sound_mix_state;
extern unsigned short sound_render_index;
extern volatile short sound_drain_count;
extern volatile unsigned char sound_task_busy;
/* The DMA list and the caches */
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
extern unsigned int sound_dma_list_end;
extern unsigned int sound_dma_list_cache_cursor;
extern unsigned short sound_dma_list_count;
extern AudioNode *sound_codebook_used;
extern AudioNode *sound_codebook_nodes;
extern unsigned char *sound_codebook_cache;
extern unsigned int sound_codebook_last;
extern AudioNode *sound_codebook_free;
extern AudioCacheNode *sound_sample_lru_head;
extern AudioCacheNode *sound_sample_lru_tail;
extern AudioCacheNode *sound_sample_free;
extern AudioCacheNode *sound_sample_nodes;
extern unsigned char *sound_sample_cache;
/* The microcode */
extern unsigned char rspbootTextStart[];
extern unsigned char rspbootTextEnd[];
extern unsigned char audio_ucode_text[];
extern unsigned char audio_ucode_data[];
/* The game's services (sound_set_services) and the frame hooks */
extern void *(*sound_service_alloc)(unsigned int, unsigned int);
extern void (*sound_service_free)(void *);
extern void (*sound_service_wait_task)(void);
extern void (*sound_services)(OSTask *);
extern void (*sound_service_start_dma)(void *, unsigned short, void *);
extern void (*sound_service_add_callback)(void (*)(void));
extern void (*sound_service_remove_callback)(void (*)(void));
extern void *(*sound_service_translate_address)(void *);
extern void (*sound_dma_flush_hook)(void);
extern void (*sound_frame_hook)(void);

/* The library's functions, this unit's included */
extern void sound_hw_run_output_thread(void *);
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_hw_wait_task(void);
extern void sound_hw_remove_delay(void);
extern void sound_hw_init_codebook_cache(int);
extern void sound_hw_init_sample_cache(unsigned short);
extern void sound_hw_submit_task(void *, unsigned short);
extern double sound_truncate(double);
extern AudioCacheNode *sound_hw_take_sample_block(unsigned int sample_base);
extern void sound_hw_advance_envelope(SoundHwVoice *audio, unsigned short samples, unsigned char *active);
extern void sound_hw_advance_voice_position(SoundHwVoice *audio, unsigned short samples);
extern unsigned char *sound_hw_fetch_codebook(unsigned int source);
extern void sound_hw_fetch_sample_range(SfdSampleRange *fetch, unsigned int addr, unsigned int length, unsigned int sample_base);
extern void sound_hw_build_voice_command(SoundHwVoice *, unsigned short, unsigned char);
extern void sound_seq_tick(void);
extern void sound_synth_tick(void);
extern void sound_hw_build_subframe(void *, unsigned short);
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
extern void sound_hw_service_frame(void);
extern void sound_hw_alloc_voices(unsigned short, unsigned int);
extern void sound_hw_alloc_dma(unsigned short, unsigned int, unsigned char);
extern void sound_hw_alloc_delay_line(unsigned int, unsigned short);
extern void sound_hw_install_callback(void);
extern void sound_hw_init_lock(void);
extern void sound_hw_start_output_thread(unsigned int *);
extern unsigned char sound_hw_cache_mode_from_flags(unsigned int);
extern void sound_hw_init_synth(unsigned int *, unsigned short, unsigned short, unsigned char);
extern void sound_hw_set_output_frequency(unsigned int *);
extern void sound_hw_remove_callback(void);
extern void sound_hw_free_delay_line(void);
extern void sound_hw_free_dma(void);
extern void sound_hw_free_codebook_cache(void);
extern void sound_hw_free_sample_cache(void);
extern void sound_hw_free_voices(void);
extern void sound_hw_stop_output_thread(void);
extern void sound_hw_reset_voice(unsigned short, unsigned short);
extern void sound_hw_activate_voice(unsigned short);
extern void sound_compute_gains(unsigned short *, unsigned short *, unsigned int, unsigned int, unsigned int, unsigned short *, unsigned int, unsigned short *);
extern void sound_hw_set_delay(SoundDelayConfig *config);

/* The OS (port/ultra on the PC) and the PC layer */
extern void bzero(void *, int);
extern void osWritebackDCache(void *, int);
extern void osInvalDCache(void *, int);
extern unsigned int dcache_create_dirty_lines(unsigned int, unsigned int);
extern int osAiSetNextBuffer(void *, unsigned int);
extern int osAiSetFrequency(unsigned int);
extern int osRecvMesg(OSMesgQueue *, void **, int);
extern int osSendMesg(OSMesgQueue *, OSMesg, int);
extern void osCreateMesgQueue(OSMesgQueue *, void **, int);
extern void osCreateThread(OSThread *, int, void (*)(void *), void *, void *, int);
extern void osStartThread(OSThread *);
extern void osYieldThread(void);
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int);
extern void pc_audio_set_sfd_voices(unsigned n);
extern __SIZE_TYPE__ pc_audio_sfd_size(unsigned n, int form);
extern void pc_log(const char *fmt, ...);

/* PC: an SFD's size and the mix state block's grow with the voices past 32 ([audio] voices, docs/port/AUDIO.md 10):
 * an SFD holds a voice record a voice slot, then the state block their last outputs (MX_STATE_SIZE_N), and
 * pc_sfd_layout gives the interpreter the slots as the voices are set up; 32 records and 664 bytes otherwise, the
 * N64's. */
static unsigned int pc_sfd_size(void);   /* sizeof(SfdBlock) with 32 voices: 2576 bytes on the N64 */
#define SFD_SIZE pc_sfd_size()
static void pc_sfd_layout(unsigned short count);
static unsigned int pc_sfd_state_size(void);
#define PC_MX_STATE(n) pc_sfd_state_size()
#define PC_MX_CAP(n) PC_MX_VOICE_SLOTS   /* the most voices the PC gives (pc_game_types.h, docs/port/AUDIO.md 10) */

/* The AI output thread (on the PC, port/ultra's AI hands the buffers to port/audio). Allocates the ring of 768-byte output
 * buffers (16, or 24 at half rate; half as many at 22,050 Hz or below), queues the first to the AI, then the next
 * one on each "buffer taken" event, until sound_hw_stop_output_thread sends the stop command. sound_output_index is
 * the buffer the AI plays: sound_hw_service_frame renders the buffers up to it. argument: unused. */
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
    sound_output_buffers[0] = sound_service_alloc(sound_output_buffer_count * OUTPUT_BUFFER_BYTES, 128);
    for (i = 1; i < sound_output_buffer_count; i++) {
        sound_output_buffers[i] = (unsigned char *)sound_output_buffers[i - 1] + OUTPUT_BUFFER_BYTES;
    }
    bzero(sound_output_buffers[0], sound_output_buffer_count * OUTPUT_BUFFER_BYTES);
    osWritebackDCache(sound_output_buffers[0], sound_output_buffer_count * OUTPUT_BUFFER_BYTES);
    sound_output_index = 0;
    osAiSetNextBuffer(sound_output_buffers[0], OUTPUT_BUFFER_BYTES);
    stop = 0;
    do {
        osRecvMesg((OSMesgQueue *)sound_output_queue, &message, OS_MESG_BLOCK);
        if (sound_output_frozen == 0) {
            switch (*(unsigned char *) message) {
            case OUTPUT_MSG_BUFFER_DONE:
                sound_output_index = (sound_output_index + 1) % sound_output_buffer_count;
                osAiSetNextBuffer(sound_output_buffers[sound_output_index], OUTPUT_BUFFER_BYTES);
                break;
            case OUTPUT_MSG_STOP:
                stop = 1;
                break;
            }
        }
    } while (stop == 0);
}

/* Starts the output: sets the AI to the asked rate (*frequency becomes the rate it gives), has the AI's event post
 * "buffer taken" to the output thread's queue, and starts the thread. */
void sound_hw_start_output_thread(unsigned int *frequency)
{
    sound_output_event = OUTPUT_MSG_BUFFER_DONE;
    osCreateMesgQueue((OSMesgQueue *) sound_output_queue, sound_output_messages, 4);
    osSetEventMesg(OS_EVENT_AI, (OSMesgQueue *) sound_output_queue, &sound_output_event);
    sound_output_frequency = osAiSetFrequency(*frequency);
    *frequency = sound_output_frequency;
    sound_output_stack = sound_service_alloc(OUTPUT_THREAD_STACK_BYTES, 128);
    osCreateThread((OSThread *) sound_output_thread, 0, sound_hw_run_output_thread, 0,
                   sound_output_stack + OUTPUT_THREAD_STACK_BYTES, OUTPUT_THREAD_PRIORITY);
    osStartThread((OSThread *) sound_output_thread);
}

/* Sets the AI's rate again without starting an output thread (sound_hw_init_shared_output, a restart over the
 * running one): *frequency becomes the rate the AI gives. */
void sound_hw_set_output_frequency(unsigned int *frequency)
{
    sound_output_frequency = osAiSetFrequency(*frequency);
    *frequency = sound_output_frequency;
}

/* Stops the output thread and frees its buffers and stack. */
void sound_hw_stop_output_thread(void)
{
    unsigned char command[1];
    command[0] = OUTPUT_MSG_STOP;
    osSendMesg((OSMesgQueue *)sound_output_queue, command, OS_MESG_BLOCK);
    sound_service_free(sound_output_buffers[0]);
    sound_service_free(sound_output_stack);
}

/* Allocates the delay effect's line: duration_ms of samples at the output rate, rounded up to whole subframes (and
 * a whole subframe more when already whole). No line when duration_ms is 0, as in Rush 2049. */
void sound_hw_alloc_delay_line(unsigned int frequency, unsigned short duration_ms)
{
    unsigned int count;
    if (duration_ms != 0) {
        count = duration_ms * frequency / 1000;
        count = SUBFRAME_FRAMES - count % SUBFRAME_FRAMES + count;
        sound_delay_length = count;
        sound_delay = 0;
        sound_delay_line = sound_service_alloc(count * 2, 0);
    } else {
        sound_delay_line = 0;
    }
}

/* Turns the delay (echo) effect on: builds the microcode's parameter block from config (the line's length, up to
 * eight taps' delays and gains, the feedback and, in mode 1, its 4-tap filter, else a filter that passes the signal
 * through), clears the line and hands the block to the next SFDs. Needs the line sound_hw_alloc_delay_line made;
 * nothing in Rush 2049 calls it. */
void sound_hw_set_delay(SoundDelayConfig *config)
{
    unsigned int count;
    SoundDelayState *state;
    unsigned int i;
    if (sound_delay_line != 0) {
        count = config->duration * sound_output_frequency / 1000;
        count = SUBFRAME_FRAMES - count % SUBFRAME_FRAMES + count;
        state = sound_service_alloc(DELAY_STATE_SIZE, 0);
        state->buffer = sound_delay_line;
        i = count * 2;
        bzero(state->buffer, i);
        osWritebackDCache(state->buffer, i);
        state->length = count;
        state->feedback = config->feedback << 8;
        state->tap_count = config->tap_count;
        for (i = 0; i < config->tap_count; i++) {
            state->tap_delays[i] = (config->tap_delays[i] * sound_output_frequency / 1000 + 3) & 0xFFFC;
            state->tap_gains[i] = config->tap_gains[i] << 8;
        }
        if (config->mode == 1) {
            for (i = 0; i < 4; i++) {
                state->filter[i] = config->filter[i];
            }
        } else {
            state->filter[0] = 0;
            state->filter[1] = 0;
            state->filter[2] = 0;
            state->filter[3] = 32767;
        }
        osWritebackDCache(state, DELAY_STATE_N64_BYTES);
        sound_hw_lock();
        sound_delay_position = 0;
        sound_delay = state;
        sound_hw_unlock();
    }
}

/* Turns the delay effect off, once the task that may still read its parameters is done. */
void sound_hw_remove_delay(void)
{
    if (sound_delay != 0) {
        sound_hw_wait_task();
        sound_service_free(sound_delay);
        sound_delay = 0;
    }
}

/* Turns the delay effect off and frees its line. */
void sound_hw_free_delay_line(void)
{
    sound_hw_remove_delay();
    if (sound_delay_line != 0) {
        sound_service_free(sound_delay_line);
    }
}

/* Allocates the hardware voices (all off, no SFD slot yet), the two SFD sets (each as many SFDs as a task may
 * hold: a 50th, or at half rate a 25th, of a second of subframes, times the margin, plus one) and the microcode's mix
 * state block. */
void sound_hw_alloc_voices(unsigned short voice_count, unsigned int frequency)
{
    unsigned short i;
    sound_hw_voice_count = voice_count; pc_sfd_layout(voice_count);
    sound_hw_voice_block = sound_service_alloc(sound_hw_voice_count * sizeof(SoundHwVoice), 0);
    sound_hw_voices = sound_hw_voice_block;
    sound_sfd_set = 0;
    for (i = 0; i < sound_hw_voice_count; i++) {
        ((SoundHwVoice *)sound_hw_voice_block)[i].active = VOICE_OFF;
        ((SoundHwVoice *)sound_hw_voice_block)[i].release_pending = 0;
        ((SoundHwVoice *)sound_hw_voice_block)[i].sfd_slot = 255;
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

/* Frees what sound_hw_alloc_voices allocated. */
void sound_hw_free_voices(void)
{
    sound_service_free(sound_sfd_sets[0]);
    sound_service_free(sound_sfd_sets[1]);
    sound_service_free(sound_hw_voice_block);
    sound_service_free(sound_mix_state);
}

/* Sizes and allocates the DMA side: the DMA list (two entries per voice per subframe a task may hold), its done
 * queue, the codebook cache (an entry per list slot) and the sample cache (sound_sample_blocks_per_voice[cache_mode]
 * blocks per voice). sound_sample_cache_size is computed but nothing here uses it. */
void sound_hw_alloc_dma(unsigned short voice_count, unsigned int frequency, unsigned char cache_mode)
{
    if (sound_half_rate == 0) {
        sound_dma_capacity = voice_count * ((unsigned short) (((float)frequency / 50.0f * sound_dma_margin) / 192.0f) + 1);
    } else {
        sound_dma_capacity = voice_count * ((unsigned short) (((float)frequency / 25.0f * sound_dma_margin_half_rate) / 192.0f) + 1);
    }
    /* 784 frames, kept to at least a subframe's 192 (a test that never fires: the size was a setting) */
    sound_sample_cache_size = 784;
    if (sound_sample_cache_size < 192) {
        sound_sample_cache_size = 192;
    }
    sound_sample_cache_size = (unsigned int) ((float)(sound_dma_capacity * sound_sample_cache_size) * sound_sample_cache_factor);
    sound_sample_cache_size = ((((sound_sample_cache_size + 63) >> 6) * ADPCM_BLOCK_BYTES) + 15) & 0xFFF0;
    sound_dma_list = sound_service_alloc(sound_dma_capacity * 2 * DMA_ENTRY_SIZE, 0);
    osCreateMesgQueue((OSMesgQueue *)sound_dma_queue, &sound_dma_message, 1);
    sound_dma_busy = 0;
    sound_hw_init_codebook_cache(sound_dma_capacity);
    sound_hw_init_sample_cache((unsigned short)(sound_sample_blocks_per_voice[cache_mode] * voice_count));
}

/* Waits for a DMA list in flight, then frees the list. */
void sound_hw_free_dma(void)
{
    while (sound_dma_busy != 0) { pc_os_spin_yield("sound_hw_free_dma");
    }
    sound_service_free(sound_dma_list);
}

/* Empties the DMA list for a new frame (and resets its cache-line bookkeeping). */
void sound_hw_reset_dma_list(void)
{
    unsigned int value;
    value = (unsigned int)sound_dma_list;
    sound_dma_list_cache_cursor = value & ~15U;
    sound_dma_list_end = value;
    sound_dma_list_count = 0;
}

/* Waits for the audio task last submitted, if one is running (through the game's service). */
void sound_hw_wait_task(void)
{
    if (sound_task_busy != 0) {
        sound_service_wait_task();
        osYieldThread();
        sound_task_busy = 0;
    }
}

/* Hands sfd_count SFDs to the RSP as one audio task running the MusyX microcode, through the game's service
 * (musyx_submit_audio_task); nothing when there are none. On the PC port/audio interprets them. */
void sound_hw_submit_task(void *sfds, unsigned short sfd_count)
{
    OSTask *task = &sound_task;
    if (sfd_count != 0) {
        task->t.type = M_AUDTASK;
        task->t.flags = 0;
        task->t.ucode_boot = (u64 *) rspbootTextStart;
        task->t.ucode_boot_size = (unsigned int) rspbootTextEnd - (unsigned int) rspbootTextStart;
        task->t.ucode = (u64 *) audio_ucode_text;
        task->t.ucode_size = AUDIO_UCODE_TEXT_BYTES;
        task->t.ucode_data = (u64 *) audio_ucode_data;
        task->t.ucode_data_size = SP_UCODE_DATA_SIZE;
        task->t.data_ptr = sfds;
        task->t.data_size = sfd_count;
        osWritebackDCache(sfds, sfd_count * SFD_N64_BYTES);
        sound_services(task);
        sound_task_busy = 1;
    }
}

/* Submits the current SFD set (sound_hw_service_frame points it at last frame's set first). */
void sound_hw_submit_sfds(void)
{
    sound_hw_submit_task(sound_sfd_current, *sound_sfd_count);
}

/* Starts a voice's envelope: the attack, from 0 to full level over attack_ms. */
void sound_hw_start_envelope(SoundHwVoice *audio)
{
    audio->envelope_stage = ENVELOPE_ATTACK;
    audio->stage_time = 0;
    audio->stage_start = 0.0f;
    audio->envelope_level = 0.0f;
    audio->stage_ms = audio->attack_ms;
    audio->stage_target = 1.0f;
}

/* Starts a voice's release: from its level now to 0 over release_ms. */
void sound_hw_release_envelope(SoundHwVoice *audio)
{
    audio->envelope_stage = ENVELOPE_RELEASE;
    audio->stage_time = 0;
    audio->stage_start = audio->envelope_level;
    audio->stage_target = 0.0f;
    audio->stage_ms = audio->release_ms;
}

/* Advances a voice's envelope by one subframe of samples and sets envelope_level, the factor on its gains. A voice
 * let go of goes to its release; at the end of a stage the attack goes on to the decay (from 1.0 to the sustain
 * level), the decay to the sustain (which holds), and the release ends the voice (*active = VOICE_OFF). In a stage
 * the level moves linearly from stage_start to stage_target over stage_ms. */
void sound_hw_advance_envelope(SoundHwVoice *audio, unsigned short samples, unsigned char *active)
{
    unsigned int increment;
    float ratio;
    /* the subframe in milliseconds, 16.16: samples at the output rate as 32 kHz samples, 32 to the ms */
    increment = ((samples * 32000U) / sound_output_frequency) << 11;
    if (audio->key_held == 0 && audio->envelope_stage != ENVELOPE_RELEASE) {
        audio->envelope_stage = ENVELOPE_RELEASE;
        audio->stage_time = 0;
        audio->stage_start = audio->envelope_level;
        audio->stage_target = 0.0f;
        audio->stage_ms = audio->release_ms;
    } else if ((audio->stage_time >> 16) >= audio->stage_ms) {
        switch (audio->envelope_stage) {
        case ENVELOPE_ATTACK:
            audio->envelope_stage = ENVELOPE_DECAY;
            audio->stage_time = 0;
            audio->stage_start = 1.0f;
            audio->stage_target = audio->sustain_level;
            audio->stage_ms = audio->decay_ms;
            break;
        case ENVELOPE_DECAY:
            audio->envelope_stage = ENVELOPE_SUSTAIN;
            audio->stage_start = audio->sustain_level;
            audio->stage_target = audio->sustain_level;
            audio->envelope_level = audio->sustain_level;
            return;
        case ENVELOPE_RELEASE:
            *active = VOICE_OFF;
            audio->envelope_level = 0.0f;
            return;
        }
    }
    if (audio->stage_ms != 0) {
        ratio = audio->stage_time / (audio->stage_ms * 65536.0f);
    } else {
        ratio = 1.0f;
    }
    audio->envelope_level = (audio->stage_target - audio->stage_start) * ratio + audio->stage_start;
    audio->stage_time += increment;
}

/* Resets voice index for a new sample: off, held, at its own pitch, silent (gains and targets 0), with the given
 * flags. */
void sound_hw_reset_voice(unsigned short index, unsigned short flags)
{
    SoundHwVoice *audio;
    audio = &sound_hw_voices[index];
    audio->active = VOICE_OFF;
    audio->flags = flags;
    audio->rate = PITCH_UNITY;
    audio->key_held = 1;
    audio->level[0] = 0;
    audio->level[1] = 0;
    audio->level[2] = 0;
    audio->level[3] = 0;
    audio->target[0] = 0;
    audio->target[1] = 0;
    audio->target[2] = 0;
    audio->target[3] = 0;
}

/* Starts voice index playing: its envelope from the attack; its first subframe takes its gains without a ramp. */
void sound_hw_activate_voice(unsigned short index)
{
    SoundHwVoice *audio = &sound_hw_voices[index];
    sound_hw_start_envelope(audio);
    audio->active = VOICE_STARTING;
}

/* Waits for the DMA list last started, if any. */
void sound_hw_wait_dma(void)
{
    if (sound_dma_busy != 0) {
        osRecvMesg((OSMesgQueue *)sound_dma_queue, 0, OS_MESG_BLOCK);
        sound_dma_busy = 0;
        osYieldThread();
    }
}

/* Starts this frame's DMA list, if it has entries, through the game's service (musyx_start_dma_list); its done
 * message comes to sound_dma_queue. */
void sound_hw_start_dma(void)
{
    if (sound_dma_list_count != 0) {
        sound_dma_busy = 1;
        sound_service_start_dma(sound_dma_list, sound_dma_list_count, sound_dma_queue);
    }
}

/* Moves a voice's play position on by samples output frames at its rate. While it loops (a looping sample, held
 * or without a tail, not yet past the loop's end) the position wraps back into the loop; else it stops at the
 * sample's last sample. */
void sound_hw_advance_voice_position(SoundHwVoice *audio, unsigned short samples)
{
    double increment;
    double loop_end;
    increment = (audio->rate * samples) * 0.000244140625;   /* rate is 4.12 fixed point: / 4096 */
    if (audio->loop_length != 0 && (audio->key_held != 0 || audio->tail == 0)
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

/* Allocates the codebook cache: entry_count 256-byte buffers, all on the free list. On the PC the buffer is marked
 * as holding big-endian data (the DMA copies cartridge bytes into it as they are). */
void sound_hw_init_codebook_cache(int entry_count)
{
    int i;
    sound_codebook_nodes = sound_service_alloc(entry_count * sizeof(AudioNode), 0);
    sound_codebook_cache = sound_service_alloc(entry_count * ADPCM_CODEBOOK_BYTES, 0);
    osInvalDCache(sound_codebook_cache, entry_count * ADPCM_CODEBOOK_BYTES); pc_mem_register_be(sound_codebook_cache, entry_count * ADPCM_CODEBOOK_BYTES);
    sound_codebook_used = 0;
    sound_codebook_free = sound_codebook_nodes;
    sound_codebook_nodes[0].buffer = sound_codebook_cache;
    sound_codebook_nodes[0].previous = 0;
    for (i = 1; i < entry_count; i++) {
        sound_codebook_nodes[i].previous = &sound_codebook_nodes[i - 1];
        sound_codebook_nodes[i - 1].next = &sound_codebook_nodes[i];
        sound_codebook_nodes[i].buffer = &sound_codebook_cache[i * ADPCM_CODEBOOK_BYTES];
    }
    sound_codebook_nodes[i - 1].next = 0;
    sound_codebook_last = 0;
}

/* Frees the codebook cache. */
void sound_hw_free_codebook_cache(void)
{
    pc_mem_unregister_be(sound_codebook_cache); sound_service_free(sound_codebook_cache);
    sound_service_free(sound_codebook_nodes);
}

/* Once a frame: counts down every cached codebook's frames left; one at 0 may be reused. */
void sound_hw_age_codebook_cache(void)
{
    AudioNode *node;
    for (node = sound_codebook_used; node != 0; node = node->next) {
        if (node->countdown != 0) {
            --node->countdown;
        }
    }
}

/* Returns the buffer that holds (or, once this frame's DMA list is done, will hold) the ADPCM codebook at cartridge
 * address source: a cached one, else a free entry, else the first used one no voice has needed for two frames, with
 * a DMA entry added to fetch it. */
unsigned char *sound_hw_fetch_codebook(unsigned int source)
{
    AudioNode *node;
    for (node = sound_codebook_used; node != 0; node = node->next) {
        if (source == node->source) {
            node->countdown = CODEBOOK_KEEP_FRAMES;
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
    node->countdown = CODEBOOK_KEEP_FRAMES;
    node->source = source;
    sound_dma_list_end += DMA_ENTRY_N64_BYTES;
    sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
    ((SoundDmaEntryCodebook *)sound_dma_list)[sound_dma_list_count].source = source;
    ((SoundDmaEntryCodebook *)sound_dma_list)[sound_dma_list_count].length = ADPCM_CODEBOOK_BYTES;
    ((SoundDmaEntryCodebook *)sound_dma_list)[sound_dma_list_count].destination = node->buffer;
    sound_dma_list_count++;
    return node->buffer;
}

/* Allocates the sample cache: block_count blocks of 0x600 bytes, all on the free list and none on the LRU list.
 * On the PC the buffer is marked as holding big-endian data. */
void sound_hw_init_sample_cache(unsigned short block_count)
{
    int i;
    sound_sample_cache = sound_service_alloc(block_count * SAMPLE_BLOCK_BYTES, 0);
    osInvalDCache(sound_sample_cache, block_count * SAMPLE_BLOCK_BYTES); pc_mem_register_be(sound_sample_cache, block_count * SAMPLE_BLOCK_BYTES);
    sound_sample_nodes = sound_service_alloc(block_count * sizeof(AudioCacheNode), 0);
    sound_sample_lru_head = 0;
    sound_sample_lru_tail = 0;
    sound_sample_free = sound_sample_nodes;
    sound_sample_nodes[0].previous = 0;
    sound_sample_nodes[0].buffer = sound_sample_cache;
    for (i = 1; i < block_count; i++) {
        sound_sample_nodes[i - 1].next = &sound_sample_nodes[i];
        sound_sample_nodes[i].previous = &sound_sample_nodes[i - 1];
        sound_sample_nodes[i].buffer = &sound_sample_cache[i * SAMPLE_BLOCK_BYTES];
    }
    sound_sample_nodes[i - 1].next = 0;
}

/* Frees the sample cache. */
void sound_hw_free_sample_cache(void)
{
    sound_service_free(sound_sample_nodes);
    pc_mem_unregister_be(sound_sample_cache); sound_service_free(sound_sample_cache);
}

/* Takes a sample cache block for new data and puts it at the LRU list's tail (the most recent end): a free one,
 * else the least recently used one (the list's head). sample_base: unused (the caller sets the block's owner). */
AudioCacheNode *sound_hw_take_sample_block(unsigned int sample_base)
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

/* Finds the cartridge bytes [addr, addr + length) of the sample starting at sample_base in the sample cache, and
 * says where they will be in fetch: one range, or two (in address order) when they span two blocks. Blocks of the
 * sample that overlap the range are used (topping up a partly loaded one, and moved to the LRU tail); the rest is
 * fetched into new blocks, aligned to 0x600 from the sample's start, by entries added to this frame's DMA list
 * (partial loads round up to 256 bytes). */
void sound_hw_fetch_sample_range(SfdSampleRange *fetch, unsigned int addr, unsigned int length, unsigned int sample_base)
{
    unsigned int size;
    AudioCacheNode *previous;
    AudioCacheNode *node;
    unsigned char *pointers[2];
    unsigned int lengths[2];
    unsigned int starts[2];
    unsigned short range_count;
    unsigned int offset;
    unsigned int end;
    unsigned int low;

    end = addr + length;
    low = addr - SAMPLE_BLOCK_BYTES;
    range_count = 0;
    for (node = sound_sample_lru_tail; node != NULL; node = previous) {
        previous = node->previous;
        if (sample_base != node->sample_base || node->address >= end || low >= node->address) {
            continue;
        }
        if (node->loaded != SAMPLE_BLOCK_BYTES && node->address + node->loaded < addr + length) {
            if (addr + length >= node->address + SAMPLE_BLOCK_BYTES) {
                size = SAMPLE_BLOCK_BYTES - node->loaded;
            } else {
                size = (addr + length - node->address - node->loaded + 0xFF) & ~0xFF;
            }
            sound_dma_list_end += DMA_ENTRY_N64_BYTES;
            sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
            ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].source = node->address + node->loaded;
            ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].length = size;
            ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].destination = node->buffer + node->loaded;
            sound_dma_list_count++;
            node->loaded += size;
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
        if (addr < node->address) {
            pointers[range_count] = node->buffer;
            starts[range_count] = node->address;
            lengths[range_count] = length - node->address + addr;
            length -= lengths[range_count];
            range_count++;
        } else if (node->address + node->loaded < addr + length) {
            pointers[range_count] = node->buffer + addr - node->address;
            starts[range_count] = addr;
            lengths[range_count] = node->address + node->loaded - addr;
            length -= lengths[range_count];
            range_count++;
            addr = node->address + node->loaded;
        } else {
            pointers[range_count] = node->buffer + addr - node->address;
            starts[range_count] = addr;
            lengths[range_count] = length;
            length = 0;
            range_count++;
            break;
        }
    }
    if (length != 0) {
        offset = (addr - sample_base) % SAMPLE_BLOCK_BYTES;
        do {
            node = sound_hw_take_sample_block(sample_base);
            node->sample_base = sample_base;
            node->address = addr - offset;
            if (length + offset > SAMPLE_BLOCK_BYTES) {
                node->loaded = SAMPLE_BLOCK_BYTES;
                sound_dma_list_end += DMA_ENTRY_N64_BYTES;
                sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
                addr = addr - offset + SAMPLE_BLOCK_BYTES;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].source = node->address;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].length = SAMPLE_BLOCK_BYTES;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].destination = node->buffer;
                sound_dma_list_count++;
                pointers[range_count] = node->buffer + offset;
                starts[range_count] = node->address + offset;
                lengths[range_count] = SAMPLE_BLOCK_BYTES - offset;
                length -= SAMPLE_BLOCK_BYTES - offset;
                range_count++;
            } else {
                node->loaded = (length + offset + 0xFF) & ~0xFF;
                sound_dma_list_end += DMA_ENTRY_N64_BYTES;
                sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].source = node->address;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].length = node->loaded;
                ((SoundDmaEntrySamples *)sound_dma_list)[sound_dma_list_count].destination = node->buffer;
                sound_dma_list_count++;
                pointers[range_count] = node->buffer + offset;
                starts[range_count] = node->address + offset;
                lengths[range_count] = length;
                length = 0;
                range_count++;
            }
            offset = 0;
        } while (length != 0);
    }
    if (range_count == 1) {
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

static unsigned pc_sfd_slots = 32;
/* PC: the SFD's release-mask words past the header's, for slots 32 and up. */
static unsigned int pc_sfd_words(void) { return pc_sfd_slots > 32 ? (pc_sfd_slots - 32 + 31) / 32 : 0; }
/* PC: an SFD's size with pc_sfd_slots voice records and its mask words, rounded to 8 bytes. */
static unsigned int pc_sfd_size(void)
{
    return (unsigned int)((__builtin_offsetof(SfdBlock, commands) + pc_sfd_slots * sizeof(SfdVoice) +
                           4 * pc_sfd_words() + 7) & ~7u);
}

/* PC: the mix state block's size: the N64's 664 bytes, and 8 more per slot past 32. */
static unsigned int pc_sfd_state_size(void) { return 664 + (pc_sfd_slots - 32) * 8; }
/* PC: sets the SFD's slot count for voice_count voices (at least the N64's 32) and tells port/audio, logging a
 * disagreement about the SFD's size. */
static void pc_sfd_layout(unsigned short voice_count)
{
    pc_sfd_slots = voice_count < 32 ? 32 : voice_count;
    pc_audio_set_sfd_voices(pc_sfd_slots);
    if (pc_audio_sfd_size(pc_sfd_slots, 0) != pc_sfd_size())
        pc_log("audio: the game's SFD of %u voices is %u bytes, the interpreter's %u", pc_sfd_slots, pc_sfd_size(),
               (unsigned)pc_audio_sfd_size(pc_sfd_slots, 0));
}

/* Writes voice audio's record into the SFD being built, as SFD slot `slot`: which input the subframe reads (its
 * main and loop segments, fetched through the caches for a cartridge sample), the end and restart points, the
 * position and rate, and the gains (target gains times the envelope, ramped across the subframe), then advances the
 * voice's envelope and position by one subframe. The voice turns off when its sample ends in this window or its
 * release ends. frame_offset (where the subframe falls in this frame's run): unused. */
void sound_hw_build_voice_command(SoundHwVoice *audio, unsigned short frame_offset, unsigned char slot)
{
    unsigned int frames;
    unsigned int end;
    unsigned int position;
    unsigned char state;
    unsigned int excess;
    unsigned int address[2];
    unsigned int length[2];
    int offset;
    unsigned int restart_offset;
    unsigned int padded[2];
    SfdVoice *cmd;
    unsigned short mainRem;
    unsigned short loopRem;
    unsigned short probe;

    cmd = &PC_SFD(SfdBlock, sound_sfd_current, *sound_sfd_count).commands[PC_SFD(SfdBlock, sound_sfd_current, *sound_sfd_count).count];
    sound_sfd_write_end += SFD_VOICE_N64_BYTES;
    sound_sfd_cache_cursor = dcache_create_dirty_lines(sound_sfd_cache_cursor, sound_sfd_write_end);
    /* the input samples one subframe reads at this rate, and 6 more for the resampler's history */
    frames = (unsigned int)(audio->rate * SUBFRAME_FRAMES + 0xFFF) >> 12;
    frames += 6;
    position = sound_truncate(audio->sample_position);
    end = audio->loop_start + audio->loop_length;
    state = VOICE_PLAYING;
    /* The window: the input this subframe reads, as a main segment and, when it runs past the loop's end, a loop
     * segment from the loop's start; with where the microcode stops (end) or jumps back (restart). */
    if (audio->loop_length != 0 && (audio->key_held != 0 || audio->tail == 0) && end >= position) {
        /* looping: the window is before the loop's end, reaches it from before the loop, is the whole loop
         * (short loops), or crosses the loop's end into a loop segment */
        if (end >= position + frames) {
            offset = 0;
            address[0] = position;
            length[0] = frames - 6;
            length[1] = 0;
            restart_offset = WINDOW_NO_END;
        } else if (audio->loop_start >= position) {
            address[0] = position;
            length[0] = audio->loop_start - position + audio->loop_length;
            length[1] = 0;
            offset = 0;
            restart_offset = audio->loop_start - position;
        } else if (audio->loop_length < (excess = frames - end + position) || excess >= position - audio->loop_start) {
            offset = position - audio->loop_start;
            address[0] = audio->loop_start;
            length[0] = audio->loop_length;
            length[1] = 0;
            restart_offset = 0;
        } else {
            offset = 0;
            address[0] = position;
            length[0] = end - position;
            address[1] = audio->loop_start;
            length[1] = excess - 6;
            restart_offset = 0;
        }
    } else {
        /* playing through: the sample may end in the window (the voice is then done) */
        if (position + frames >= audio->length) {
            address[0] = position;
            offset = 0;
            length[0] = audio->length - position + 0x20;
            length[1] = 0;
            restart_offset = WINDOW_SAMPLE_ENDS;
            state = VOICE_OFF;
        } else {
            address[0] = position;
            offset = 0;
            length[0] = frames - 6;
            length[1] = 0;
            restart_offset = WINDOW_NO_END;
        }
    }
    padded[0] = length[0] + 6;
    if (length[1] != 0) {
        padded[1] = length[1] + 6;
    }
    /* The segments' data: a sample in the cartridge through the caches (ADPCM: whole 40-byte blocks, its codebook
     * too), else straight from RAM. ADPCM positions count 64-sample blocks (>> 6) of two 32-sample frames (>> 5);
     * PCM16 positions are rounded down to 4 samples. */
    if (audio->flags & VOICE_FLAG_CARTRIDGE) {
        switch (audio->format) {
        case SAMPLE_FORMAT_ADPCM:
            if (length[1] != 0) {
                loopRem = address[1] & 0x3F;
                sound_hw_fetch_sample_range(&cmd->loop, (address[1] >> 6) * ADPCM_BLOCK_BYTES + (unsigned int)audio->data + ADPCM_CODEBOOK_BYTES, ((((padded[1] + (loopRem & 0x1F) + 0x1F) >> 5) + (loopRem >> 5) + 1) >> 1) * ADPCM_BLOCK_BYTES, (unsigned int)audio->data + ADPCM_CODEBOOK_BYTES);
                cmd->loop_offset = loopRem;
                loopRem &= 0x1F;
                cmd->loop_blocks = (padded[1] + loopRem + 0x1F) >> 5;
            } else {
                cmd->loop.first = 0;
                cmd->loop_blocks = 0;
            }
            mainRem = address[0] & 0x3F;
            sound_hw_fetch_sample_range(&cmd->main, (address[0] >> 6) * ADPCM_BLOCK_BYTES + (unsigned int)audio->data + ADPCM_CODEBOOK_BYTES, ((((padded[0] + (mainRem & 0x1F) + 0x1F) >> 5) + (mainRem >> 5) + 1) >> 1) * ADPCM_BLOCK_BYTES, (unsigned int)audio->data + ADPCM_CODEBOOK_BYTES);
            cmd->u.book = sound_hw_fetch_codebook((unsigned int)audio->data);
            goto adpcm_tail;
        case SAMPLE_FORMAT_PCM16:
            if (length[1] != 0) {
                loopRem = address[1] & 3;
                sound_hw_fetch_sample_range(&cmd->loop, (unsigned int)((address[1] & 0xFFFC) * 2 + audio->data), ((padded[1] + loopRem + 3) & 0xFFFC) * 2, (unsigned int)audio->data);
                cmd->u.pcm.loop_count = padded[1];
                cmd->loop_offset = loopRem;
            } else {
                cmd->loop.first = 0;
                cmd->u.pcm.loop_count = 0;
            }
            mainRem = address[0] & 3;
            sound_hw_fetch_sample_range(&cmd->main, (unsigned int)((address[0] & 0xFFFC) * 2 + audio->data), ((padded[0] + mainRem + 3) & 0xFFFC) * 2, (unsigned int)audio->data);
            goto pcm_tail;
        }
    } else {
        switch (audio->format) {
        case SAMPLE_FORMAT_ADPCM:
            if (length[1] != 0) {
                loopRem = address[1] & 0x3F;
                cmd->loop.first = (address[1] >> 6) * ADPCM_BLOCK_BYTES + audio->data + ADPCM_CODEBOOK_BYTES;
                cmd->loop.first_length = ((((padded[1] + (loopRem & 0x1F) + 0x1F) >> 5) + loopRem / 32 + 1) >> 1) * ADPCM_BLOCK_BYTES;
                cmd->loop.second_length = 0;
                cmd->loop_offset = loopRem;
                loopRem &= 0x1F;
                cmd->loop_blocks = (padded[1] + loopRem + 0x1F) >> 5;
            } else {
                cmd->loop.first = 0;
                cmd->loop_blocks = 0;
            }
            mainRem = address[0] & 0x3F;
            cmd->main.first = (address[0] >> 6) * ADPCM_BLOCK_BYTES + audio->data + ADPCM_CODEBOOK_BYTES;
            cmd->main.first_length = ((((padded[0] + (mainRem & 0x1F) + 0x1F) >> 5) + mainRem / 32 + 1) >> 1) * ADPCM_BLOCK_BYTES;
            cmd->main.second_length = 0;
            cmd->u.book = audio->data;
        adpcm_tail:
            cmd->main_offset = mainRem;
            mainRem &= 0x1F;
            cmd->main_blocks = (padded[0] + mainRem + 0x1F) >> 5;
            break;
        case SAMPLE_FORMAT_PCM16:
            if (length[1] != 0) {
                loopRem = address[1] & 3 & 0xFFFF;
                cmd->loop.first = (address[1] & 0xFFFC) * 2 + audio->data;
                cmd->loop.first_length = ((padded[1] + loopRem + 3) & 0xFFFC) * 2;
                cmd->loop.second_length = 0;
                cmd->u.pcm.loop_count = padded[1];
                cmd->loop_offset = loopRem;
            } else {
                cmd->loop.first = 0;
                cmd->u.pcm.loop_count = 0;
            }
            mainRem = address[0] & 3;
            cmd->main.first = (address[0] & 0xFFFC) * 2 + audio->data;
            cmd->main.first_length = ((padded[0] + mainRem + 3) & 0xFFFC) * 2;
            cmd->main.second_length = 0;
        pcm_tail:
            cmd->u.pcm.main_count = padded[0];
            cmd->main_blocks = 0;
            cmd->main_offset = mainRem;
            break;
        }
    }
    /* the end and restart points, from the main segment's base (restart bit 15: in the loop segment) */
    cmd->start = offset;
    if (restart_offset == WINDOW_NO_END) {
        cmd->end = 0x4000;
    } else if (restart_offset == WINDOW_SAMPLE_ENDS) {
        cmd->end = length[0] + mainRem;
        cmd->restart = length[0] + mainRem - 0x20;
    } else {
        cmd->end = length[0] + mainRem;
        if (cmd->loop.first != 0) {
            cmd->restart = (loopRem + restart_offset) | 0x8000;
        } else {
            cmd->restart = mainRem + restart_offset;
        }
    }
    audio->sfd_slot = slot;
    /* The gains: the targets times the envelope, reached at once in a voice's first subframe, else by a ramp over
     * the subframe from where the last one ended. */
    sound_hw_advance_envelope(audio, SUBFRAME_FRAMES, &state);
    if (audio->active == VOICE_STARTING) {
        cmd->volume[0] = (audio->level[0] = audio->target[0] * audio->envelope_level) << 16;
        cmd->volume[1] = (audio->level[1] = audio->target[1] * audio->envelope_level) << 16;
        cmd->volume[2] = (audio->level[2] = audio->target[2] * audio->envelope_level) << 16;
        cmd->volume[3] = (audio->level[3] = audio->target[3] * audio->envelope_level) << 16;
        cmd->delta[0] = 0;
        cmd->delta[1] = 0;
        cmd->delta[2] = 0;
        cmd->delta[3] = 0;
    } else {
        cmd->volume[0] = audio->level[0] << 16;
        cmd->volume[1] = audio->level[1] << 16;
        cmd->volume[2] = audio->level[2] << 16;
        cmd->volume[3] = audio->level[3] << 16;
        cmd->delta[0] = (((audio->level[0] = audio->target[0] * audio->envelope_level) << 16) - cmd->volume[0]) / SUBFRAME_FRAMES;
        cmd->delta[1] = (((audio->level[1] = audio->target[1] * audio->envelope_level) << 16) - cmd->volume[1]) / SUBFRAME_FRAMES;
        cmd->delta[2] = (((audio->level[2] = audio->target[2] * audio->envelope_level) << 16) - cmd->volume[2]) / SUBFRAME_FRAMES;
        cmd->delta[3] = (((audio->level[3] = audio->target[3] * audio->envelope_level) << 16) - cmd->volume[3]) / SUBFRAME_FRAMES;
    }
    cmd->rate = audio->rate;
    cmd->fraction = (audio->sample_position - sound_truncate(audio->sample_position)) * 65536.0;
    cmd->unused4C = (unsigned short)(padded[0] * (audio->rate * 16)) + cmd->fraction;
    cmd->output = 0;
    PC_SFD(SfdBlock, sound_sfd_current, *sound_sfd_count).count++;
    sound_hw_advance_voice_position(audio, SUBFRAME_FRAMES);
    audio->active = state;
}

/* Starts building into SFD set sound_sfd_set: no SFDs yet, the first one with no voices. */
void sound_hw_begin_sfd_set(void)
{
    unsigned int cursor;
    sound_sfd_count = &sound_sfd_counts[sound_sfd_set];
    sound_sfd_current = sound_sfd_sets[sound_sfd_set];
    cursor = (unsigned int)sound_sfd_current + SFD_HEADER_N64_BYTES;
    sound_sfd_cache_cursor = cursor & ~15U;
    sound_sfd_write_end = cursor;
    *sound_sfd_count = 0;
    *sound_sfd_current = 0;
}

/* PC: a stopped voice's slot is its bit in the release mask: slots 0-31 in the header's word, as on the N64 (whose
 * shift takes the slot's low five bits, so a voice that never had a slot, 255, is bit 31), the others in the words
 * after the SFD's last voice record, which pc_sfd_mask_out writes with the header's (docs/port/AUDIO.md 10). */
static unsigned int pc_mask_ext[8];
/* PC: marks slot's bit in the release mask: the header's word (as on the N64) or the extra words past slot 31. */
static void pc_release(unsigned int *mask, unsigned char slot)
{
    if (slot < 32 || slot >= pc_sfd_slots)
        *mask |= 1U << (slot & 31);
    else
        pc_mask_ext[(slot - 32) / 32] |= 1U << (slot & 31);
}

/* PC: writes the extra release-mask words after the SFD's last voice record, and clears them. */
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
/* Builds one SFD, rendering into AI buffer output: the release mask of the voices stopped short since the last one,
 * a record per playing voice (in voice order: a voice's slot is its place among the playing ones), the output
 * pointer on the last record (on the first, with a 0 status, when no voice plays), the mix state and the delay
 * effect's parameters and line position. Then moves to the next SFD of the set if the task has room for it.
 * frame_offset: passed on, unused. */
void sound_hw_build_subframe(void *output, unsigned short frame_offset)
{
    unsigned short i;
    unsigned char slot;
    unsigned int released;
    slot = 0;
    released = 0;
    for (i = 0; i < sound_hw_voice_count; i++) {
        if (sound_hw_voices[i].release_pending != 0) {
            sound_hw_voices[i].release_pending = 0;
            PC_RELEASE(released, sound_hw_voices[i].sfd_slot);
        }
        if (sound_hw_voices[i].active != 0) {
            sound_hw_build_voice_command(&sound_hw_voices[i], frame_offset, slot);
            slot++;
        }
    }
    if (PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).count == 0) {
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands[0].status = 0;
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands[0].output = output;
    } else {
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).commands[PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).count - 1].output = output;
    }
    PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).work = sound_mix_state;
    PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).changed = released; pc_sfd_mask_out(&PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count));
    PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).loop = sound_delay;
    if (PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).loop != 0) {
        PC_SFD(AudioBlock, sound_sfd_current, *sound_sfd_count).sample = sound_delay_position / SUBFRAME_FRAMES;
        sound_delay_position += SUBFRAME_FRAMES;
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

/* At the start of a frame's rendering: notes each playing voice's position (the one before becomes what
 * sound_hw_get_voice_position reports). */
void sound_hw_latch_voice_positions(void)
{
    int i;
    for (i = 0; i < sound_hw_voice_count; i++) {
        if (sound_hw_voices[i].active != 0) {
            sound_hw_voices[i].position = sound_hw_voices[i].latched_position;
            sound_hw_voices[i].latched_position = (unsigned int) sound_hw_voices[i].sample_position;
        }
    }
}

/* Renders AI buffer buffer_index: first the library's tick (the sequencer, then the synth) when one is due (60 a
 * second, counted in subframes), then the buffer's SFD. Returns 1. */
int sound_hw_render_subframe(unsigned short buffer_index, unsigned short frame_offset)
{
    if (sound_initialized != 0) {
        if (--sound_tick_countdown == 0) {
            sound_tick_countdown = sound_subframes_per_tick;
            sound_seq_tick();
            sound_synth_tick();
        }
    }
    sound_hw_build_subframe(sound_output_buffers[buffer_index], frame_offset);
    return 1;
}

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
/* The library's retrace callback (sound_hw_install_callback), the hardware layer's heartbeat. Under the frame lock:
 * updates the 3D emitters, waits for last frame's DMA, starts a new DMA list (with the speech streams' reads, through
 * sound_dma_flush_hook) and a new SFD set, renders an SFD for each AI buffer from sound_render_index up to the one
 * before the AI's (at most a task's worth, wrapping around the ring), starts the DMA list the SFDs need, then submits
 * last frame's SFD set as the audio task (its DMA is done) and refills the streams. So the output runs a frame
 * behind the render. The first call only notes where the AI is. */
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
    /* the buffer before the one the AI plays: render up to it */
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
    if (sound_render_index != RENDER_INDEX_NONE) {
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
                for (index = sound_render_index; index < target && count < sound_sfds_per_task; offset += SUBFRAME_FRAMES, count++) {
                    sound_hw_render_subframe(index++, offset);
                }
            } else {
                for (index = sound_render_index; index < sound_output_buffer_count && count < sound_sfds_per_task; offset += SUBFRAME_FRAMES, count++) {
                    sound_hw_render_subframe(index++, offset);
                }
                for (index = PC_RING_START(index); index < target && count < sound_sfds_per_task; offset += SUBFRAME_FRAMES, count++) {
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

/* Clears the frame hooks and has the game call sound_hw_service_frame every retrace. */
void sound_hw_install_callback(void)
{
    sound_render_index = RENDER_INDEX_NONE;
    sound_dma_flush_hook = 0;
    sound_frame_hook = 0;
    sound_service_add_callback(sound_hw_service_frame);
}

/* Lets the output drain (waits until sound_hw_service_frame has rendered a ring's worth of buffers more), then
 * removes the retrace callback. */
void sound_hw_remove_callback(void)
{
    sound_drain_count = sound_output_buffer_count;
    while (sound_drain_count > 0) { pc_os_spin_yield("sound_hw_remove_callback");
    }
    sound_service_remove_callback(sound_hw_service_frame);
}

/* The common start-up of both outputs: the voices (at most PC_MX_VOICE_SLOTS on the PC, 32 on the N64), the DMA
 * list and caches, the delay line, the retrace callback; the tick (the 60 Hz frame in whole subframes) and the
 * output description sound_get_output_info gives. */
void sound_hw_init_synth(unsigned int *frequency, unsigned short voice_count,
                   unsigned short delay_ms, unsigned char cache_mode)
{
    unsigned char i;
    float frame_samples;
    if (voice_count > PC_MX_CAP(32)) {
        voice_count = PC_MX_CAP(32);
    }
    sound_task_busy = 0;
    sound_hw_alloc_voices(voice_count, *frequency);
    sound_hw_alloc_dma(voice_count, *frequency, cache_mode);
    sound_hw_alloc_delay_line(*frequency, delay_ms);
    sound_hw_install_callback();
    frame_samples = (float)*frequency / 60.0f;
    sound_subframes_per_tick = (unsigned int)(frame_samples / 192.0f + 0.5f);
    sound_tick_countdown = sound_subframes_per_tick;
    sound_samples_per_tick = sound_subframes_per_tick * SUBFRAME_FRAMES;
    sound_output_info.frequency = *frequency;
    sound_output_info.sample_bits = 16;
    sound_output_info.mode = 1;
    /* the driver's name: the loop tests one copy of the text and copies the other */
    for (i = 0; sound_driver_name[i] != 0; i++) {
        sound_output_info.text[i] = sound_driver_name_copy[i];
    }
    sound_output_info.text[i] = 0;
}

/* The sample cache's size class from sound_hw_init's flags: 0x10000, 0x20000, 0x40000 and 0x80000 choose class 2,
 * 3, 4 and 0, none of them class 1 (an index into sound_sample_blocks_per_voice). */
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

/* Starts the hardware layer with its own output thread (sound_lib_init): the lock, the AI at *frequency (which
 * becomes the rate it gives), voice_count voices, a delay line of delay_ms, the cache class and half rate from flags.
 * Returns 0. */
int sound_hw_init(unsigned int *frequency, unsigned short voice_count, unsigned short delay_ms, unsigned int flags)
{
    sound_output_frozen = 0;
    sound_hw_init_lock();
    sound_half_rate = (flags & SOUND_INIT_HALF_RATE) != 0;
    sound_hw_start_output_thread(frequency);
    sound_hw_init_synth(frequency, voice_count, delay_ms, sound_hw_cache_mode_from_flags(flags));
    return 0;
}

/* Restarts the hardware layer over the output thread already running (sound_lib_init_shared_output: the game's
 * AudioInitSys starting the library again): as sound_hw_init, but the AI only gets its rate again, with no new
 * thread, and the half-rate setting stays as it was. Returns 0. */
int sound_hw_init_shared_output(unsigned int *frequency, unsigned short voice_count, unsigned short delay_ms, unsigned int flags)
{
    sound_hw_init_lock();
    sound_hw_set_output_frequency(frequency);
    sound_hw_init_synth(frequency, voice_count, delay_ms, sound_hw_cache_mode_from_flags(flags));
    return 0;
}

/* Shuts the hardware layer down (sound_lib_exit): the callback (after the output drains), the task, the delay
 * line, the DMA list, both caches, the voices and the output thread. */
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

/* As sound_hw_exit, but leaves the output thread running, for sound_hw_init_shared_output. */
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

/* Creates the library lock: a one-message queue holding its token. */
void sound_hw_init_lock(void)
{
    osCreateMesgQueue(&sound_lock_queue, &sound_lock_message, 1);
    osSendMesg(&sound_lock_queue, 0, OS_MESG_NOBLOCK);
}

/* Takes the library lock, which nests: the game's calls into the library hold it against the retrace callback. */
void sound_hw_lock(void)
{
    if (sound_lock_depth == 0) {
        osRecvMesg(&sound_lock_queue, 0, OS_MESG_BLOCK);
    }
    sound_lock_depth++;
}

/* Gives the library lock back when the outermost sound_hw_lock is undone. */
void sound_hw_unlock(void)
{
    if (sound_lock_depth > 0) {
        if (--sound_lock_depth == 0) {
            osSendMesg(&sound_lock_queue, 0, OS_MESG_NOBLOCK);
        }
    }
}

/* Takes the library lock for the retrace callback (not nesting: the callback never holds it already). */
void sound_hw_lock_frame(void)
{
    osRecvMesg(&sound_lock_queue, 0, OS_MESG_BLOCK);
}

/* Gives the retrace callback's lock back. */
void sound_hw_unlock_frame(void) {
    osSendMesg(&sound_lock_queue, NULL, OS_MESG_NOBLOCK);
}

/* PC: the synth's tick found driver voice `index` playing with its macro ended (synth.c): counted when its sample
 * loops, which then plays until the voice is taken again (--log-voices, docs/port/AUDIO.md 10.5). */
void pc_mx_note_macroless(int index)
{
    if (sound_hw_voices[index].loop_length != 0)
        pc_snd_count(PC_SND_MX_LOOP_ORPHAN);
}

/* Is voice index playing? */
unsigned char sound_hw_is_voice_active(int index)
{
    return sound_hw_voices[index].active != 0;
}

/* Whether the hardware is too busy for more: never, here. */
int sound_hw_is_busy(void)
{
    return 0;
}

/* Gives voice index a new sample (sample: data, start offset, length and loop in samples, format), resetting the
 * voice; with reset, also an envelope that is full at once (no attack or decay, sustain 1.0, a 20 ms release). A
 * sample after its loop keeps a tail of 10 samples or more. A sample outside RAM (the RDRAM window's K0 segment) is
 * in the cartridge: its data is fetched through the caches. PC: a host pointer (at or above 4 GiB) is RAM too; only
 * cartridge addresses are fetched by DMA. */
void sound_hw_set_voice_sample(unsigned int index, SoundHwSampleInfo *sample, unsigned char reset)
{
    sound_hw_reset_voice(index, 0);
    if (reset) {
        sound_hw_voices[index].attack_ms = 0;
        sound_hw_voices[index].decay_ms = 0;
        sound_hw_voices[index].sustain_level = 1.0f;
        sound_hw_voices[index].release_ms = DEFAULT_RELEASE_MS;
    }
    sound_hw_voices[index].data = sample->data;
    sound_hw_voices[index].sample_position = sample->offset;
    sound_hw_voices[index].latched_position = sound_hw_voices[index].position = sample->offset;
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
        sound_hw_voices[index].flags |= VOICE_FLAG_CARTRIDGE;
    }
}

/* Sets voice index's release to 20 ms and, with release, starts it now. */
void sound_hw_stop_sample(int index, unsigned char release)
{
    sound_hw_voices[index].release_ms = DEFAULT_RELEASE_MS;
    if (release) {
        sound_hw_release_envelope(&sound_hw_voices[index]);
    }
}

/* Sets voice index's envelope from four halfwords (a macro's ADSR step, maybe unaligned): attack and decay times
 * (ms), sustain level (4.12, 4096 = 1.0), release time (ms). */
void sound_hw_set_envelope(int index, unsigned short *data)
{
    sound_hw_voices[index].attack_ms = data[0];
    sound_hw_voices[index].decay_ms = data[1];
    sound_hw_voices[index].sustain_level = (float)(unsigned int)data[2] * (1.0 / 4096.0);
    sound_hw_voices[index].release_ms = data[3];
}

/* Starts voice index playing (sound_hw_activate_voice). */
void sound_hw_start_voice(unsigned int index)
{
    sound_hw_activate_voice((unsigned short)index);
}

/* Lets go of voice index: its envelope releases at the next subframe and a looping sample plays out its tail. */
void sound_hw_release_hold(int index)
{
    sound_hw_voices[index].key_held = 0;
}

/* A voice's priority: nothing at this level. */
void sound_hw_set_priority(int unused)
{
}

/* Sets voice index's rate (4.12 fixed point: input samples per output frame), at most 2.0, so a subframe's input
 * always fits the microcode's buffer. */
void sound_hw_set_pitch(int index, unsigned short rate)
{
    if (rate <= PITCH_MAX) {
        sound_hw_voices[index].rate = rate;
    } else {
        sound_hw_voices[index].rate = PITCH_MAX;
    }
}

/* Sets voice index's target gains from a volume, pan, surround span and aux (reverb) send, each 16.16 with 127.0
 * as the most (sound_compute_gains). The order of the targets is the SFD's (left, right, surround, aux, after
 * port/audio's names); compute_gains' first output goes to the second target and its second to the first. */
void sound_hw_set_volume(int index, unsigned int volume, unsigned int pan, unsigned int span, unsigned int aux)
{
    SoundHwVoice *voice;
    voice = &sound_hw_voices[index];
    sound_compute_gains((unsigned short *)&voice->target[1], (unsigned short *)&voice->target[0], volume, pan, span,
                        (unsigned short *)&voice->target[2], aux, (unsigned short *)&voice->target[3]);
}

/* Stops voice index at once, without a release: the next SFD's release mask frees its slot, and the microcode
 * fades its last output out instead of cutting it. */
void sound_hw_break_voice(int index)
{
    if (sound_hw_voices[index].active) {
        sound_hw_voices[index].active = 0;
        sound_hw_voices[index].release_pending = 1;
    }
}

/* Key-off for voice index, if it plays: a 20 ms release, started at the next subframe if the key was held, at
 * once if not. */
void sound_hw_key_off(int index)
{
    if (sound_hw_voices[index].active != 0) {
        sound_hw_voices[index].release_ms = DEFAULT_RELEASE_MS;
        if (sound_hw_voices[index].key_held != 0) {
            sound_hw_voices[index].key_held = 0;
        } else {
            sound_hw_release_envelope(&sound_hw_voices[index]);
        }
    }
}

/* Turns voice index off, with no release and no release-mask bit. */
void sound_hw_kill_voice(int index)
{
    sound_hw_voices[index].active = 0;
}

/* Turns the delay effect on with config (a SoundDelayConfig). */
void sound_hw_set_delay_effect(void *config)
{
    sound_hw_set_delay(config);
}

/* Turns the delay effect off. */
void sound_hw_remove_delay_effect(void)
{
    sound_hw_remove_delay();
}

/* Voice index's position, in samples, as of the frame before last's rendering: about where its output plays. */
unsigned int sound_hw_get_voice_position(int index)
{
    return sound_hw_voices[index].position;
}

/* A stream's samples written by the CPU: writes them back from the D-cache for the microcode (N64 only). */
void sound_hw_flush_stream(void *buffer, unsigned int samples)
{
    osWritebackDCache(buffer, samples * 2);
}

/* A stream's ring of samples: copies its first 4 samples past its end, so the resampler reading across the wrap
 * finds them, and writes them back from the D-cache. */
void sound_hw_flush_stream_wrap(short *buffer, unsigned int samples)
{
    buffer[samples + 0] = buffer[0];
    buffer[samples + 1] = buffer[1];
    buffer[samples + 2] = buffer[2];
    buffer[samples + 3] = buffer[3];
    osWritebackDCache(&buffer[samples], 8);
}

/* A sample's address for the hardware: RAM (K0) and cartridge (0xB0000000) addresses as they are, anything else
 * through the game's service (musyx_cart_address: an offset in the cartridge). */
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

/* (A hook for setting sample memory up: nothing to do on this hardware.) */
void sound_hw_init_sample_memory(void)
{
}

/* (A hook for keeping a sample's data: nothing to do.) */
void sound_hw_save_sample(void **descriptor, void **data)
{
}

/* (A hook for dropping a sample's data: nothing to do.) */
void sound_hw_remove_sample(void **descriptor, void *data)
{
}

/* (A hook run after the synth sets its voices up: nothing to do.) */
void sound_hw_commit_voices(void)
{
}

/* Freezes the output thread: it stops queueing buffers (the pre-NMI reset, audio_prenmi). */
void sound_hw_freeze_output(void)
{
    sound_output_frozen = 1;
}
