/* port/audio/out_backend.h: what a device backend provides to audio_out.c. A backend pulls device-rate
 * frames with pc_audio_pull from its own thread or callback. open and close are never called with the
 * queue's lock held. */
#ifndef PC_OUT_BACKEND_H
#define PC_OUT_BACKEND_H

#include <stdint.h>

/* What the device opened as, for the queue and the log. */
typedef struct PcAudioDevice {
    uint32_t rate;            /* frames a second */
    uint32_t period;          /* frames a pull takes (SDL's callback, ALSA's period) */
    uint32_t buffer;          /* frames the device itself holds, when the backend knows (0 otherwise) */
    char driver[32];          /* the system's driver: SDL's (wasapi, directsound, pulseaudio, disk...), ALSA's PCM */
} PcAudioDevice;

typedef struct PcAudioBackendOps {
    const char *name;
    /* want_rate 0: the device's own rate. 0 on success, then pulling has started. */
    int (*open)(uint32_t want_rate, PcAudioDevice *dev);
    void (*close)(void);
} PcAudioBackendOps;

#ifdef PC_AUDIO_HAVE_SDL2
extern const PcAudioBackendOps pc_audio_backend_sdl;
#endif
#ifdef PC_AUDIO_HAVE_ALSA
extern const PcAudioBackendOps pc_audio_backend_alsa;
#endif

#endif
