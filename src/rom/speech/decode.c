/* src/rom/speech/decode.c: the speech module's decoder side of a stream.
 *
 * A speech stream (speech/streams.c's SpeechStream, speech_streams[2]) reads a bank entry's compressed data from
 * the cartridge in 1 KiB blocks into a 4 KiB input ring (speech_read_input: async DMA), decodes GSM frames from it
 * (gsm.c) into a PCM ring of `capacity` samples, and hands the samples to the stream voice that plays the ring
 * (speech_take_samples, from the voice's callback). speech_service runs speech_decode once a frame for each busy
 * stream. MusyX-side code with no MAT3 counterpart; the game never starts the speech module (nothing calls
 * speech_init). N64-shaped: the input is read by cartridge DMA. */
#include "../rom_tu.h"

#define SPEECH_INPUT_BLOCK 1024          /* bytes per input read */
#define SPEECH_INPUT_RING 4096           /* bytes of input ring */
#define SPEECH_INPUT_AHEAD 3072          /* read another block when no more than this is buffered */
#define SPEECH_MIN_FRAME_INPUT 37        /* bytes the longest frame (with its run code) can need */
#define SPEECH_HEADER_BITS 96            /* the entry's three header words, skipped before the first frame */

/* The decoder's state (SpeechStream.state) */
enum {
    SPEECH_DECODE_IDLE = 0,     /* no data */
    SPEECH_DECODE_PRIMING = 1,  /* filling the PCM ring before the voice starts */
    SPEECH_DECODE_PRIMED = 2,   /* ring full: speech_service may start the voice */
    SPEECH_DECODE_PLAYING = 3,  /* the voice plays the ring */
    SPEECH_DECODE_ENDED = 4     /* played out, or stopped */
};

/* The stream record, as speech/streams.c has it (PC: native layout, 0x1268 bytes: the read function, the rings and
 * the message queues are host-sized). The first 400 bytes are the GSM decoder's state (gsm.c's GsmState); only its
 * last long-term lag is named here, since the stream's setup sets it. */
typedef struct SpeechStream {
    unsigned char gsm_history[358];             /* 0x000: GsmState: residual history, filter states */
    unsigned short gsm_last_lag;                /* 0x166: GsmState's nrp, the last valid long-term lag */
    unsigned char gsm_rest[40];                 /* 0x168: GsmState: LARpp, j, the repeat and silence counts */
    void (*read_input)(void *source, void *destination, unsigned int size, OSMesgQueue *done); /* 0x190 */
    unsigned char *input;                       /* 0x198: the entry's compressed data (cartridge address) */
    void *output;                               /* 0x1A0: the PCM ring (the stream's buffer) */
    unsigned int capacity;                      /* 0x1A8: its length in samples */
    unsigned int input_ring[1024];              /* 0x1AC: compressed input, read a block at a time */
    unsigned short remaining_blocks;            /* 0x11AC: GSM frames left to decode (header word 1, high half) */
    unsigned short header_low;                  /* 0x11AE: header word 1, low half (not used) */
    unsigned int remaining_input;               /* 0x11B0: input words left to read (header word 2, low 24 bits) */
    int remaining;                              /* 0x11B4: samples left to play once the last frame is decoded */
    OSMesgQueue queue;                          /* 0x11B8: an input read's done message */
    OSMesg message;                             /* 0x11E0 */
    unsigned int bit_position;                  /* 0x11E8: the decoder's position in the input, in bits */
    unsigned int consumed_input;                /* 0x11EC: bytes of input decoded (whole words) */
    unsigned int received;                      /* 0x11F0: bytes of input read */
    unsigned int consumed;                      /* 0x11F4: samples the voice has taken */
    unsigned int available;                     /* 0x11F8: samples decoded */
    signed char pending;                        /* 0x11FC: an input read is in flight */
    signed char state;                          /* 0x11FD: SPEECH_DECODE_* */
    signed char budget;                         /* 0x11FE: frames to decode per call (2; 4 at half rate) */
    unsigned char unknown_11DF;                 /* 0x11FF */
    OSMesgQueue request_queue;                  /* 0x1200: made by speech_play, read by nothing */
    OSMesg request_messages[2];                 /* 0x1228 */
    volatile unsigned char busy;                /* 0x1238: SPEECH_STREAM_* (streams.c) */
    unsigned char volume;                       /* 0x1239 */
    unsigned char pan;                          /* 0x123A */
    unsigned char span;                         /* 0x123B: front/back */
    unsigned char priority;                     /* 0x123C: the voice's priority */
    unsigned char unknown_1205[3];              /* 0x123D */
    unsigned int rate;                          /* 0x1240: sample rate, Hz */
    int handle;                                 /* 0x1244: the stream voice (sound_stream_start), -1 for none */
    short *buffer;                              /* 0x1248: the PCM ring */
    unsigned int buffer_count;                  /* 0x1250: its length in samples */
    unsigned char stop_countdown;               /* 0x1254: frames until a stopping stream's voice stops, then frees */
    unsigned char aux;                          /* 0x1255: the voice's effect send */
    unsigned char unknown_121A[2];              /* 0x1256 */
    unsigned int processed;                     /* 0x1258: samples played */
    float duration;                             /* 0x125C: seconds, set when the voice starts */
    unsigned int token;                         /* 0x1260: the caller's handle (speech_assign_token) */
} SpeechStream;
/* The GSM state fills the first 400 bytes (gsm.c's GsmStream puts the read function right after it). */
typedef char pc_stream_gsm_state[__builtin_offsetof(SpeechStream, read_input) == 400 ? 1 : -1];

