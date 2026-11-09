/* src/rom/sound/gains.c: the MusyX sound library's volume, pan and pitch arithmetic: a voice's channel gains from
 * its volume, pan, surround span and aux send (for the hardware layer's sound_hw_set_volume), and pitches from notes
 * and semitones (for the synth). MusyX is the N64's sound library: no MAT3 counterpart. */
#include "../rom_tu.h"
#include "types.h"

/* The 16.16 inputs: 127.0 is the most, a pan of 128.0 (0x800000) the special "both sides, out of phase" */
#define LEVEL_MAX 0x7F0000U
#define PAN_SURROUND 0x800000U
#define MINUS_3DB 0.7079f
/* A note-to-pitch value: the key the sample plays at its own rate (high byte) and that rate in Hz (low 24 bits);
 * all ones means "key 64 at 22,050 Hz". */
#define DEFAULT_SAMPLE_PITCH 0x40005622U

extern const float sound_volume_curve[129];
extern const float sound_pan_curve[3];
extern const float sound_semitone_ratio;
extern const float sound_semitone_up_ratios[];
extern const float sound_semitone_down_ratios[];
extern int sound_synth_frequency;

/* A voice's four gains (Q15) from its volume, pan, span and aux send, all 16.16 up to 127.0. The volume goes through
 * sound_volume_curve (129 points, interpolated); the span splits it between the front pair and the surround output
 * (*span_output, at -3 dB) by sound_pan_curve; the pan then splits the front part between *left and *right by the
 * same curve, the second from 128.0 - pan; a pan of 128.0 instead puts it at -3 dB on both, the second inverted. The
 * aux send goes through the volume curve into *aux_output at -3 dB. hardware.c stores *left in the voice's second
 * target and *right in its first (the SFD's left channel). */
void sound_compute_gains(u16 *left, u16 *right, u32 volume, u32 pan,
                   u32 span, u16 *span_output, u32 aux, u16 *aux_output)
{
    float fraction;
    float gain;
    float factor;
    const float *entry;

    if (volume > LEVEL_MAX) {
        volume = LEVEL_MAX;
    }
    entry = &sound_volume_curve[volume >> 16];
    gain = (1.0f - (float)(volume & 0xFFFFU) * (1.0f / 65536.0f)) * entry[0] +
           entry[1] * ((float)(volume & 0xFFFFU) * (1.0f / 65536.0f));
    fraction = (float)(span & 0x3FFFFFU) * (1.0f / 4194304.0f);
    entry = &sound_pan_curve[span >> 22];
    factor = (1.0f - fraction) * entry[0] +
           entry[1] * fraction;
    *span_output = (int)(gain * factor * MINUS_3DB * 32767.0f);
    span = PAN_SURROUND - span;
    if (span >= PAN_SURROUND) {
        span = LEVEL_MAX;
    }
    entry = &sound_pan_curve[span >> 22];
    factor = (1.0f - (float)(span & 0x3FFFFFU) * (1.0f / 4194304.0f)) * entry[0] +
           entry[1] * ((float)(span & 0x3FFFFFU) * (1.0f / 4194304.0f));
    gain *= factor;
    if (pan == PAN_SURROUND) {
        *left = (int)(gain * MINUS_3DB * 32767.0f);
        *right = (int)(gain * -MINUS_3DB * 32767.0f);
    } else {
        entry = &sound_pan_curve[pan >> 22];
        factor = (1.0f - (float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f)) * entry[0] +
               entry[1] * ((float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f));
        *left = (int)(gain * factor * 32767.0f);
        pan = PAN_SURROUND - pan;
        if (pan >= PAN_SURROUND) {
            pan = LEVEL_MAX;
        }
        entry = &sound_pan_curve[pan >> 22];
        factor = (1.0f - (float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f)) * entry[0] +
               entry[1] * ((float)(pan & 0x3FFFFFU) * (1.0f / 4194304.0f));
        *right = (int)(gain * factor * 32767.0f);
    }
    if (aux > LEVEL_MAX) {
        aux = LEVEL_MAX;
    }
    entry = &sound_volume_curve[aux >> 16];
    gain = (1.0f - (float)(aux & 0xFFFFU) * (1.0f / 65536.0f)) * entry[0] +
           entry[1] * ((float)(aux & 0xFFFFU) * (1.0f / 65536.0f));
    *aux_output = (int)(gain * MINUS_3DB * 32767.0f);
}

/* value one semitone up (times sound_semitone_ratio). */
u16 sound_pitch_up_semitone(u16 value)
{
    return (u16)(u32)((float)value * sound_semitone_ratio);
}

/* The rate (4.12 fixed point at the synth's output rate) that plays a sample at key note: encoded_value has the
 * sample's own key and rate (DEFAULT_SAMPLE_PITCH when all ones), and each semitone between them is a ratio from
 * the tables. */
u32 sound_note_to_pitch(u8 note, u32 encoded_value)
{
    u8 reference;
    float value;
    if (encoded_value == 0xFFFFFFFFU) {
        encoded_value = DEFAULT_SAMPLE_PITCH;
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

/* value truncated to a whole number (through a 32-bit unsigned integer, so for 0 to 4 294 967 295 only). */
double sound_truncate(double value)
{
    return (unsigned int)value;
}
