/* GENERATED ROM-aligned TU — segment 0x15930 (rom/lib_15930)
 * layout map de003be105741e6994da52fab58833577c09b8f2e9958c6fed60aca373036350.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-04 — sound_rate_to_pitch
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_synth_frequency;
unsigned int sound_rate_to_pitch(unsigned int rate)
{
    return ((float)rate * 4096.0f) / sound_synth_frequency;
}

/* PROMOTED 2026-10-04 — sound_get_version_text
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_version_text[];
void *sound_get_version_text(void)
{
    return sound_version_text;
}

