/* GENERATED ROM-aligned TU — segment 0x25bb0 (rom/lib_25bb0)
 * layout map f5baec304e2af6053d0e085c8e4b667c97ff73771969d95aeccac477c5f6c448.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* Shared 0x1228 stream record. Named members replace only observed ranges;
 * signed state/scale, volatile busy, SDK queues and callback ABI are retained.
 * Hoisted before the first slot that uses it (
 * speech_assign_token); text unchanged. */
typedef struct SpeechStream { unsigned char unknown_0000[358]; unsigned short block_count; unsigned char unknown_0168[40]; void (*callback)(void *, void *, unsigned int, OSMesgQueue *); void *data; void *argument2; unsigned int argument3; unsigned char unknown_01A0[4096]; unsigned short read_count; unsigned short write_count; unsigned int buffered; int remaining; OSMesgQueue queue; OSMesg message; unsigned int field_11C8; unsigned int field_11CC; unsigned int field_11D0; unsigned int consumed; unsigned int available; signed char field_11DC; signed char state; signed char scale; unsigned char unknown_11DF; OSMesgQueue request_queue; OSMesg request_messages[2]; volatile unsigned char busy; unsigned char value1; unsigned char value2; unsigned char value3; unsigned char option; unsigned char unknown_1205[3]; unsigned int rate; int handle; short *buffer; unsigned int buffer_count; unsigned char request_state; unsigned char mode; unsigned char unknown_121A[2]; unsigned int processed; float field_1220; unsigned int token; } SpeechStream;

/* PROMOTED 2026-10-04 — speech_adjust_decode_budget
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: a stream (speech_streams) holds host pointers before its state, so this view is the full one */
typedef SpeechStream StreamState;
extern unsigned char sound_half_rate;
void speech_adjust_decode_budget(StreamState *stream)
{
    if (sound_half_rate != 0) {
        stream->scale *= 2;
    }
}

