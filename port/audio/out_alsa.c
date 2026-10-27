/* port/audio/out_alsa.c: ALSA output (built when CMake finds the ALSA headers). A thread pulls 10 ms periods
 * from the queue and writes them; the device's blocking write paces it. The device is RUSH2049_ALSA_DEVICE,
 * else "default"; ALSA's plug layer converts the rate if the hardware needs it. */
#define _POSIX_C_SOURCE 200809L
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "out_backend.h"
#include "pc_audio.h"

static snd_pcm_t *pcm;
static pthread_t thread;
static atomic_int running;
static uint32_t period;

static void *writer(void *arg)
{
    int16_t *buf = malloc((size_t)period * 4);
    (void)arg;
    while (buf && running) {
        pc_audio_pull(buf, period);
        snd_pcm_sframes_t left = period;
        int16_t *p = buf;
        while (left > 0 && running) {
            snd_pcm_sframes_t n = snd_pcm_writei(pcm, p, (snd_pcm_uframes_t)left);
            if (n < 0) {
                if (snd_pcm_recover(pcm, (int)n, 1) < 0)
                    break;
                continue;
            }
            left -= n;
            p += 2 * n;
        }
    }
    free(buf);
    return NULL;
}

static int alsa_open(uint32_t want_rate, uint32_t *got_rate)
{
    const char *name = getenv("RUSH2049_ALSA_DEVICE");
    int err = snd_pcm_open(&pcm, name && *name ? name : "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
        return -1;
    err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, want_rate, 1, 60000);
    if (err < 0) {
        fprintf(stderr, "audio: alsa: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        pcm = NULL;
        return -1;
    }
    *got_rate = want_rate;
    period = want_rate / 100 ? want_rate / 100 : 1;
    running = 1;
    if (pthread_create(&thread, NULL, writer, NULL) != 0) {
        running = 0;
        snd_pcm_close(pcm);
        pcm = NULL;
        return -1;
    }
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
