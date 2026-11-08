/* GENERATED ROM-aligned TU — segment 0x21210 (rom/lib_21210)
 * layout map 7db48fae6e80338f4674ce14cd1283cee2dbc149f9f1dacaebf5704c974b2257.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-11 — sound_midi_set_controller
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_midi_controllers[][16][134];
/* On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 9): an effect voice is its own channel in set 255, so its 134 controllers
 * and its two bytes here are a voice's. */
#define sound_voice_controllers (*(unsigned char (*)[PC_MX_VOICE_SLOTS][134])pc_mx_fx_ctrl)
#define sound_voice_last_keys (*(unsigned char (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_chan_a)
#define sound_voice_portamento (*(unsigned char (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_chan_b)
void sound_midi_set_controller(unsigned char controller, unsigned char channel,
                   unsigned char set, unsigned char value)
{
    if (set != 255) {
        sound_midi_controllers[set][channel][controller] = value & 127;
    } else {
        sound_voice_controllers[channel][controller] = value & 127;
    }
}

/* PROMOTED 2026-10-11 — sound_midi_set_controller14
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_midi_set_controller(u8 controller, u8 channel, u8 set, u8 value);
void sound_midi_set_controller14(u8 controller, u8 channel, u8 set, u16 value)
{
    if (controller < 64) {
        sound_midi_set_controller(controller & 31, channel, set, value >> 7);
        sound_midi_set_controller((controller & 31) + 32, channel, set, value & 127);
    } else if (controller == 128 || controller == 129) {
        sound_midi_set_controller(controller & 254, channel, set, value >> 7);
        sound_midi_set_controller((controller & 254) + 1, channel, set, value & 127);
    } else if (controller == 132 || controller == 133) {
        sound_midi_set_controller(controller & 254, channel, set, value >> 7);
        sound_midi_set_controller((controller & 254) + 1, channel, set, value & 127);
    } else {
        sound_midi_set_controller(controller, channel, set, value >> 7);
    }
}

/* PROMOTED 2026-10-04 — sound_midi_reset_channel
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_midi_controllers[][16][134];
extern void sound_midi_set_controller(u8, u8, u8, u8);
extern void sound_midi_set_last_key(u8, u8, u8);
extern void sound_midi_set_portamento_state(u8, u8, u8);
void sound_midi_reset_channel(u8 channel, u8 set)
{
    u32 i;
    if (set != 255) {
        for (i = 0; i < 134; i++) {
            sound_midi_controllers[set][channel][i] = 0;
        }
    } else {
        for (i = 0; i < 134; i++) {
            sound_voice_controllers[channel][i] = 0;
        }
    }
    sound_midi_set_controller(7, channel, set, 127);
    sound_midi_set_controller(10, channel, set, 64);
    sound_midi_set_controller(128, channel, set, 64);
    sound_midi_set_controller(129, channel, set, 0);
    sound_midi_set_controller(64, channel, set, 0);
    sound_midi_set_controller(65, channel, set, 0);
    sound_midi_set_controller(91, channel, set, 0);
    sound_midi_set_controller(131, channel, set, 0);
    sound_midi_set_controller(132, channel, set, 64);
    sound_midi_set_controller(133, channel, set, 0);
    sound_midi_set_last_key(channel, set, 255);
    sound_midi_set_portamento_state(channel, set, 0);
}

/* PROMOTED 2026-10-11 — sound_midi_get_controller14
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u16 sound_midi_get_controller14(u8 controller, u8 channel, u8 set)
{
    u8 *pair;
    if (set != 255) {
        if (controller < 64) {
            pair = &sound_midi_controllers[set][channel][controller & 31];
            return (pair[0] << 7) | pair[32];
        } else if (controller == 128 || controller == 129) {
            pair = &sound_midi_controllers[set][channel][controller & 254];
            return (pair[0] << 7) | pair[1];
        } else if (controller == 132 || controller == 133) {
            pair = &sound_midi_controllers[set][channel][controller & 254];
            return (pair[0] << 7) | pair[1];
        } else if (controller < 70) {
            return sound_midi_controllers[set][channel][controller] < 64 ? 0 : 16383;
        } else if (controller >= 96 && controller <= 101) {
            return 0;
        } else {
            return sound_midi_controllers[set][channel][controller] << 7;
        }
    } else {
        if (controller < 64) {
            pair = &sound_voice_controllers[channel][controller & 31];
            return (pair[0] << 7) | pair[32];
        } else if (controller == 128 || controller == 129) {
            pair = &sound_voice_controllers[channel][controller & 254];
            return (pair[0] << 7) | pair[1];
        } else if (controller == 132 || controller == 133) {
            pair = &sound_voice_controllers[channel][controller & 254];
            return (pair[0] << 7) | pair[1];
        } else if (controller < 70) {
            return sound_voice_controllers[channel][controller] < 64 ? 0 : 16383;
        } else if (controller >= 96 && controller <= 101) {
            return 0;
        } else {
            return sound_voice_controllers[channel][controller] << 7;
        }
    }
}

/* PROMOTED 2026-10-11 — sound_midi_copy_voice_controller
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct VoicePrefix { u8 unknown00[96]; u32 identifier60; } VoicePrefix;
#pragma pack()
void sound_midi_copy_voice_controller(u8 controller, VoicePrefix *destination, VoicePrefix *source)
{
    u32 dst;
    u32 src;
    u32 index;
    u8 *dstdata;
    u8 *srcdata;
    dst = destination->identifier60 & 255;
    src = source->identifier60 & 255;
    if (controller < 64) {
        index = controller & 31;
        dstdata = &sound_voice_controllers[dst][index];
        srcdata = &sound_voice_controllers[src][index];
        dstdata[0] = srcdata[0];
        dstdata[32] = srcdata[32];
    } else if (controller == 128 || controller == 129) {
        index = controller & 254;
        dstdata = &sound_voice_controllers[dst][index];
        srcdata = &sound_voice_controllers[src][index];
        dstdata[0] = srcdata[0];
        dstdata[1] = srcdata[1];
    } else if (controller == 132 || controller == 133) {
        index = controller & 254;
        dstdata = &sound_voice_controllers[dst][index];
        srcdata = &sound_voice_controllers[src][index];
        dstdata[0] = srcdata[0];
        dstdata[1] = srcdata[1];
    } else {
        sound_voice_controllers[dst][controller] = sound_voice_controllers[src][controller];
    }
}

/* PROMOTED 2026-10-11 — sound_midi_set_last_key
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_midi_last_keys[][16];
void sound_midi_set_last_key(unsigned char channel, unsigned char set,
                   unsigned char value)
{
    if (set != 255) {
        sound_midi_last_keys[set][channel] = value;
    } else {
        sound_voice_last_keys[channel] = value;
    }
}

/* PROMOTED 2026-10-11 — sound_midi_get_last_key
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_midi_get_last_key(unsigned char channel, unsigned char set)
{
    if (set != 255) {
        return sound_midi_last_keys[set][channel];
    } else {
        return sound_voice_last_keys[channel];
    }
}

/* PROMOTED 2026-10-11 — sound_midi_set_portamento_state
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_midi_portamento[][16];
void sound_midi_set_portamento_state(unsigned char channel, unsigned char set,
                   unsigned char value)
{
    if (set != 255) {
        sound_midi_portamento[set][channel] = value;
    } else {
        sound_voice_portamento[channel] = value;
    }
}

/* PROMOTED 2026-10-11 — sound_midi_get_portamento_state
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_midi_get_portamento_state(unsigned char channel, unsigned char set)
{
    if (set != 255) {
        return sound_midi_portamento[set][channel];
    } else {
        return sound_voice_portamento[channel];
    }
}

/* PROMOTED 2026-10-04 — sound_synth_init_controller_inputs
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct ControlSource { u8 controller; u8 combine; u16 scale; } ControlSource;
typedef struct ControlInput { ControlSource source[4]; u8 count; u8 unknown11; } ControlInput;
typedef struct VoiceControls { u8 unknown00[196]; ControlInput input[9]; } VoiceControls;
#pragma pack(0)
void sound_synth_init_controller_inputs(VoiceControls *voice)
{
    voice->input[0].source[0].controller = 7;
    voice->input[0].source[0].combine = 0;
    voice->input[0].source[0].scale = 256;
    voice->input[0].count = 1;
    voice->input[1].source[0].controller = 10;
    voice->input[1].source[0].combine = 0;
    voice->input[1].source[0].scale = 256;
    voice->input[1].count = 1;
    voice->input[2].source[0].controller = 131;
    voice->input[2].source[0].combine = 0;
    voice->input[2].source[0].scale = 256;
    voice->input[2].count = 1;
    voice->input[3].source[0].controller = 128;
    voice->input[3].source[0].combine = 0;
    voice->input[3].source[0].scale = 256;
    voice->input[3].count = 1;
    voice->input[5].source[0].controller = 1;
    voice->input[5].source[0].combine = 0;
    voice->input[5].source[0].scale = 256;
    voice->input[5].count = 1;
    voice->input[6].source[0].controller = 64;
    voice->input[6].source[0].combine = 0;
    voice->input[6].source[0].scale = 256;
    voice->input[6].count = 1;
    voice->input[7].source[0].controller = 65;
    voice->input[7].source[0].combine = 0;
    voice->input[7].source[0].scale = 256;
    voice->input[7].count = 1;
    voice->input[8].source[0].controller = 91;
    voice->input[8].source[0].combine = 0;
    voice->input[8].source[0].scale = 256;
    voice->input[8].count = 1;
    voice->input[4].source[0].controller = 132;
    voice->input[4].source[0].combine = 0;
    voice->input[4].source[0].scale = 256;
    voice->input[4].count = 1;
}

/* PROMOTED 2026-10-11 — sound_synth_read_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
#pragma pack(1)
typedef struct SynthVoiceInputView { u8 unknown00[36]; u32 flags24; u8 unknown28[34]; u8 channel4A; u8 set4B; u8 unknown4C[20]; u32 id60; u8 unknown64[268]; s16 lfo170; u8 unknown172[10]; s16 lfo17C; u8 unknown17E[34]; } SynthVoiceInputView;
#pragma pack(0)
extern u16 sound_midi_get_controller14(u8 controller, u8 channel, u8 set);
u16 sound_synth_read_input(void *voice, void *control)
{
    u32 i;
    u32 value;
    u8 controller;
    s32 tmp;
    s32 combined;

    for (value = 0, i = 0; i < ((ControlInput *)control)->count; ++i) {
        controller = ((ControlInput *)control)->source[i].controller;
        if (controller == 128 || controller == 1 || controller == 10 ||
            controller == 160 || controller == 161 || controller == 131 ||
            controller == 132) {
            switch (controller) {
            case 160:
                tmp = ((SynthVoiceInputView *)voice)->lfo170 * 2;
                break;
            case 161:
                tmp = ((SynthVoiceInputView *)voice)->lfo17C * 2;
                break;
            default:
                tmp = sound_midi_get_controller14(controller, ((SynthVoiceInputView *)voice)->channel4A, ((SynthVoiceInputView *)voice)->set4B) - 8192;
                break;
            }
            tmp = (s32)((s16)((ControlInput *)control)->source[i].scale * tmp) >> 8;
            tmp = tmp < -8192 ? -8192 : tmp > 8191 ? 8191 : tmp;
            switch (((ControlInput *)control)->source[i].combine) {
            case 0:
                combined = tmp;
                break;
            case 1:
                combined = value - 8192 + tmp;
                combined = combined < -8192 ? -8192 : combined > 8191 ? 8191 : combined;
                break;
            case 2:
                combined = (s32)((value - 8192) * tmp) >> 13;
                combined = combined < -8192 ? -8192 : combined > 8191 ? 8191 : combined;
                break;
            }
            value = combined + 8192;
        } else {
            tmp = sound_midi_get_controller14(controller, ((SynthVoiceInputView *)voice)->channel4A, ((SynthVoiceInputView *)voice)->set4B);
            tmp = (s32)((s16)((ControlInput *)control)->source[i].scale * tmp) >> 8;
            if (tmp > 16383) tmp = 16383;
            switch (((ControlInput *)control)->source[i].combine) {
            case 0:
                value = tmp;
                break;
            case 1:
                value += tmp;
                value = value > 16383 ? 16383 : value;
                break;
            case 2:
                value = (value * tmp) >> 14;
                value = value > 16383 ? 16383 : value;
                break;
            }
        }
    }
    return value;
}

/* PROMOTED 2026-10-04 — sound_synth_get_volume_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned short sound_synth_read_input(void *voice, void *control);
unsigned short sound_synth_get_volume_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 196);
}

/* PROMOTED 2026-10-04 — sound_synth_get_pan_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_pan_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 214);
}

/* PROMOTED 2026-10-04 — sound_synth_get_span_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_span_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 232);
}

/* PROMOTED 2026-10-04 — sound_synth_get_pitch_bend_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_pitch_bend_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 250);
}

/* PROMOTED 2026-10-04 — sound_synth_get_doppler_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_doppler_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 268);
}

/* PROMOTED 2026-10-04 — sound_synth_get_modulation_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_modulation_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 286);
}

/* PROMOTED 2026-10-04 — sound_synth_get_pedal_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_pedal_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 304);
}

/* PROMOTED 2026-10-04 — sound_synth_get_portamento_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_portamento_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 322);
}

/* PROMOTED 2026-10-04 — sound_synth_get_reverb_input
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_synth_get_reverb_input(void *voice)
{
    return sound_synth_read_input(voice, (unsigned char *)voice + 340);
}

/* PROMOTED 2026-10-11 — sound_macro_map_controller
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_map_controller(u8 controller)
{
    switch (controller) {
    case 0x80: controller = 0x80; break;
    case 0x81: controller = 0x82; break;
    case 0x82: controller = 0xA0; break;
    case 0x83: controller = 0xA1; break;
    case 0x84: controller = 0x83; break;
    case 0x85: controller = 0x84; break;
    }
    return controller;
}

/* PROMOTED 2026-10-11 — sound_macro_get_controller
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct VoiceState { u8 unknown00[36]; u32 flags24; u8 unknown28[34]; u8 channel4A; u8 set4B; u8 unknown4C[20]; u32 id60; u8 unknown64[268]; s16 lfo170; u8 unknown172[10]; s16 lfo17C; u8 unknown17E[34]; } VoiceState;
#pragma pack(0)
extern u8 sound_macro_map_controller(u8 controller);
extern u16 sound_midi_get_controller14(u8 controller, u8 channel, u8 set);
u16 sound_macro_get_controller(VoiceState *voice, u8 controller)
{
    u16 value;
    switch (sound_macro_map_controller(controller)) {
    case 160:
        value = voice->lfo170 * 2 + 8192;
        break;
    case 161:
        value = voice->lfo17C * 2 + 8192;
        break;
    default:
        value = voice->channel4A != 255
            ? sound_midi_get_controller14(controller, voice->channel4A, voice->set4B) : 0;
        break;
    }
    return value;
}

/* PROMOTED 2026-10-11 — sound_macro_set_controller
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern void sound_midi_set_controller14(u8 controller, u8 channel, u8 set, u16 value);
void sound_macro_set_controller(VoiceState *voice, u8 controller, s16 value)
{
    value = value < 0 ? 0 : value > 16383 ? 16383 : value;
    switch (sound_macro_map_controller(controller)) {
    case 160:
    case 161:
        break;
    default:
        if (voice->channel4A != 255) {
            sound_midi_set_controller14(controller, voice->channel4A, voice->set4B, value);
        }
        break;
    }
}

