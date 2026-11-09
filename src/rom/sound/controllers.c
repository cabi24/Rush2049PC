/* src/rom/sound/controllers.c: the MusyX sound library's MIDI controllers and a voice's controller inputs.
 *
 * Each song player has 16 MIDI channels with 134 controllers each (sound_midi_controllers[player][channel]), the
 * last key played on each and its portamento state; an effect voice is its own channel in the set VOICE_OWN_SET,
 * with its own controllers (sound_voice_controllers, by voice). The sequencer (sequencer.c), the API (api.c) and
 * the macros (macros.c) set them. Each synth voice mixes up to four controllers into each of its nine inputs
 * (volume, pan, span, pitch bend, doppler, modulation, pedal, portamento, reverb); the synth tick (synth.c) reads
 * them through sound_synth_get_*_input. The macros read and write controllers as variables
 * (sound_macro_get_controller, sound_macro_set_controller).
 *
 * Part of MusyX, the N64's sound library: no MAT3 counterpart. The voice is read at its N64 offsets (macros.c has
 * the map of its fields). */
#include "../rom_tu.h"

/* Controller numbers: 0..127 are MIDI's, 128 and up the library's own. A controller below 64 is the high half of a
 * 14-bit value whose low half is the controller 32 above; 128/129 and 132/133 are 14-bit pairs too. */
#define CTRL_MODULATION      1
#define CTRL_VOLUME          7
#define CTRL_PAN             10
#define CTRL_PEDAL           64  /* sustain; 64..69 are switches, read as off (0) or on (16383) */
#define CTRL_PORTAMENTO      65
#define CTRL_REVERB          91
#define CTRL_PITCH_BEND      128
#define CTRL_PITCH_BEND_LOW  129
#define CTRL_PRESSURE        130 /* channel pressure (the sequencer's) */
#define CTRL_SPAN            131 /* front-back pan */
#define CTRL_DOPPLER         132
#define CTRL_DOPPLER_LOW     133
#define CTRL_LFO1            160 /* not stored: the voice's first LFO (+0x170) */
#define CTRL_LFO2            161 /* not stored: the voice's second LFO (+0x17C) */
#define MIDI_CONTROLLER_COUNT 134

#define VOICE_OWN_SET        255 /* the controller set of an effect voice: its own, indexed by the voice */
#define NO_CHANNEL           255
#define NO_LAST_KEY          255
#define CONTROL_SCALE_ONE    256 /* an input source's scale, 8.8 fixed point: 1.0 */

/* How an input source combines with the sources before it. */
enum ControlCombine { CONTROL_COMBINE_SET, CONTROL_COMBINE_ADD, CONTROL_COMBINE_MULTIPLY };

/* The voice's controller inputs (+0xC4), in this order; macros.c's SELECT_* steps fill the same records. */
enum SynthInput {
    SYNTH_INPUT_VOLUME, SYNTH_INPUT_PAN, SYNTH_INPUT_SPAN, SYNTH_INPUT_PITCH_BEND, SYNTH_INPUT_DOPPLER,
    SYNTH_INPUT_MODULATION, SYNTH_INPUT_PEDAL, SYNTH_INPUT_PORTAMENTO, SYNTH_INPUT_REVERB, SYNTH_INPUT_COUNT
};

/* An input: up to four sources, each a controller, a combination and a scale. */
#pragma pack(1)
typedef struct ControlSource { u8 controller; u8 combine; u16 scale; } ControlSource;
typedef struct ControlInput { ControlSource source[4]; u8 count; u8 unknown11; } ControlInput;
/* The voice as the inputs see it: the nine inputs at +0xC4. */
typedef struct VoiceControls { u8 unknown00[196]; ControlInput input[SYNTH_INPUT_COUNT]; } VoiceControls;
#pragma pack()

/* The voice as the controller readers see it: its channel and set, its id, its two LFOs' values. */
#pragma pack(1)
typedef struct VoiceState { u8 unknown00[36]; u32 flags24; u8 unknown28[34]; u8 channel4A; u8 set4B; u8 unknown4C[20]; u32 id60; u8 unknown64[268]; s16 lfo170; u8 unknown172[10]; s16 lfo17C; u8 unknown17E[34]; } VoiceState;
#pragma pack(0)

