/* The master volume (pc_audio_set_volume): full volume leaves the samples as they were, the slider's square is the
 * gain, 0 is silence, the ends clamp, and it holds across a re-init. Through the manual backend's pulls. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

static int16_t s_in[400 * 2], s_out[400 * 2];

/* Push the same frames, pull them back at a volume. */
static void round_trip(double volume)
{
    pc_audio_set_volume(volume);
    pc_audio_push(s_in, 400);
    CHECK(pc_audio_pull(s_out, 400) == 400, "pulled 400 at volume %g", volume);
}

int main(void)
{
    int ok = 1;
    for (int i = 0; i < 400 * 2; i++)
        s_in[i] = (int16_t)(i % 2 ? -(i * 81 % 32768) : i * 81 % 32768);
    s_in[0] = 32767;
    s_in[1] = -32768;
    CHECK(pc_audio_init("manual", NULL) == 0, "manual init");
    pc_audio_set_frequency(22050);

    round_trip(1.0);
    CHECK(!memcmp(s_in, s_out, sizeof s_in), "full volume: the samples as they were");
    round_trip(0.5);
    for (int i = 0; i < 400 * 2; i++)
        ok &= s_out[i] == (int16_t)(s_in[i] * 16384 >> 16);
    CHECK(ok, "half the slider: a quarter of the amplitude (about -12 dB)");
    CHECK(s_out[0] == 8191 && s_out[1] == -8192, "the extremes: %d %d", s_out[0], s_out[1]);
    round_trip(0.0);
    ok = 1;
    for (int i = 0; i < 400 * 2; i++)
        ok &= s_out[i] == 0;
    CHECK(ok, "0: silence");
    round_trip(7.0);
    CHECK(!memcmp(s_in, s_out, sizeof s_in), "above 1 clamps to full volume");
    round_trip(-1.0);
    CHECK(s_out[0] == 0 && s_out[1] == 0, "below 0 clamps to silence");

    pc_audio_set_volume(0.5);
    pc_audio_shutdown();
    CHECK(pc_audio_init("manual", NULL) == 0, "manual init again");
    pc_audio_set_frequency(22050);
    pc_audio_push(s_in, 400);
    pc_audio_pull(s_out, 400);
    CHECK(s_out[0] == 8191, "the volume holds across a re-init: %d", s_out[0]);
    pc_audio_set_volume(1.0);
    pc_audio_shutdown();
    return test_finish("volume");
}
