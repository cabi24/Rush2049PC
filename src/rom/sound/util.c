/* src/rom/sound/util.c: small services of the MusyX sound library: memory through the game's allocator, the random
 * numbers and the sine table the synth's LFOs and macros use, a binary search for the resource registries, and
 * conversions between milliseconds, ticks and the library's time unit. MusyX is the N64's sound library: no MAT3
 * counterpart. */
#include "../rom_tu.h"
#include "sound_services.h"

#define SINE_QUARTER 1024              /* sound_sine_table: one quarter wave, 1024 steps */

extern unsigned int sound_random_seed;
extern const short sound_sine_table[1024];
extern u32 sound_synth_get_tick_rate(unsigned char *);

/* size bytes from the game's allocator (sound_set_services). */
void *sound_alloc(unsigned int size)
{
    return sound_services.allocate(size, 0);
}

/* Gives memory from sound_alloc back. */
void sound_free(void *allocation)
{
    sound_services.release(allocation);
}

/* The next pseudo-random number: a multiplicative step of sound_random_seed, its bits 6 to 21. */
unsigned short sound_random(void)
{
    sound_random_seed *= 2822053219U;
    return sound_random_seed >> 6;
}

/* The sine of phase (4096 steps a turn), from the quarter-wave table: rising, falling (the table read backwards
 * from its end), and both negated. */
short sound_sine(u16 phase)
{
    phase &= 4095;
    if (phase < SINE_QUARTER) {
        return sound_sine_table[phase];
    }
    if (phase < 2 * SINE_QUARTER) {
        return sound_sine_table[1023 - (phase & 1023)];
    }
    if (phase < 3 * SINE_QUARTER) {
        return -sound_sine_table[phase & 1023];
    }
    return -sound_sine_table[1023 - (phase & 1023)];
}

/* Binary search of count elements of size bytes from base, sorted by compare (key first); returns the match or
 * 0. */
void *sound_bsearch(const void *key, const void *base, int count, int size,
                  int (*compare)(const void *, const void *))
{
    int low;
    int high;
    int middle;
    int result;
    void *element;
    if (count != 0) {
        low = 1;
        high = count;
        do {
            middle = (low + high) >> 1;
            element = (void *)((const unsigned char *)base + size * (middle - 1));
            result = compare(key, element);
            if (result == 0) {
                return element;
            }
            if (result < 0) {
                high = middle - 1;
            } else {
                low = middle + 1;
            }
        } while (high >= low);
    }
    return 0;
}

/* Milliseconds to the library's time unit (1/256 ms), in place. */
void sound_ms_to_time(unsigned int *time)
{
    *time *= 256U;
}

/* Ticks of a sequence's tempo (sound_synth_get_tick_rate of state) to the library's time unit, in place. */
void sound_ticks_to_time(u32 *value, unsigned char *state)
{
    u32 rate;
    rate = sound_synth_get_tick_rate(state);
    *value = (((*value << 16) / rate) * 1000U) >> 5;
}

/* The library's time unit to milliseconds. */
unsigned int sound_time_to_ms(unsigned int time)
{
    return time / 256U;
}
