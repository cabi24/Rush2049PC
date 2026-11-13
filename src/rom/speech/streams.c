/* src/rom/speech/streams.c: the speech module: playing entries of a speech bank on two streams.
 *
 * A speech bank (speech_set_bank) is a table of entries in the cartridge: a count, then one word per entry (its
 * rate in the high byte, 8 or 16 kHz; its offset in the bank's data in the low 24 bits). speech_play starts an
 * entry on a free stream (speech_streams[2]) and returns a token for it. Each frame the library's frame hook,
 * speech_service, decodes ahead (speech/decode.c, gsm.c), starts the stream voice (streams_3d.c's
 * sound_stream_start) once the PCM ring is primed, and frees the stream once it has played out or was stopped.
 * The streams have their own lock (speech_lock), apart from the library's. MusyX-side code with no MAT3
 * counterpart; the game never calls speech_init, so none of this runs (the module is linked in, unused).
 * N64-shaped: the bank is read by cartridge DMA, and speech_set_bank tells a cartridge address from a RAM one by
 * its top bits. */
#include "../rom_tu.h"
#include "../sound/sound_services.h"

#define SPEECH_STREAMS 2                        /* streams that can play at once */
#define SPEECH_FLAG_STATIC_BUFFERS 1            /* speech_init's flags: allocate both rings once, at the largest size */
#define SPEECH_STATIC_BUFFER_SAMPLES 4320       /* the ring speech_play sizes for 16 kHz */

/* A stream's lifecycle (SpeechStream.busy) */
enum {
    SPEECH_STREAM_FREE = 0,
    SPEECH_STREAM_STARTING = 1,   /* decoding ahead; the voice has not started */
    SPEECH_STREAM_PLAYING = 2,    /* the voice plays the ring */
    SPEECH_STREAM_STOPPING = 3    /* stopped or played out: the voice stops, then the stream is freed */
};
/* The decoder's state (SpeechStream.state, decode.c) */
enum { SPEECH_DECODE_IDLE = 0, SPEECH_DECODE_PRIMING = 1, SPEECH_DECODE_PRIMED = 2, SPEECH_DECODE_PLAYING = 3,
       SPEECH_DECODE_ENDED = 4 };

/* The stream record (PC: native layout, 0x1268 bytes; decode.c has the same, with a comment per field). The first
 * 400 bytes are the GSM decoder's state. */
typedef struct SpeechStream {
    unsigned char gsm_history[358];
    unsigned short gsm_last_lag;
    unsigned char gsm_rest[40];
    void (*read_input)(void *source, void *destination, unsigned int size, OSMesgQueue *done);
    unsigned char *input;
    void *output;
    unsigned int capacity;
    unsigned int input_ring[1024];
    unsigned short remaining_blocks;
    unsigned short header_low;
    unsigned int remaining_input;
    int remaining;
    OSMesgQueue queue;
    OSMesg message;
    unsigned int bit_position;
    unsigned int consumed_input;
    unsigned int received;
    unsigned int consumed;
    unsigned int available;
    signed char pending;
    signed char state;                  /* SPEECH_DECODE_* */
    signed char budget;
    unsigned char pad11FF;
    OSMesgQueue request_queue;
    OSMesg request_messages[2];
    volatile unsigned char busy;        /* SPEECH_STREAM_* */
    unsigned char volume;
    unsigned char pan;
    unsigned char span;
    unsigned char priority;
    unsigned char pad123D[3];
    unsigned int rate;
    int handle;                         /* the stream voice, -1 for none */
    short *buffer;                      /* the PCM ring */
    unsigned int buffer_count;          /* its length in samples */
    unsigned char stop_countdown;       /* SPEECH_STREAM_STOPPING: frames until the voice stops (at 2) and the stream frees (0) */
    unsigned char aux;
    unsigned char pad1256[2];
    unsigned int processed;             /* samples played */
    float duration;                     /* seconds */
    unsigned int token;
} SpeechStream;

typedef int (*SpeechStreamCallback)(short *, unsigned int, short *, unsigned int, unsigned int);