extern void bzero(void *, int);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, int);
extern int osRecvMesg(OSMesgQueue *, OSMesg *, int);
extern int gsm_decode_stream_frame(SpeechStream *, void *);

/* Sets a stream up to decode the entry at data (or idle, when data or read_input is null): clears the GSM state
 * (with its last lag at 40, as GSM 06.10 starts), output ring `output` of `capacity` samples, the read function
 * and its done queue, the counters, and a budget of two frames per call. */
void speech_reset_decoder(SpeechStream *stream, void *data, void *output,
                   unsigned int capacity,
                   void (*read_input)(void *, void *, unsigned int, OSMesgQueue *))
{
    bzero(stream, 400);
    stream->gsm_last_lag = 40;
    stream->read_input = read_input;
    stream->input = data;
    stream->output = output;
    stream->capacity = capacity;
    stream->remaining = 0;
    osCreateMesgQueue(&stream->queue, &stream->message, 1);
    if (data == 0 || read_input == 0) {
        stream->state = SPEECH_DECODE_IDLE;
    } else {
        stream->state = SPEECH_DECODE_PRIMING;
    }
    stream->pending = 0;
    stream->remaining_blocks = stream->remaining_input = stream->header_low = 0;
    stream->bit_position = 0;
    stream->received = 0;
    stream->consumed_input = 0;
    stream->available = 0;
    stream->consumed = 0;
    stream->budget = 2;
}

/* One frame of decoding for a stream (speech_service). Collects a finished input read (the first block also
 * brings the entry's header: frame count, input length); starts the next 1 KiB read while the stream is live
 * and no more than 3 KiB of input is waiting; decodes up to `budget` frames into the free part of the PCM ring;
 * once the stream plays and the frames are done, fills the ring's free part with silence; and declares the
 * stream primed when the ring is nearly full. */
