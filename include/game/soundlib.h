/* include/game/soundlib.h: the records the MusyX sound library's C (src/rom/sound, src/rom/speech) and the game's own
 * C and data declarations share: the game's services to the library, the sample DMA list's entries, the cartridge
 * reads the speech streams queue, and the speech streams.
 *
 * One declaration each. The library's files include this after src/rom/rom_tu.h, the game's through game/audio.h
 * after game/types.h: it needs the basic types (u8 ... f32) and libultra's OSMesgQueue, OSMesg and OSTask, which
 * each side declares in its own headers, in the same layout. */
#ifndef GAME_SOUNDLIB_H
#define GAME_SOUNDLIB_H

/* The game's services to the sound library (sound_set_services: the game's audio_driver_services, which the library
 * copies to sound_services). The library calls nothing of the OS itself; MUSYX_DATA.md 8.1. */
typedef struct SoundServices {
    void (*submit_task)(OSTask *task);                       /* run an audio task (musyx_submit_audio_task) */
    void (*wait_task)(void);                                 /* wait for the last one */
    void (*add_callback)(void (*callback)(void));            /* call callback every retrace */
    void (*remove_callback)(void (*callback)(void));         /* ... no longer */
    void (*start_dma)(void *dst, u16 size, void *src);       /* a list of sample DMAs (musyx_start_dma_list) */
    u32 (*translate_address)(u32 address);                   /* a place in the cartridge: its ROM offset */
    void *(*allocate)(u32 size, u32 mode);                   /* memory from the game's heap */
    void (*release)(void *allocation);                       /* ... back */
} SoundServices;

/* An entry of the sample DMA list (sound_dma_list): length bytes from ROM offset source to destination.
 * The hardware layer (hardware.c) fills the list each frame with the sample and codebook caches' reads, and the
 * speech streams' queued reads (dma_queue.c) join it; the game's sample DMA thread runs it (sample_dma_next). */
typedef struct SampleDmaRequest {
    s32 length;
    u32 source;                  /* a ROM offset (sound_hw_cart_address) */
    u8 *destination;
} SampleDmaRequest;

/* A cartridge read a speech stream queued for the frame's DMA list (sound_dma_read_async, dma_queue.c). */
typedef struct TransferRequest {
    void *destination;
    void *source;                /* the ROM offset, kept in a pointer */
    u32 size;
} TransferRequest;

/* A speech stream (speech_streams[2], src/rom/speech): a bank entry's compressed data read from the cartridge in 1 KiB
 * blocks into a 4 KiB input ring, decoded a GSM frame at a time (gsm.c) into a PCM ring of `capacity` samples that
 * a stream voice plays (streams.c). The game never starts the speech module: nothing calls speech_init. */
typedef struct SpeechStream {
    /* The GSM decoder's state (gsm.c's GsmState, 400 bytes), which the decoder reads through the stream's first
     * bytes; the data loader lays it out as these: */
    u8 gsm_history[358];         /* the residual history and the filter states */
    u16 gsm_last_lag;            /* GsmState's nrp, the last valid long-term lag (the stream's setup sets 40) */
    u8 gsm_rest[40];             /* the log-area ratios, j, the repeat and silence counts */
    void (*read_input)(void *source, void *destination, u32 size, OSMesgQueue *done);  /* an input read */
    u8 *input;                   /* the entry's compressed data (a ROM offset) */
    void *output;                /* the PCM ring (the stream's buffer) */
    u32 capacity;                /* its length in samples */
    union {
        u8 bytes[4096];          /* the reads fill it a block at a time */
        u32 words[1024];         /* the decoder takes the entry's header and frames from it as words */
    } input_ring;
    u16 remaining_blocks;        /* GSM frames left to decode (header word 1, high half) */
    u16 header_low;              /* header word 1, low half (not used) */
    u32 remaining_input;         /* input words left to read (header word 2, low 24 bits) */
    s32 remaining;               /* samples left to play once the last frame is decoded */
    OSMesgQueue queue;           /* an input read's done message */
    OSMesg message;
    u32 bit_position;            /* the decoder's position in the input, in bits */
    u32 consumed_input;          /* bytes of input decoded (whole words) */
    u32 received;                /* bytes of input read */
    u32 consumed;                /* samples the voice has taken */
    u32 available;               /* samples decoded */
    s8 pending;                  /* an input read is in flight */
    s8 state;                    /* SPEECH_DECODE_* (decode.c) */
    s8 budget;                   /* frames to decode per call (2; 4 at half rate) */
    u8 unused1;
    OSMesgQueue request_queue;   /* made by speech_play, read by nothing */
    OSMesg request_messages[2];
    volatile u8 busy;            /* SPEECH_STREAM_* (streams.c) */
    u8 volume;
    u8 pan;
    u8 span;                     /* front/back */
    u8 priority;                 /* the voice's priority */
    u8 unused2[3];
    u32 rate;                    /* sample rate, Hz */
    s32 handle;                  /* the stream voice (sound_stream_start), -1 for none */
    s16 *buffer;                 /* the PCM ring */
    u32 buffer_count;            /* its length in samples */
    u8 stop_countdown;           /* frames until a stopping stream's voice stops (at 2), then frees (0) */
    u8 aux;                      /* the voice's effect send */
    u8 unused3[2];
    u32 processed;               /* samples played */
    f32 duration;                /* seconds, set when the voice starts */
    u32 token;                   /* the caller's handle (speech_assign_token) */
} SpeechStream;

#endif /* GAME_SOUNDLIB_H */