/* The speech module's data */
extern unsigned char sound_half_rate;
extern SpeechStream speech_streams[SPEECH_STREAMS];
extern volatile unsigned char speech_initialized[];
extern OSMesgQueue speech_lock_queue;
extern OSMesg speech_lock_message;
extern unsigned int speech_next_token;
extern void *speech_bank_data;
extern void *speech_bank_offsets;
extern unsigned int speech_bank_count;
extern unsigned int speech_flags;
extern void *speech_static_buffers[SPEECH_STREAMS];
extern unsigned char speech_bank_owned;
extern unsigned int sound_hw_cart_address(unsigned int);
extern int sound_hw_is_cartridge(const void *);
extern void *speech_bank;
extern void (*sound_frame_hook)(void);

/* The decoder (decode.c), the library and the OS */
extern unsigned int speech_take_samples(SpeechStream *, unsigned int);
extern void sound_dma_read_async(void *, void *, unsigned int);
extern unsigned int sound_stream_buffer_size(unsigned int);
extern void osInvalDCache(void *, int);
extern void speech_reset_decoder(SpeechStream *, void *, void *, unsigned int, void (*)(void *, void *, unsigned int, OSMesgQueue *));
extern void speech_mark_stopped(SpeechStream *);
extern void speech_decode(SpeechStream *);
extern void speech_mark_playing(SpeechStream *);
extern int sound_stream_start(unsigned char, short *, unsigned int, unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, SpeechStreamCallback, unsigned int);
extern void sound_stream_stop(int);
extern void sound_stream_set_volume(int, unsigned char, unsigned char, unsigned char, unsigned char);
extern void sound_dma_read_blocking(void *, void *, unsigned int);
extern void sound_hw_lock(void);
extern void sound_dma_remove_queue(void);
extern void sound_hw_unlock(void);

/* Doubles a stream's decode budget (frames per speech_service) when the library runs at half rate. */
void speech_adjust_decode_budget(SpeechStream *stream)
{
    if (sound_half_rate != 0) {
        stream->budget *= 2;
    }
}

/* The stream voice's callback (sound_stream_refill): the voice has played first_count samples at first and
 * second_count at second (the two parts of the ring when it wraps) of stream `selected`. Counts them as taken and
 * played. Returns 0: it writes nothing to the ring, so there is nothing for the caller to flush (the decoder
 * fills the ring itself). */
int speech_consume(short *first, unsigned int first_count,
                  short *second, unsigned int second_count, unsigned int selected)
{
    if (speech_initialized[0]) {
        speech_take_samples(&speech_streams[selected], first_count + second_count);
        speech_streams[selected].processed += first_count + second_count;
    }
    return 0;
}

/* A stream's read function: starts an async cartridge DMA of `count` bytes from source to destination and posts
 * the done message at once (the DMA layer orders it before anything that reads the data). */
void speech_read_input(void *source, void *destination, unsigned int count,
                   OSMesgQueue *queue)
{
    sound_dma_read_async(destination, source, count);
    osSendMesg(queue, 0, OS_MESG_BLOCK);
}

/* Creates the streams' lock: a one-message queue holding its message (free). */
void speech_init_lock(void)
{
    osCreateMesgQueue(&speech_lock_queue, &speech_lock_message, 1);
    osSendMesg(&speech_lock_queue, 0, OS_MESG_NOBLOCK);
}

/* Takes the streams' lock, waiting for it. Returns a token for speech_unlock (always 0). */
int speech_lock(void)
{
    osRecvMesg(&speech_lock_queue, 0, OS_MESG_BLOCK);
    return 0;
}

/* Gives the streams' lock back. token: speech_lock's, unused. */
void speech_unlock(int token)
{
    osSendMesg(&speech_lock_queue, 0, OS_MESG_NOBLOCK);
}

/* Takes the streams' lock for speech_service (the same lock as speech_lock). */
void speech_lock_frame(void)
{
    osRecvMesg(&speech_lock_queue, 0, OS_MESG_BLOCK);
}

/* Gives the lock back after speech_service. */
void speech_unlock_frame(void) {
    osSendMesg(&speech_lock_queue, NULL, OS_MESG_NOBLOCK);
}

/* Gives stream `selected` the next token no other busy stream holds (tokens count up, skipping the -1 that means
 * "none", and wrap) and returns it. */
