/* port/audio/out_backend.h: what a device backend provides to audio_out.c. A backend pulls device-rate
 * frames with pc_audio_pull from its own thread or callback. open and close are never called with the
 * queue's lock held. */
#ifndef PC_OUT_BACKEND_H
#define PC_OUT_BACKEND_H

#include <stdint.h>

typedef struct PcAudioBackendOps {
    const char *name;
    int (*open)(uint32_t want_rate, uint32_t *got_rate);   /* 0 on success, then pulling has started */
    void (*close)(void);
} PcAudioBackendOps;

#ifdef PC_AUDIO_HAVE_SDL2
extern const PcAudioBackendOps pc_audio_backend_sdl;
#endif
#ifdef PC_AUDIO_HAVE_ALSA
extern const PcAudioBackendOps pc_audio_backend_alsa;
#endif

#endif