/* The voice as sound_midi_copy_voice_controller sees it: its id. */
#pragma pack(1)
typedef struct VoicePrefix { u8 unknown00[96]; u32 id60; } VoicePrefix;
#pragma pack()

/* The song players' channel state (library data) ... */
extern u8 sound_midi_controllers[][16][MIDI_CONTROLLER_COUNT];
extern unsigned char sound_midi_last_keys[][16];
extern unsigned char sound_midi_portamento[][16];
/* ... and an effect voice's. On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no
 * N64 address (pc_game_types.h, docs/port/DECOUPLE.md 9): an effect voice is its own channel in set 255, so its 134
 * controllers and its two bytes here are a voice's. */
#define sound_voice_controllers (*(unsigned char (*)[PC_MX_VOICE_SLOTS][134])pc_mx_fx_ctrl)
#define sound_voice_last_keys (*(unsigned char (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_chan_a)
#define sound_voice_portamento (*(unsigned char (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_chan_b)

/* This file, used before its definition. */
extern void sound_midi_set_last_key(u8, u8, u8);
extern void sound_midi_set_portamento_state(u8, u8, u8);

/* Sets one 7-bit controller of a channel: of the song player `set`, or of the effect voice `channel` when set is
 * VOICE_OWN_SET. */
void sound_midi_set_controller(unsigned char controller, unsigned char channel,
                   unsigned char set, unsigned char value)
{
    if (set != VOICE_OWN_SET) {
        sound_midi_controllers[set][channel][controller] = value & 127;
    } else {
        sound_voice_controllers[channel][controller] = value & 127;
    }
}

/* Sets a controller from a 14-bit value: both halves of a pair (a controller below 64 and the one 32 above, or
 * 128/129, 132/133), else the high seven bits alone. */
void sound_midi_set_controller14(u8 controller, u8 channel, u8 set, u16 value)
{
    if (controller < 64) {
        sound_midi_set_controller(controller & 31, channel, set, value >> 7);
        sound_midi_set_controller((controller & 31) + 32, channel, set, value & 127);
    } else if (controller == CTRL_PITCH_BEND || controller == CTRL_PITCH_BEND_LOW) {
        sound_midi_set_controller(controller & 254, channel, set, value >> 7);
        sound_midi_set_controller((controller & 254) + 1, channel, set, value & 127);
    } else if (controller == CTRL_DOPPLER || controller == CTRL_DOPPLER_LOW) {
        sound_midi_set_controller(controller & 254, channel, set, value >> 7);
        sound_midi_set_controller((controller & 254) + 1, channel, set, value & 127);
    } else {
        sound_midi_set_controller(controller, channel, set, value >> 7);
    }
}

/* Resets a channel: every controller to 0, then full volume, centred pan, pitch bend and doppler, no last key, no
 * portamento. */
void sound_midi_reset_channel(u8 channel, u8 set)
{
    u32 i;
    if (set != VOICE_OWN_SET) {
        for (i = 0; i < MIDI_CONTROLLER_COUNT; i++) {
            sound_midi_controllers[set][channel][i] = 0;
        }
    } else {
        for (i = 0; i < MIDI_CONTROLLER_COUNT; i++) {
            sound_voice_controllers[channel][i] = 0;
        }
    }
    sound_midi_set_controller(CTRL_VOLUME, channel, set, 127);
    sound_midi_set_controller(CTRL_PAN, channel, set, 64);
    sound_midi_set_controller(CTRL_PITCH_BEND, channel, set, 64);
    sound_midi_set_controller(CTRL_PITCH_BEND_LOW, channel, set, 0);
    sound_midi_set_controller(CTRL_PEDAL, channel, set, 0);
    sound_midi_set_controller(CTRL_PORTAMENTO, channel, set, 0);
    sound_midi_set_controller(CTRL_REVERB, channel, set, 0);
    sound_midi_set_controller(CTRL_SPAN, channel, set, 0);
    sound_midi_set_controller(CTRL_DOPPLER, channel, set, 64);
    sound_midi_set_controller(CTRL_DOPPLER_LOW, channel, set, 0);
    sound_midi_set_last_key(channel, set, NO_LAST_KEY);
    sound_midi_set_portamento_state(channel, set, 0);
}

/* A controller as a 14-bit value (0..16383): a pair's two halves, a switch (64..69) as off or on, the data-entry and
 * parameter-number controllers (96..101) as 0, any other the 7-bit value shifted up. */
