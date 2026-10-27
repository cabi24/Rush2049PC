/* port/audio/out_sdl.c: SDL2 audio output (built when CMake finds SDL2). The callback pulls from the queue;
 * SDL converts format and channels if the device needs it, and the queue converts the rate. */
#include <SDL.h>

#include "out_backend.h"
#include "pc_audio.h"

static SDL_AudioDeviceID device;

static void SDLCALL callback(void *user, Uint8 *stream, int len)
{
    (void)user;
    pc_audio_pull((int16_t *)stream, (size_t)len / 4);
}

static int sdl_open(uint32_t want_rate, uint32_t *got_rate)
{
    SDL_AudioSpec want, have;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return -1;
    SDL_zero(want);
    want.freq = (int)want_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = callback;
    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!device) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return -1;
    }
    *got_rate = (uint32_t)have.freq;
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
