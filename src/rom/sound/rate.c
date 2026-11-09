/* src/rom/sound/rate.c: two small services of the MusyX sound library: a sample rate as a voice pitch, and the
 * library's version text. MusyX is the N64's sound library: no MAT3 counterpart. */
#include "../rom_tu.h"

extern int sound_synth_frequency;
extern unsigned char sound_version_text[];

/* A sample rate in Hz as a voice's rate at the synth's output rate: 4.12 fixed point, 4096 = the output rate. */
unsigned int sound_rate_to_pitch(unsigned int rate)
{
    return ((float)rate * 4096.0f) / sound_synth_frequency;
}

/* The library's version text. */
void *sound_get_version_text(void)
{
    return sound_version_text;
}