u16 sound_midi_get_controller14(u8 controller, u8 channel, u8 set)
{
    u8 *pair;
    if (set != VOICE_OWN_SET) {
        if (controller < 64) {
            pair = &sound_midi_controllers[set][channel][controller & 31];
            return (pair[0] << 7) | pair[32];
        } else if (controller == CTRL_PITCH_BEND || controller == CTRL_PITCH_BEND_LOW) {
            pair = &sound_midi_controllers[set][channel][controller & 254];
            return (pair[0] << 7) | pair[1];
        } else if (controller == CTRL_DOPPLER || controller == CTRL_DOPPLER_LOW) {
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
        } else if (controller == CTRL_PITCH_BEND || controller == CTRL_PITCH_BEND_LOW) {
            pair = &sound_voice_controllers[channel][controller & 254];
            return (pair[0] << 7) | pair[1];
        } else if (controller == CTRL_DOPPLER || controller == CTRL_DOPPLER_LOW) {
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

/* Copies one controller (both halves of a 14-bit pair) from one effect voice's own set to another's: a PLAYMACRO
 * child of an effect voice starts with its parent's controllers (sound_synth_copy_controllers, synth.c). */
void sound_midi_copy_voice_controller(u8 controller, VoicePrefix *destination, VoicePrefix *source)
{
    u32 dst;
    u32 src;
    u32 index;
    u8 *dstdata;
    u8 *srcdata;
    dst = destination->id60 & 255;
    src = source->id60 & 255;
    if (controller < 64) {
        index = controller & 31;
        dstdata = &sound_voice_controllers[dst][index];
        srcdata = &sound_voice_controllers[src][index];
        dstdata[0] = srcdata[0];
        dstdata[32] = srcdata[32];
    } else if (controller == CTRL_PITCH_BEND || controller == CTRL_PITCH_BEND_LOW) {
        index = controller & 254;
        dstdata = &sound_voice_controllers[dst][index];
        srcdata = &sound_voice_controllers[src][index];
        dstdata[0] = srcdata[0];
        dstdata[1] = srcdata[1];
    } else if (controller == CTRL_DOPPLER || controller == CTRL_DOPPLER_LOW) {
        index = controller & 254;
        dstdata = &sound_voice_controllers[dst][index];
        srcdata = &sound_voice_controllers[src][index];
        dstdata[0] = srcdata[0];
        dstdata[1] = srcdata[1];
    } else {
        sound_voice_controllers[dst][controller] = sound_voice_controllers[src][controller];
    }
}

/* Records the last key played on a channel (NO_LAST_KEY: none); LAST_NOTE steps start from it. */
void sound_midi_set_last_key(unsigned char channel, unsigned char set,
                   unsigned char key)
{
    if (set != VOICE_OWN_SET) {
        sound_midi_last_keys[set][channel] = key;
    } else {
        sound_voice_last_keys[channel] = key;
    }
}

/* The last key played on a channel, NO_LAST_KEY if none. */
unsigned char sound_midi_get_last_key(unsigned char channel, unsigned char set)
{
    if (set != VOICE_OWN_SET) {
        return sound_midi_last_keys[set][channel];
    } else {
        return sound_voice_last_keys[channel];
    }
}

/* Records a channel's portamento state (0: off). */
void sound_midi_set_portamento_state(unsigned char channel, unsigned char set,
                   unsigned char state)
{
    if (set != VOICE_OWN_SET) {
        sound_midi_portamento[set][channel] = state;
    } else {
        sound_voice_portamento[channel] = state;
    }
}

/* A channel's portamento state (0: off). */
unsigned char sound_midi_get_portamento_state(unsigned char channel, unsigned char set)
{
    if (set != VOICE_OWN_SET) {
        return sound_midi_portamento[set][channel];
    } else {
        return sound_voice_portamento[channel];
    }
}

/* Gives a voice's nine inputs their default mixes: each one controller at scale 1.0 (volume, pan, span, pitch bend,
 * doppler, modulation, pedal, portamento, reverb). The first run of a macro calls it (sound_macro_run); SELECT_*
 * steps then replace a mix. */
void sound_synth_init_controller_inputs(VoiceControls *voice)
{
    voice->input[SYNTH_INPUT_VOLUME].source[0].controller = CTRL_VOLUME;
    voice->input[SYNTH_INPUT_VOLUME].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_VOLUME].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_VOLUME].count = 1;
    voice->input[SYNTH_INPUT_PAN].source[0].controller = CTRL_PAN;
    voice->input[SYNTH_INPUT_PAN].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_PAN].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_PAN].count = 1;
    voice->input[SYNTH_INPUT_SPAN].source[0].controller = CTRL_SPAN;
    voice->input[SYNTH_INPUT_SPAN].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_SPAN].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_SPAN].count = 1;
    voice->input[SYNTH_INPUT_PITCH_BEND].source[0].controller = CTRL_PITCH_BEND;
    voice->input[SYNTH_INPUT_PITCH_BEND].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_PITCH_BEND].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_PITCH_BEND].count = 1;
    voice->input[SYNTH_INPUT_MODULATION].source[0].controller = CTRL_MODULATION;
    voice->input[SYNTH_INPUT_MODULATION].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_MODULATION].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_MODULATION].count = 1;
    voice->input[SYNTH_INPUT_PEDAL].source[0].controller = CTRL_PEDAL;
    voice->input[SYNTH_INPUT_PEDAL].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_PEDAL].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_PEDAL].count = 1;
    voice->input[SYNTH_INPUT_PORTAMENTO].source[0].controller = CTRL_PORTAMENTO;
    voice->input[SYNTH_INPUT_PORTAMENTO].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_PORTAMENTO].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_PORTAMENTO].count = 1;
    voice->input[SYNTH_INPUT_REVERB].source[0].controller = CTRL_REVERB;
    voice->input[SYNTH_INPUT_REVERB].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_REVERB].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_REVERB].count = 1;
    voice->input[SYNTH_INPUT_DOPPLER].source[0].controller = CTRL_DOPPLER;
    voice->input[SYNTH_INPUT_DOPPLER].source[0].combine = CONTROL_COMBINE_SET;
    voice->input[SYNTH_INPUT_DOPPLER].source[0].scale = CONTROL_SCALE_ONE;
    voice->input[SYNTH_INPUT_DOPPLER].count = 1;
}

