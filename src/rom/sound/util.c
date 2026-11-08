/* GENERATED ROM-aligned TU — segment 0x1f340 (rom/lib_1f340)
 * layout map e00424843f6bc10c711360c058d6ac0994494b0a03b7951830560602027a25aa.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-04 — sound_alloc
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *(*sound_service_alloc)(unsigned int size, unsigned int mode);
void *sound_alloc(unsigned int size)
{
    return sound_service_alloc(size, 0);
}

/* PROMOTED 2026-10-04 — sound_free
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void (*sound_service_free)(void *allocation);
void sound_free(void *allocation)
{
    sound_service_free(allocation);
}

/* PROMOTED 2026-10-11 — sound_random
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_random_seed;
unsigned short sound_random(void)
{
    sound_random_seed *= 2822053219U;
    return sound_random_seed >> 6;
}

/* PROMOTED 2026-10-11 — sound_sine
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern const short sound_sine_table[1024];
extern const short sound_sine_table_end[];
short sound_sine(u16 phase)
{
    phase &= 4095;
    if (phase < 1024) {
        return sound_sine_table[phase];
    }
    if (phase < 2048) {
        return (&sound_sine_table_end[-1023])[1023 - (phase & 1023)];
    }
    if (phase < 3072) {
        return -sound_sine_table[phase & 1023];
    }
    return -(&sound_sine_table_end[-1023])[1023 - (phase & 1023)];
}

/* PROMOTED 2026-10-11 — sound_bsearch
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
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

/* PROMOTED 2026-10-04 — sound_ms_to_time
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_ms_to_time(unsigned int *time)
{
    *time *= 256U;
}

/* PROMOTED 2026-10-04 — sound_ticks_to_time
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u32 sound_synth_get_tick_rate(unsigned char *);
void sound_ticks_to_time(u32 *value, unsigned char *state)
{
    u32 rate;
    rate = sound_synth_get_tick_rate(state);
    *value = (((*value << 16) / rate) * 1000U) >> 5;
}

/* PROMOTED 2026-10-04 — sound_time_to_ms
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned int sound_time_to_ms(unsigned int time)
{
    return time / 256U;
}

