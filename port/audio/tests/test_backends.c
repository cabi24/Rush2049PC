/* Device backends, where built: SDL2 through its "disk" driver (it writes what it plays to a raw file,
 * which is measured), ALSA through alsa-lib's "null" device. Each must open, drain the queue in real time
 * and close. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test.h"

static void sleep_ms(long ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static int16_t *tone(size_t n, double freq, double rate)
{
    int16_t *s = malloc(n * 4);
    for (size_t i = 0; i < n; i++)
        s[2 * i] = s[2 * i + 1] = (int16_t)lrint(9000 * sin(2 * M_PI * freq * i / rate));
    return s;
}

int main(void)
{
    int16_t *t = tone(22050, 440, 22050);
#ifdef PC_AUDIO_HAVE_SDL2
    {
        setenv("SDL_AUDIODRIVER", "disk", 1);
        setenv("SDL_DISKAUDIOFILE", "test_backends_sdl.raw", 1);
        CHECK(pc_audio_init("sdl", NULL) == 0, "sdl init");
        pc_audio_set_frequency(22050);
        CHECK(!strcmp(pc_audio_backend_name(), "sdl"), "backend %s", pc_audio_backend_name());
        uint32_t rate = pc_audio_device_rate();
        pc_audio_push(t, 22050);
        sleep_ms(1500);
        CHECK(pc_audio_queued_frames() == 0, "sdl left %zu queued", pc_audio_queued_frames());
        pc_audio_shutdown();
        FILE *f = fopen("test_backends_sdl.raw", "rb");
        CHECK(f != NULL, "no disk output");
        if (f) {
            static int16_t raw[48000 * 4 * 2];
            size_t n = fread(raw, 4, sizeof raw / 4, f), first = 0, last = 0;
            fclose(f);
            for (size_t i = 0; i < n; i++)
                if (raw[2 * i]) {
                    if (!first)
                        first = i;
                    last = i;
                }
            double freq = last > first ? tone_freq(raw + 2 * first, last - first, 2, rate) : 0;
            printf("sdl disk: %zu frames at %u Hz, tone %zu frames, %.3f Hz\n", n, rate, last - first, freq);
            CHECK(fabs(freq - 440) < 0.5, "sdl tone %.3f Hz", freq);
            CHECK(labs((long)(last - first) - (long)(22050.0 * rate / 22050)) < (long)rate / 50, "sdl tone length");
        }
    }
#endif
#ifdef PC_AUDIO_HAVE_ALSA
    {
        setenv("RUSH2049_ALSA_DEVICE", "null", 1);
        CHECK(pc_audio_init("alsa", NULL) == 0, "alsa init");
        pc_audio_set_frequency(22050);
        CHECK(!strcmp(pc_audio_backend_name(), "alsa"), "backend %s", pc_audio_backend_name());
        pc_audio_push(t, 11025);
        sleep_ms(800);
        CHECK(pc_audio_queued_frames() == 0, "alsa left %zu queued", pc_audio_queued_frames());
        pc_audio_shutdown();
    }
#endif
    free(t);
    return test_finish("backends");
}