void speech_decode(SpeechStream *stream)
{
    int budget;
    int size;

    budget = stream->budget;
    if (stream->pending) {
        if (osRecvMesg(&stream->queue, 0, 0) >= 0) {
            if (stream->received == 0) {
                stream->remaining_blocks = (stream->input_ring[1] >> 16) & 65535;
                stream->header_low = stream->input_ring[1] & 65535;
                stream->remaining_input = stream->input_ring[2] & 0xFFFFFF;
                stream->bit_position = SPEECH_HEADER_BITS;
            }
            stream->received += SPEECH_INPUT_BLOCK;
            if (stream->remaining_input > 256) stream->remaining_input -= 256;
            else stream->remaining_input = 0;
            stream->pending = 0;
        }
    }
    if (!stream->pending && (stream->state == SPEECH_DECODE_PRIMING || stream->state == SPEECH_DECODE_PRIMED
                             || stream->state == SPEECH_DECODE_PLAYING)
        && stream->received - stream->consumed_input <= SPEECH_INPUT_AHEAD) {
        if (stream->received == 0) size = SPEECH_INPUT_BLOCK;
        else size = stream->remaining_input > 256 ? SPEECH_INPUT_BLOCK : stream->remaining_input * 4;
        if (size > 0) {
            stream->pending = 1;
            stream->read_input(stream->input + stream->received, (unsigned char *)stream->input_ring + ((stream->received % SPEECH_INPUT_RING) / 8) * 8, size, &stream->queue);
        }
    }
    /* Original game: while the stream is priming, this loop decodes even when remaining_blocks is 0. An entry
     * shorter than the priming fill (about 12 frames at 8 kHz) would take the 16-bit count past zero to 65535,
     * and the stream would then never be declared played out. The game plays no speech, so it never happens. */
    while (stream->available - stream->consumed < stream->capacity - SPEECH_FRAME_SAMPLES
        && (stream->state == SPEECH_DECODE_PRIMING || stream->remaining_blocks > 0)
        && stream->received - stream->consumed_input >= SPEECH_MIN_FRAME_INPUT && budget > 0) {
        gsm_decode_stream_frame(stream, (unsigned char *)stream->output + ((stream->available % stream->capacity) / 4) * 8);
        stream->consumed_input = (stream->bit_position >> 3) & ~3U;
        stream->available += SPEECH_FRAME_SAMPLES;
        if (--stream->remaining_blocks == 0) stream->remaining = stream->capacity;
        budget--;
    }
    if ((stream->state == SPEECH_DECODE_PLAYING || stream->state == SPEECH_DECODE_ENDED) && stream->remaining_blocks <= 0) {
        while (stream->available - stream->consumed < stream->capacity - SPEECH_FRAME_SAMPLES && budget > 0) {
            bzero((unsigned char *)stream->output + ((stream->available % stream->capacity) / 4) * 8, SPEECH_FRAME_SAMPLES * 2);
            stream->available += SPEECH_FRAME_SAMPLES;
            budget--;
        }
    }
    if (stream->state == SPEECH_DECODE_PRIMING && stream->available - stream->consumed >= stream->capacity - 2 * SPEECH_FRAME_SAMPLES)
        stream->state = SPEECH_DECODE_PRIMED;
}

/* The voice has played `count` more samples of a playing stream (speech_consume, the voice's callback): counts
 * down the play-out after the last frame (then the stream has ended) and takes up to `count` decoded samples.
 * Returns the samples taken, 0 when the stream is not playing. */
unsigned int speech_take_samples(SpeechStream *stream, unsigned int count)
{
    unsigned int amount;

    if (stream->state == SPEECH_DECODE_PLAYING) {
        if (stream->remaining > 0) {
            stream->remaining -= count;
            if (stream->remaining <= 0) {
                stream->state = SPEECH_DECODE_ENDED;
                stream->remaining = 0;
            }
        }
        amount = count < stream->available - stream->consumed ? count : stream->available - stream->consumed;
        stream->consumed += amount;
        return amount;
    }
    return 0;
}

/* The voice has started: a primed stream now plays. */
void speech_mark_playing(SpeechStream *stream)
{
    if (stream->state == SPEECH_DECODE_PRIMED) {
        stream->state = SPEECH_DECODE_PLAYING;
    }
}

/* Ends a stream's decoding (a stop, or a voice that could not start). */
void speech_mark_stopped(SpeechStream *stream)
{
    stream->state = SPEECH_DECODE_ENDED;
}