unsigned int speech_assign_token(int selected)
{
    SpeechStream *stream;
    int i;

    for (;;) {
        for (i = 0; i < SPEECH_STREAMS; i++) {
            stream = &speech_streams[i];
            if (stream->busy && i != selected && stream->token == speech_next_token) {
                break;
            }
        }
        if (i == SPEECH_STREAMS) {
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

/* Returns the number of the busy stream holding `token`, or -1. */
int speech_find_stream(unsigned int token)
{
    int i;

    for (i = 0; i < SPEECH_STREAMS; i++) {
        if (speech_streams[i].busy && speech_streams[i].token == token) {
            return i;
        }
    }
    return -1;
}

/* Starts bank entry `index` on a free stream. rate: the sample rate in Hz, or 0 for the entry's own (8 or
 * 16 kHz); priority, volume, pan, span and aux are the stream voice's. The PCM ring holds about 8/30 s (rounded
 * up to whole GSM frames): from speech_flags' static buffers, else allocated. The voice starts later, once
 * speech_service has primed the ring. Returns the stream's token, or -1 (not initialized, no such entry, no free
 * stream, no memory). */
unsigned int speech_play(unsigned int index, unsigned int rate,
                           unsigned char priority, unsigned char volume,
                           unsigned char pan, unsigned char span,
                           unsigned char aux)
{
    SpeechStream *stream;
    int selected;
    int queue_token;
    unsigned int size;
    unsigned int frequency;

    if (speech_initialized[0] && index < speech_bank_count) {
        for (selected = 0; selected < SPEECH_STREAMS; selected++) {
            if (speech_streams[selected].busy == SPEECH_STREAM_FREE) {
                break;
            }
        }
        if (selected != SPEECH_STREAMS) {
            stream = &speech_streams[selected];
            osCreateMesgQueue(&stream->request_queue, stream->request_messages, 2);
            frequency = rate;
            if (!frequency) {
                frequency = ((((unsigned int *)speech_bank_offsets)[index] >> 24) & 255) == 0 ? 8000 : 16000;
            }
            stream->priority = priority;
            stream->rate = frequency;
            stream->volume = volume;
            stream->pan = pan;
            stream->span = span;
            stream->aux = aux;
            stream->buffer_count = ((frequency * 8 / 30 + 159) / SPEECH_FRAME_SAMPLES) * SPEECH_FRAME_SAMPLES;
            if (speech_flags & SPEECH_FLAG_STATIC_BUFFERS) {
                stream->buffer = speech_static_buffers[selected];
            } else {
                size = sound_stream_buffer_size(stream->buffer_count);
                stream->buffer = sound_services.allocate(size, 0);
                osInvalDCache(stream->buffer, size);
            }
            if (stream->buffer) {
                queue_token = speech_lock();
                speech_reset_decoder(stream, (unsigned char *)speech_bank_data + (((unsigned int *)speech_bank_offsets)[index] & 0xFFFFFF),
                stream->buffer, stream->buffer_count, speech_read_input);
                speech_adjust_decode_budget(stream);
                stream->handle = -1;
                stream->busy = SPEECH_STREAM_STARTING;
                speech_unlock(queue_token);
                return speech_assign_token(selected);
            }
        }
    }
    return (unsigned int)-1;
}

/* Stops stream `selected`: one whose voice has not started is freed at once; a playing one ends its decoding and
 * goes to stopping, where speech_service stops the voice two frames later and frees the stream two after that. */
void speech_stop_stream(int selected)
{
    int token;

    switch (speech_streams[selected].busy) {
    case SPEECH_STREAM_STARTING:
        speech_streams[selected].busy = SPEECH_STREAM_FREE;
        if (!(speech_flags & SPEECH_FLAG_STATIC_BUFFERS)) {
            sound_services.release(speech_streams[selected].buffer);
        }
        break;
    case SPEECH_STREAM_PLAYING:
        token = speech_lock();
        speech_mark_stopped(&speech_streams[selected]);
        speech_streams[selected].busy = SPEECH_STREAM_STOPPING;
        speech_streams[selected].stop_countdown = 4;
        speech_unlock(token);
        break;
    }
}

/* Stops the stream holding `token`. Returns 1 if there was one, else 0. */
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

/* Is the stream holding `token` still busy (1) or gone (0)? */
unsigned char speech_is_playing(unsigned int token)
{
    if (speech_initialized[0]) {
        token = speech_find_stream(token);
        if (token == -1) {
            return 0;
        }
        return speech_streams[token].busy != 0;
    }
    return 0;
}

/* Sets the volume, pan, span and aux send of the stream holding `token` on its voice, if it has one, and keeps
 * volume, pan and span for a voice still to start. Original game: not the aux send, so a voice that starts later
 * gets speech_play's. Returns 1 if there was such a stream, else 0. */
int speech_set_volume(unsigned int token, unsigned char volume,
                  unsigned char pan, unsigned char span,
                  unsigned char aux)
{
    int queue_token;

    if (speech_initialized[0]) {
        token = speech_find_stream(token);
        if (token != (unsigned int)-1) {
            queue_token = speech_lock();
            if (speech_streams[token].handle != -1) {
                sound_stream_set_volume(speech_streams[token].handle, volume, pan, span, aux);
            }
            speech_streams[token].pan = pan;
            speech_streams[token].span = span;
            speech_streams[token].volume = volume;
            speech_unlock(queue_token);
            return 1;
        }
    }
    return 0;
}

/* The library's frame hook (sound_frame_hook, run by sound_hw_service_frame before each frame's rendering), under
 * the streams' lock. For each busy stream: decode ahead (speech_decode); a starting stream whose ring is primed
 * starts its voice (its duration noted from the frames still to decode; freed if no voice is free); a playing
 * stream that has played out goes to stopping; a stopping stream counts down, stopping its voice at 2 and freeing
 * itself at 0. */
void speech_service(void)
{
    int selected;

    if (speech_initialized[0]) {
        speech_lock_frame();
        for (selected = 0; selected != SPEECH_STREAMS; selected++) {
            if (speech_streams[selected].busy) {
                speech_decode(&speech_streams[selected]);
                switch (speech_streams[selected].busy) {
                case SPEECH_STREAM_STARTING:
                    if (speech_streams[selected].state == SPEECH_DECODE_PRIMED) {
                        /* Original game: the frames left to decode, so the duration leaves out the ones already
                         * decoded into the ring (about 12 at 8 kHz). */
                        speech_streams[selected].duration = (float)(unsigned int)speech_streams[selected].remaining_blocks * 160.0f / speech_streams[selected].rate;
                        speech_streams[selected].processed = 0;
                        speech_streams[selected].busy = SPEECH_STREAM_PLAYING;
                        speech_mark_playing(&speech_streams[selected]);
                        speech_streams[selected].handle = sound_stream_start(speech_streams[selected].priority, speech_streams[selected].buffer, speech_streams[selected].buffer_count, speech_streams[selected].rate, speech_streams[selected].volume, speech_streams[selected].pan, speech_streams[selected].span, speech_streams[selected].aux, speech_consume, selected);
                        if (speech_streams[selected].handle == -1) {
                            speech_mark_stopped(&speech_streams[selected]);
                            if (!(speech_flags & SPEECH_FLAG_STATIC_BUFFERS)) sound_services.release(speech_streams[selected].buffer);
                            speech_streams[selected].busy = SPEECH_STREAM_FREE;
                        }
                    }
                    break;
                case SPEECH_STREAM_PLAYING:
                    if (speech_streams[selected].state == SPEECH_DECODE_ENDED) {
                        speech_streams[selected].busy = SPEECH_STREAM_STOPPING;
                        speech_streams[selected].stop_countdown = 4;
                    }
                    break;
                case SPEECH_STREAM_STOPPING:
                    if (speech_streams[selected].stop_countdown == 0) {
                        if (!(speech_flags & SPEECH_FLAG_STATIC_BUFFERS)) sound_services.release(speech_streams[selected].buffer);
                        speech_streams[selected].busy = SPEECH_STREAM_FREE;
                    } else {
                        if (speech_streams[selected].stop_countdown == 2) {
                            sound_stream_stop(speech_streams[selected].handle);
                            speech_streams[selected].handle = -1;
                        }
                        speech_streams[selected].stop_countdown--;
                    }
                    break;
                }
            }
        }
        speech_unlock_frame();
    }
}

/* The stream holding `token`: returns the seconds it has played (0 unless its voice plays) and stores its
 * duration in *duration. Returns 0 (and leaves *duration) when there is no such stream. */
float speech_get_position(unsigned int token, float *duration)
{
    int queue_token;
    float seconds;

    if (token != (unsigned int)-1 && speech_initialized[0]) {
        token = speech_find_stream(token);
        if (token != (unsigned int)-1) {
            queue_token = speech_lock();
            if (speech_streams[token].busy == SPEECH_STREAM_PLAYING) {
                seconds = (float)speech_streams[token].processed / (float)speech_streams[token].rate;
            } else {
                seconds = 0.0f;
            }
            *duration = speech_streams[token].duration;
            speech_unlock(queue_token);
            return seconds;
        }
    }
    return 0.0f;
}

/* Selects the speech bank. A bank in the cartridge (source a ROM offset or a cartridge address: sound_hw_is_cartridge)
 * has its count and offset table read into allocated memory by DMA, its entries' data following them; a bank in RAM
 * is used where it is, with its entries' data at `data` (in the cartridge). The speech code carries the cartridge
 * addresses it reads from in pointers (dma_queue.c). Frees the previous table if it was allocated here. Returns 1, or
 * 0 when the module is not initialized or out of memory. */
int speech_set_bank(void *source, void *data)
{
    unsigned int *count;

    if (speech_initialized[0]) {
        if (speech_bank_offsets && speech_bank_owned) {
            sound_services.release(speech_bank_offsets);
        }
        if (sound_hw_is_cartridge(source)) {
            speech_bank_data = speech_bank = (void *)(uintptr_t)sound_hw_cart_address((unsigned int)(uintptr_t)source);
            count = sound_services.allocate(4, 0);
            if (!count) {
                return 0;
            }
            sound_dma_read_blocking(count, speech_bank, 4);
            speech_bank_count = *count;
            sound_services.release(count);
            speech_bank_offsets = sound_services.allocate(speech_bank_count * 4, 0);
            if (!speech_bank_offsets) {
                return 0;
            }
            sound_dma_read_blocking(speech_bank_offsets, (unsigned char *)speech_bank + 4, speech_bank_count * 4);
            speech_bank_owned = 1;
        } else {
            speech_bank = source;
            speech_bank_data = (void *)(uintptr_t)sound_hw_cart_address((unsigned int)(uintptr_t)data);
            speech_bank_offsets = (unsigned int *)source + 1;
            speech_bank_count = *(unsigned int *)source;
            speech_bank_owned = 0;
        }
        return 1;
    }
    return 0;
}

/* Starts the speech module: both streams idle, the lock, the static rings if flags asks for them
 * (SPEECH_FLAG_STATIC_BUFFERS), and speech_service as the library's frame hook. */
void speech_init(unsigned int flags)
{
    int i;
    unsigned int size;

    speech_flags = flags;
    speech_bank_offsets = 0;
    for (i = 0; i < SPEECH_STREAMS; i++) {
        speech_reset_decoder(&speech_streams[i], 0, 0, 0, 0);
        speech_adjust_decode_budget(&speech_streams[i]);
        speech_streams[i].busy = SPEECH_STREAM_FREE;
    }
    speech_init_lock();
    if (speech_flags & SPEECH_FLAG_STATIC_BUFFERS) {
        size = sound_stream_buffer_size(SPEECH_STATIC_BUFFER_SAMPLES);
        for (i = 0; i < SPEECH_STREAMS; i++) {
            speech_static_buffers[i] = sound_services.allocate(size, 0);
            osInvalDCache(speech_static_buffers[i], size);
        }
    }
    sound_frame_hook = speech_service;
    speech_initialized[0] = 1;
    speech_next_token = 0;
}

/* Waits (spinning) until no stream is busy. */
void speech_wait_idle(void)
{
    int i;

    do {
        for (i = 0; i < SPEECH_STREAMS; i++) {
            if (speech_streams[i].busy != SPEECH_STREAM_FREE) {
                break;
            }
        }
    } while (i != SPEECH_STREAMS);
}

/* Shuts the speech module down: stops both streams and waits for them, removes the frame hook and the DMA
 * queue under the library's lock, and frees the bank's table and the static rings. */
void speech_exit(void)
{
    int i;

    for (i = 0; i < SPEECH_STREAMS; i++) {
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
    if (speech_flags & SPEECH_FLAG_STATIC_BUFFERS) {
        for (i = 0; i < SPEECH_STREAMS; i++) {
            sound_services.release(speech_static_buffers[i]);
        }
    }
}
