/* GENERATED ROM-aligned TU — segment 0x1ece0 (rom/lib_1ece0)
 * layout map 9574159704610867be8d0c06826fb45c38860c9806fbcd314befaf1c5d1b3e92.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-11 — sound_compute_gains
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
extern const float sound_volume_curve[129];
extern const float sound_pan_curve[3];
void sound_compute_gains(u16 *left, u16 *right, u32 volume, u32 pan,
                   u32 span, u16 *span_output, u32 aux, u16 *aux_output)
{
    float fraction;
    float gain;
    float factor;
    const float *entry;

    if (volume > 0x7F0000U) {
        volume = 0x7F0000U;
    }
    entry = &sound_volume_curve[volume >> 16];
    gain = (1.0f - (float)(volume & 0xFFFFU) * (1.0f / 65536.0f)) * entry[0] +
           entry[1] * ((float)(volume & 0xFFFFU) * (1.0f / 65536.0f));
    fraction = (float)(span & 0x3FFFFFU) * (1.0f / 4194304.0f);
    entry = &sound_pan_curve[span >> 22];
    factor = (1.0f - fraction) * entry[0] +
           entry[1] * fraction;
    *span_output = (int)(gain * factor * 0.7079f * 32767.0f);
    span = 0x800000U - span;
    if (span >= 0x800000U) {
        span = 0x7F0000U;
    }
    entry = &sound_pan_curve[span >> 22];
    factor = (1.0f - (float)(span & 0x3FFFFFU) * (1.0f / 4194304.0f)) * entry[0] +
           entry[1] * ((float)(span & 0x3FFFFFU) * (1.0f / 4194304.0f));
    gain *= factor;
    if (pan == 0x800000U) {
        *left = (int)(gain * 0.7079f * 32767.0f);
        *right = (int)(gain * -0.7079f * 32767.0f);
    } else {
        entry = &sound_pan_curve[pan >> 22];
        factor = (1.0f - (float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f)) * entry[0] +
               entry[1] * ((float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f));
        *left = (int)(gain * factor * 32767.0f);
        pan = 0x800000U - pan;
        if (pan >= 0x800000U) {
            pan = 0x7F0000U;
        }
        entry = &sound_pan_curve[pan >> 22];
        factor = (1.0f - (float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f)) * entry[0] +
               entry[1] * ((float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f));
        *right = (int)(gain * factor * 32767.0f);
    }
    if (aux > 0x7F0000U) {
        aux = 0x7F0000U;
    }
    entry = &sound_volume_curve[aux >> 16];
    gain = (1.0f - (float)(aux & 0xFFFFU) * (1.0f / 65536.0f)) * entry[0] +
           entry[1] * ((float)(aux & 0xFFFFU) * (1.0f / 65536.0f));
    *aux_output = (int)(gain * 0.7079f * 32767.0f);
}

/* PROMOTED 2026-10-11 — sound_pitch_up_semitone
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern const float sound_semitone_ratio;
u16 sound_pitch_up_semitone(u16 value)
{
    return (u16)(u32)((float)value * sound_semitone_ratio);
}

/* PROMOTED 2026-10-11 — sound_note_to_pitch
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern const float sound_semitone_up_ratios[];
extern const float sound_semitone_down_ratios[];
extern int sound_synth_frequency;
u32 sound_note_to_pitch(u8 note, u32 encoded_value)
{
    u8 reference;
    float value;
    if (encoded_value == 0xFFFFFFFFU) {
        encoded_value = 0x40005622U;
    }
    reference = encoded_value >> 24;
    if (note != reference) {
        value = (reference < note ? sound_semitone_up_ratios[note - reference] : sound_semitone_down_ratios[reference - note]) *
                (float)(encoded_value & 0xFFFFFFU);
    } else {
        value = (float)(encoded_value & 0xFFFFFFU);
    }
    return (u32)((value * 4096.0f) / (float)sound_synth_frequency);
}

/* PROMOTED 2026-10-04 — sound_truncate
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
double sound_truncate(double value)
{
    return (unsigned int)value;
}

