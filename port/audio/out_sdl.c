/* port/audio/out_sdl.c: SDL2 audio output (built when CMake finds SDL2). The callback pulls from the queue.
 *
 * The device opens at its own rate (SDL_GetDefaultAudioInfo, else 48 kHz) and the queue converts the game's
 * 22047 Hz to it. Asked for 22050 Hz, SDL 2.30's WASAPI driver leaves the conversion to WASAPI's own resampler
 * (AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM), which SDL's own source says has been reported to distort. The pull is the
 * device's period (SDL_AUDIO_ALLOW_SAMPLES_CHANGE): WASAPI's is 10 ms. A pull of another size puts a rebuffering
 * stream between the two, whose callback comes twice in some periods and not at all in others. The samples are
 * float: a band-limited signal at full scale (the game's mix saturates in places) overshoots it between its
 * samples, and 16 bits would clip that. WASAPI mixes in float; SDL converts to other formats and channel counts. */
#include <SDL.h>
#include <stdio.h>

#include "out_backend.h"
#include "pc_audio.h"

static SDL_AudioDeviceID device;

static void SDLCALL callback(void *user, Uint8 *stream, int len)
{
    (void)user;
    pc_audio_pull_f32((float *)stream, (size_t)len / 8);
}

/* The default output's own rate, or 0. */
static uint32_t native_rate(void)
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
    SDL_AudioSpec spec;
    if (SDL_GetDefaultAudioInfo(NULL, &spec, 0) == 0 && spec.freq >= 8000 && spec.freq <= 384000)
        return (uint32_t)spec.freq;
#endif
    return 0;
}

static int sdl_open(uint32_t want_rate, PcAudioDevice *dev)
{
    SDL_AudioSpec want, have;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "audio: sdl: %s\n", SDL_GetError());
        return -1;
    }
    uint32_t rate = want_rate ? want_rate : native_rate();
    if (!rate)
        rate = 48000;
    SDL_zero(want);
    want.freq = (int)rate;
    want.format = AUDIO_F32SYS;   /* the converter's overshoot past full scale reaches the device's mixer */
    want.channels = 2;
    want.samples = (Uint16)(rate / 100);   /* 10 ms, when the driver lets us choose */
    want.callback = callback;
    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!device) {
        fprintf(stderr, "audio: sdl: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return -1;
    }
    const char *drv = SDL_GetCurrentAudioDriver();
    dev->rate = (uint32_t)have.freq;
    dev->period = have.samples;
    dev->buffer = 0;
    snprintf(dev->driver, sizeof dev->driver, "%s", drv ? drv : "?");
    SDL_PauseAudioDevice(device, 0);
    return 0;
}

static void sdl_close(void)
{
    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

const PcAudioBackendOps pc_audio_backend_sdl = {"sdl", sdl_open, sdl_close};