/* PROMOTED 2026-10-11 — speech_consume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SpeechStream speech_streams[2];
extern volatile unsigned char speech_initialized[];
extern unsigned int speech_take_samples(SpeechStream *, unsigned int);
int speech_consume(short *first, unsigned int first_count,
                  short *second, unsigned int second_count, unsigned int selected)
{
    if (speech_initialized[0]) {
        speech_take_samples(&speech_streams[selected], first_count + second_count);
        speech_streams[selected].processed += first_count + second_count;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — speech_read_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_dma_read_async(void *, void *, unsigned int);
extern int osSendMesg(OSMesgQueue *, void *, int);
void speech_read_input(void *source, void *destination, unsigned int count,
                   OSMesgQueue *queue)
{
    sound_dma_read_async(destination, source, count);
    osSendMesg(queue, 0, 1);
}

/* PROMOTED 2026-10-04 — speech_init_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, int);
extern int osSendMesg(OSMesgQueue *, OSMesg, int);
extern OSMesgQueue speech_lock_queue;
extern OSMesg speech_lock_message;
void speech_init_lock(void)
{
    osCreateMesgQueue(&speech_lock_queue, &speech_lock_message, 1);
    osSendMesg(&speech_lock_queue, 0, 0);
}

/* PROMOTED 2026-10-05 — speech_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int osRecvMesg(OSMesgQueue *, void **, int);
int speech_lock(void)
{
    osRecvMesg(&speech_lock_queue, 0, 1);
    return 0;
}

/* PROMOTED 2026-10-05 — speech_unlock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void speech_unlock(int token)
{
    osSendMesg(&speech_lock_queue, 0, 0);
}

/* PROMOTED 2026-10-05 — speech_lock_frame
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void speech_lock_frame(void)
{
    osRecvMesg(&speech_lock_queue, 0, 1);
}

/* PROMOTED 2026-10-11 — speech_unlock_frame
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#define NULL ((void *)0)
#define OS_MESG_NOBLOCK 0
extern s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void speech_unlock_frame(void) {
    osSendMesg(&speech_lock_queue, NULL, OS_MESG_NOBLOCK);
}

/* PROMOTED 2026-10-05 — speech_assign_token
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SpeechStream speech_streams[2];
extern unsigned int speech_next_token;
unsigned int speech_assign_token(int selected)
{
    SpeechStream *stream;
    int i;

    for (;;) {
        for (i = 0; i < 2; i++) {
            stream = &speech_streams[i];
            if (stream->busy && i != selected && stream->token == speech_next_token) {
                break;
            }
        }
        if (i == 2) {
            break;
        }
        speech_next_token++;
        if (speech_next_token == (unsigned int)-1) {
            speech_next_token = 0;
        }
    }
    stream = &speech_streams[selected];
    stream->token = speech_next_token;
    speech_next_token++;
    if (speech_next_token == (unsigned int)-1) {
        speech_next_token = 0;
    }
    return stream->token;
}

/* PROMOTED 2026-10-04 — speech_find_stream
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SpeechStream speech_streams[2];
int speech_find_stream(unsigned int token)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (speech_streams[i].busy && speech_streams[i].token == token) {
            return i;
        }
    }
    return -1;
}

/* PROMOTED 2026-10-11 — speech_play
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
#include "PR/os_message.h"
extern void *(*sound_service_alloc)(unsigned int, unsigned int);
extern void *speech_bank_data;
extern void *speech_bank_offsets;
extern unsigned int speech_bank_count;
extern unsigned int speech_flags;
extern void *speech_static_buffers[2];
extern unsigned int sound_stream_buffer_size(unsigned int);
extern void osInvalDCache(void *, int);
extern int speech_lock(void);
extern void speech_unlock(int);
extern unsigned int speech_assign_token(int);
extern void speech_adjust_decode_budget(StreamState *);
extern void speech_read_input(void *, void *, unsigned int, OSMesgQueue *);
extern void speech_reset_decoder(SpeechStream *, void *, void *, unsigned int, void (*)(void *, void *, unsigned int, OSMesgQueue *));
unsigned int speech_play(unsigned int index, unsigned int rate,
                           unsigned char option, unsigned char value1,
                           unsigned char value2, unsigned char value3,
                           unsigned char mode)
{
    SpeechStream *stream;
    int selected;
    int queue_token;
    unsigned int size;
    unsigned int frequency;

    if (speech_initialized[0] && index < speech_bank_count) {
        for (selected = 0; selected < 2; selected++) {
            if (speech_streams[selected].busy == 0) {
                break;
            }
        }
        if (selected != 2) {
            stream = &speech_streams[selected];
            osCreateMesgQueue(&stream->request_queue, stream->request_messages, 2);
            frequency = rate;
            if (!frequency) {
                frequency = ((((unsigned int *)speech_bank_offsets)[index] >> 24) & 255) == 0 ? 8000 : 16000;
            }
            stream->option = option;
            stream->rate = frequency;
            stream->value1 = value1;
            stream->value2 = value2;
            stream->value3 = value3;
            stream->mode = mode;
            stream->buffer_count = ((frequency * 8 / 30 + 159) / 160) * 160;
            if (speech_flags & 1) {
                stream->buffer = speech_static_buffers[selected];
            } else {
                size = sound_stream_buffer_size(stream->buffer_count);
                stream->buffer = sound_service_alloc(size, 0);
                osInvalDCache(stream->buffer, size);
            }
            if (stream->buffer) {
                queue_token = speech_lock();
                speech_reset_decoder(stream, (unsigned char *)speech_bank_data + (((unsigned int *)speech_bank_offsets)[index] & 0xFFFFFF),
                stream->buffer, stream->buffer_count, speech_read_input);
                speech_adjust_decode_budget((StreamState *)stream);
                stream->handle = -1;
                stream->busy = 1;
                speech_unlock(queue_token);
                return speech_assign_token(selected);
            }
        }
    }
    return (unsigned int)-1;
}

/* PROMOTED 2026-10-05 — speech_stop_stream
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int speech_flags;
extern void (*sound_service_free)(void *);
extern int speech_lock(void);
extern void speech_unlock(int);
extern void speech_mark_stopped(SpeechStream *);
void speech_stop_stream(int selected)
{
    int token;

    switch (speech_streams[selected].busy) {
    case 1:
        speech_streams[selected].busy = 0;
        if (!(speech_flags & 1)) {
            sound_service_free(speech_streams[selected].buffer);
        }
        break;
    case 2:
        token = speech_lock();
        speech_mark_stopped(&speech_streams[selected]);
        speech_streams[selected].busy = 3;
        speech_streams[selected].request_state = 4;
        speech_unlock(token);
        break;
    }
}

/* PROMOTED 2026-10-04 — speech_stop
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile unsigned char speech_initialized[];
extern int speech_find_stream(unsigned int);
extern void speech_stop_stream(int);
int speech_stop(unsigned int token)
{
    int selected;

    if (token != (unsigned int)-1 && speech_initialized[0]) {
        selected = speech_find_stream(token);
        if (selected != -1) {
            speech_stop_stream(selected);
            return 1;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — speech_is_playing
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile unsigned char speech_stream_busy[][sizeof(SpeechStream)];
unsigned char speech_is_playing(unsigned int token)
{
    if (speech_initialized[0]) {
        token = speech_find_stream(token);
        if (token == -1) {
            return 0;
        }
        return speech_stream_busy[token][0] != 0;
    }
    return 0;
}

/* PROMOTED 2026-10-05 — speech_set_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_stream_set_volume(int, unsigned char, unsigned char, unsigned char, unsigned char);
int speech_set_volume(unsigned int token, unsigned char value1,
                  unsigned char value2, unsigned char value3,
                  unsigned char value4)
{
    int queue_token;

    if (speech_initialized[0]) {
        token = speech_find_stream(token);
        if (token != (unsigned int)-1) {
            queue_token = speech_lock();
            if (speech_streams[token].handle != -1) {
                sound_stream_set_volume(speech_streams[token].handle, value1, value2, value3, value4);
            }
            speech_streams[token].value2 = value2;
            speech_streams[token].value3 = value3;
            speech_streams[token].value1 = value1;
            speech_unlock(queue_token);
            return 1;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-05 — speech_service
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef int (*SpeechStreamCallback)(short *, unsigned int, short *, unsigned int, unsigned int);
/* the six callbacks before allocate are function pointers too (docs/port/MUSYX_DATA.md 8.1) */
typedef struct SpeechServiceHooks { void (*unknown_00[6])(void); void *(*allocate)(unsigned int, int); void (*release)(void *); } SpeechServiceHooks;
extern SpeechServiceHooks sound_services;
extern void speech_lock_frame(void);
extern void speech_unlock_frame(void);
extern void speech_decode(SpeechStream *);
extern void speech_mark_playing(SpeechStream *);
extern int sound_stream_start(unsigned char, short *, unsigned int, unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, SpeechStreamCallback, unsigned int);
extern void sound_stream_stop(int);
extern int speech_consume(short *, unsigned int, short *, unsigned int, unsigned int);
void speech_service(void)
{
    int selected;

    if (speech_initialized[0]) {
        speech_lock_frame();
        for (selected = 0; selected != 2; selected++) {
            if (speech_streams[selected].busy) {
                speech_decode(&speech_streams[selected]);
                switch (speech_streams[selected].busy) {
                case 1:
                    if (speech_streams[selected].state == 2) {
                        speech_streams[selected].field_1220 = (float)(unsigned int)speech_streams[selected].read_count * 160.0f / speech_streams[selected].rate;
                        speech_streams[selected].processed = 0;
                        speech_streams[selected].busy = 2;
                        speech_mark_playing(&speech_streams[selected]);
                        speech_streams[selected].handle = sound_stream_start(speech_streams[selected].option, speech_streams[selected].buffer, speech_streams[selected].buffer_count, speech_streams[selected].rate, speech_streams[selected].value1, speech_streams[selected].value2, speech_streams[selected].value3, speech_streams[selected].mode, speech_consume, selected);
                        if (speech_streams[selected].handle == -1) {
                            speech_mark_stopped(&speech_streams[selected]);
                            if (!(speech_flags & 1)) sound_services.release(speech_streams[selected].buffer);
                            speech_streams[selected].busy = 0;
                        }
                    }
                    break;
                case 2:
                    if (speech_streams[selected].state == 4) {
                        speech_streams[selected].busy = 3;
                        speech_streams[selected].request_state = 4;
                    }
                    break;
                case 3:
                    if (speech_streams[selected].request_state == 0) {
                        if (!(speech_flags & 1)) sound_services.release(speech_streams[selected].buffer);
                        speech_streams[selected].busy = 0;
                    } else {
                        if (speech_streams[selected].request_state == 2) {
                            sound_stream_stop(speech_streams[selected].handle);
                            speech_streams[selected].handle = -1;
                        }
                        speech_streams[selected].request_state--;
                    }
                    break;
                }
            }
        }
        speech_unlock_frame();
    }
}