/* Mixes one of a voice's inputs (`control`, a ControlInput of the voice) into a 14-bit value. Each source reads its
 * controller from the voice's channel (or one of the voice's LFOs), scales it (8.8) and sets, adds to or multiplies
 * the value so far. Pitch bend, modulation, pan, span, doppler and the LFOs are taken as centred (-8192..8191 around
 * 8192), the others as 0..16383. */
u16 sound_synth_read_input(void *voice, void *control)
{
    u32 i;
    u32 value;
    u8 controller;
    s32 scaled;
    s32 combined;

    for (value = 0, i = 0; i < ((ControlInput *)control)->count; ++i) {
        controller = ((ControlInput *)control)->source[i].controller;
        if (controller == CTRL_PITCH_BEND || controller == CTRL_MODULATION || controller == CTRL_PAN ||
            controller == CTRL_LFO1 || controller == CTRL_LFO2 || controller == CTRL_SPAN ||
            controller == CTRL_DOPPLER) {
            switch (controller) {
            case CTRL_LFO1:
                scaled = ((VoiceState *)voice)->lfo170 * 2;
                break;
            case CTRL_LFO2:
                scaled = ((VoiceState *)voice)->lfo17C * 2;
                break;
            default:
                scaled = sound_midi_get_controller14(controller, ((VoiceState *)voice)->channel4A, ((VoiceState *)voice)->set4B) - 8192;
                break;
            }
            scaled = (s32)((s16)((ControlInput *)control)->source[i].scale * scaled) >> 8;
            scaled = scaled < -8192 ? -8192 : scaled > 8191 ? 8191 : scaled;
            switch (((ControlInput *)control)->source[i].combine) {
            case CONTROL_COMBINE_SET:
                combined = scaled;
                break;
            case CONTROL_COMBINE_ADD:
                combined = value - 8192 + scaled;
                combined = combined < -8192 ? -8192 : combined > 8191 ? 8191 : combined;
                break;
            case CONTROL_COMBINE_MULTIPLY:
                combined = (s32)((value - 8192) * scaled) >> 13;
                combined = combined < -8192 ? -8192 : combined > 8191 ? 8191 : combined;
                break;
            }
            value = combined + 8192;
        } else {
            scaled = sound_midi_get_controller14(controller, ((VoiceState *)voice)->channel4A, ((VoiceState *)voice)->set4B);
            scaled = (s32)((s16)((ControlInput *)control)->source[i].scale * scaled) >> 8;
            if (scaled > 16383) scaled = 16383;
            switch (((ControlInput *)control)->source[i].combine) {
            case CONTROL_COMBINE_SET:
                value = scaled;
                break;
            case CONTROL_COMBINE_ADD:
                value += scaled;
                value = value > 16383 ? 16383 : value;
                break;
            case CONTROL_COMBINE_MULTIPLY:
                value = (value * scaled) >> 14;
                value = value > 16383 ? 16383 : value;
                break;
            }
        }
    }
    return value;
}

