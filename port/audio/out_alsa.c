/* port/audio/out_alsa.c: ALSA output (built when CMake finds the ALSA headers). A thread pulls a period at a time
 * from the queue and writes it; the device's blocking write paces it. The device is RUSH2049_ALSA_DEVICE, else
 * "default", at 48 kHz unless a rate is forced; ALSA's plug layer converts if the hardware needs it.
 *
 * At the start the device's buffer (40 ms) is filled with silence but for one period. An empty buffer takes
 * several periods at once, faster than the AI has queued them: the first pulls ran dry. */
#define _POSIX_C_SOURCE 200809L
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "out_backend.h"
#include "pc_audio.h"

#define LATENCY_US 40000

static snd_pcm_t *pcm;
static pthread_t thread;
static atomic_int running;
static uint32_t period;

/* Write n frames; 0 when the device is gone. */
static int write_all(const int16_t *p, snd_pcm_sframes_t left)
{
    while (left > 0 && running) {
        snd_pcm_sframes_t n = snd_pcm_writei(pcm, p, (snd_pcm_uframes_t)left);
        if (n < 0) {
            if (snd_pcm_recover(pcm, (int)n, 1) < 0)
                return 0;
            continue;
        }
        left -= n;
        p += 2 * n;
    }
    return 1;
}

static void *writer(void *arg)
{
    int16_t *buf = malloc((size_t)period * 4);
    (void)arg;
    while (buf && running) {
        pc_audio_pull(buf, period);
        if (!write_all(buf, period))
            break;
    }
    free(buf);
    return NULL;
}

static int alsa_open(uint32_t want_rate, PcAudioDevice *dev)
{
    const char *name = getenv("RUSH2049_ALSA_DEVICE");
    uint32_t rate = want_rate ? want_rate : 48000;
    snd_pcm_uframes_t buffer_size = 0, period_size = 0;
    if (!name || !*name)
        name = "default";
    int err = snd_pcm_open(&pcm, name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
        return -1;
    err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, rate, 1, LATENCY_US);
    if (err < 0) {
        fprintf(stderr, "audio: alsa: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        pcm = NULL;
        return -1;
    }
    snd_pcm_get_params(pcm, &buffer_size, &period_size);
    period = period_size ? (uint32_t)period_size : rate / 100;
    running = 1;
    if (buffer_size > period) {
        size_t n = buffer_size - period;
        int16_t *zero = calloc(n, 4);
        if (zero) {
            write_all(zero, (snd_pcm_sframes_t)n);
            free(zero);
        }
    }
    if (pthread_create(&thread, NULL, writer, NULL) != 0) {
        running = 0;
        snd_pcm_close(pcm);
        pcm = NULL;
        return -1;
    }
    dev->rate = rate;
    dev->period = period;
    dev->buffer = (uint32_t)buffer_size;
    snprintf(dev->driver, sizeof dev->driver, "%s", name);
    return 0;
}

static void alsa_close(void)
{
    if (!pcm)
        return;
    running = 0;
    pthread_join(thread, NULL);
    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
    pcm = NULL;
}

const PcAudioBackendOps pc_audio_backend_alsa = {"alsa", alsa_open, alsa_close};