/* PROMOTED 2026-10-05 — speech_get_position
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
float speech_get_position(unsigned int token, float *value)
{
    int queue_token;
    float ratio;

    if (token != (unsigned int)-1 && speech_initialized[0]) {
        token = speech_find_stream(token);
        if (token != (unsigned int)-1) {
            queue_token = speech_lock();
            if (speech_streams[token].busy == 2) {
                ratio = (float)speech_streams[token].processed / (float)speech_streams[token].rate;
            } else {
                ratio = 0.0f;
            }
            *value = speech_streams[token].field_1220;
            speech_unlock(queue_token);
            return ratio;
        }
    }
    return 0.0f;
}

/* PROMOTED 2026-10-11 — speech_set_bank
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char speech_bank_owned;
extern void *(*sound_service_translate_address)(void *);
extern void *(*sound_service_alloc)(unsigned int, unsigned int);
extern void *speech_bank;
extern void *speech_bank_data;
extern void *speech_bank_offsets;
extern unsigned int speech_bank_count;
extern void sound_dma_read_blocking(void *, void *, unsigned int);
int speech_set_bank(void *source, void *data)
{
    unsigned int *count;

    if (speech_initialized[0]) {
        if (speech_bank_offsets && speech_bank_owned) {
            sound_service_free(speech_bank_offsets);
        }
        if (((unsigned int)source & 0xF0000000) != 0x80000000) {
            speech_bank_data = speech_bank = sound_service_translate_address(source);
            count = sound_service_alloc(4, 0);
            if (!count) {
                return 0;
            }
            sound_dma_read_blocking(count, speech_bank, 4);
            speech_bank_count = *count;
            sound_service_free(count);
            speech_bank_offsets = sound_service_alloc(speech_bank_count * 4, 0);
            if (!speech_bank_offsets) {
                return 0;
            }
            sound_dma_read_blocking(speech_bank_offsets, (unsigned char *)speech_bank + 4, speech_bank_count * 4);
            speech_bank_owned = 1;
        } else {
            speech_bank = source;
            speech_bank_data = sound_service_translate_address(data);
            speech_bank_offsets = (unsigned int *)source + 1;
            speech_bank_count = *(unsigned int *)source;
            speech_bank_owned = 0;
        }
        return 1;
    }
    return 0;
}

/* PROMOTED 2026-10-05 — speech_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *speech_bank_offsets;
extern void *speech_static_buffers[2];
extern void (*sound_frame_hook)(void);
extern void speech_reset_decoder(SpeechStream *, void *, void *, unsigned int, void (*)(void *, void *, unsigned int, OSMesgQueue *));
extern void speech_adjust_decode_budget(StreamState *);
extern void speech_init_lock(void);
extern unsigned int sound_stream_buffer_size(unsigned int);
extern void osInvalDCache(void *, int);
extern void speech_service(void);
void speech_init(unsigned int flags)
{
    int i;
    unsigned int size;

    speech_flags = flags;
    speech_bank_offsets = 0;
    for (i = 0; i < 2; i++) {
        speech_reset_decoder(&speech_streams[i], 0, 0, 0, 0);
        speech_adjust_decode_budget((StreamState *)&speech_streams[i]);
        speech_streams[i].busy = 0;
    }
    speech_init_lock();
    if (speech_flags & 1) {
        size = sound_stream_buffer_size(4320);
        for (i = 0; i < 2; i++) {
            speech_static_buffers[i] = sound_services.allocate(size, 0);
            osInvalDCache(speech_static_buffers[i], size);
        }
    }
    sound_frame_hook = speech_service;
    speech_initialized[0] = 1;
    speech_next_token = 0;
}

/* PROMOTED 2026-10-05 — speech_wait_idle
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void speech_wait_idle(void)
{
    int i;

    do {
        for (i = 0; i < 2; i++) {
            if (speech_streams[i].busy != 0) {
                break;
            }
        }
    } while (i != 2);
}

/* PROMOTED 2026-10-05 — speech_exit
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char speech_bank_owned;
extern void (*sound_frame_hook)(void);
extern void *speech_bank_offsets;
extern void *speech_static_buffers[2];
extern void speech_wait_idle(void);
extern void sound_hw_lock(void);
extern void sound_dma_remove_queue(void);
extern void sound_hw_unlock(void);
void speech_exit(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        speech_stop_stream(i);
    }
    speech_wait_idle();
    speech_initialized[0] = 0;
    sound_hw_lock();
    sound_frame_hook = 0;
    sound_dma_remove_queue();
    sound_hw_unlock();
    if (speech_bank_offsets && speech_bank_owned) {
        sound_services.release(speech_bank_offsets);
    }
    if (speech_flags & 1) {
        for (i = 0; i < 2; i++) {
            sound_services.release(speech_static_buffers[i]);
        }
    }
}