/* A voice's inputs, mixed (sound_synth_read_input): each returns 0..16383; the centred ones are 8192 at rest. */

/* The volume input (default: controller 7). */
unsigned short sound_synth_get_volume_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_VOLUME]);
}

/* The pan input (default: controller 10). */
unsigned short sound_synth_get_pan_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_PAN]);
}

/* The span (front-back) input (default: controller 131). */
unsigned short sound_synth_get_span_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_SPAN]);
}

/* The pitch-bend input (default: controllers 128/129). */
unsigned short sound_synth_get_pitch_bend_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_PITCH_BEND]);
}

/* The doppler input (default: controllers 132/133; 8192 is no shift). */
unsigned short sound_synth_get_doppler_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_DOPPLER]);
}

/* The modulation input (default: controller 1). */
unsigned short sound_synth_get_modulation_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_MODULATION]);
}

/* The sustain-pedal input (default: controller 64). */
unsigned short sound_synth_get_pedal_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_PEDAL]);
}

/* The portamento input (default: controller 65). */
unsigned short sound_synth_get_portamento_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_PORTAMENTO]);
}

/* The reverb-send input (default: controller 91). */
unsigned short sound_synth_get_reverb_input(void *voice)
{
    return sound_synth_read_input(voice, &((VoiceControls *)voice)->input[SYNTH_INPUT_REVERB]);
}

/* Maps a controller number as macros write it (0x80 and up) to the library's: 0x80 pitch bend, 0x81 pressure,
 * 0x82 and 0x83 the two LFOs, 0x84 span, 0x85 doppler. Others are MIDI's own and pass through. */
u8 sound_macro_map_controller(u8 controller)
{
    switch (controller) {
    case 0x80: controller = CTRL_PITCH_BEND; break;
    case 0x81: controller = CTRL_PRESSURE; break;
    case 0x82: controller = CTRL_LFO1; break;
    case 0x83: controller = CTRL_LFO2; break;
    case 0x84: controller = CTRL_SPAN; break;
    case 0x85: controller = CTRL_DOPPLER; break;
    }
    return controller;
}

/* A controller as a macro variable reads it (a macro's controller number, 14 bits): an LFO's value, centred at
 * 8192, or the controller of the voice's channel (0 when it has none). */
u16 sound_macro_get_controller(VoiceState *voice, u8 controller)
{
    u16 value;
    switch (sound_macro_map_controller(controller)) {
    case CTRL_LFO1:
        value = voice->lfo170 * 2 + 8192;
        break;
    case CTRL_LFO2:
        value = voice->lfo17C * 2 + 8192;
        break;
    default:
        /* Original game: the controller is read by the macro's number, not the mapped one (which only picks
         * out the LFOs here), so a macro's 0x81 (pressure) reads the pitch bend and 0x84 (span) the doppler.
         * sound_macro_set_controller writes the same way; the inputs (SELECT_*) do use the mapped number. */
        value = voice->channel4A != NO_CHANNEL
            ? sound_midi_get_controller14(controller, voice->channel4A, voice->set4B) : 0;
        break;
    }
    return value;
}

/* A macro's write of a controller variable: clamped to 0..16383 and set on the voice's channel (if it has one).
 * The LFOs can't be written. */
void sound_macro_set_controller(VoiceState *voice, u8 controller, s16 value)
{
    value = value < 0 ? 0 : value > 16383 ? 16383 : value;
    switch (sound_macro_map_controller(controller)) {
    case CTRL_LFO1:
    case CTRL_LFO2:
        break;
    default:
        if (voice->channel4A != NO_CHANNEL) {
            sound_midi_set_controller14(controller, voice->channel4A, voice->set4B, value);
        }
        break;
    }
}
