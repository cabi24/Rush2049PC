/* src/rom/sound/macros.c: the MusyX sound library's SoundMacro interpreter.
 *
 * Every sound the library plays is a SoundMacro: a small program of 8-byte steps (MacroCommand) in the sound
 * project's pool, which start samples, wait, loop, branch, and set the note, volume, pan, envelopes, sweeps,
 * vibrato, portamento, LFOs and variables. A synth voice runs one: sound_macro_start allocates a voice and points
 * it at a macro, and each library tick the synth (synth.c, sound_synth_update_voices) calls sound_macro_run, which
 * executes steps until one waits or ends; every opcode has a handler here. The synth tick then turns what the
 * handlers set into pitch and volume for the hardware layer (hardware.c). The file also keeps which voice owns a
 * MIDI channel's legato (sound_synth_*_channel).
 *
 * MusyX is the N64's sound library; MAT3, the GameCube build, has its own sound code, so there is no MAT3
 * counterpart. N64-shaped: the voice keeps its N64 layout (below), which every handler reads through its own view
 * with fields named after their N64 offsets, and its macro pointers are 32-bit window addresses (N64P, PC_N64W). */
#include "../rom_tu.h"
#include "musyx.h"

/* ---- The synth voice ----
 * sound_synth_voices[i], 0x1A0 bytes, keeps the N64 layout on the PC (docs/port/MUSYX_DATA.md 8.3). Each handler
 * below sees it through a view of the fields it touches; the fields are named after their offset. The map:
 *   +0x00 addr, +0x04 curAddr        the macro's first step and the next step to run (window addresses)
 *   +0x08 retAddr, +0x0C retCurAddr  GOSUB's return point (0: none)
 *   +0x10 next, +0x14 previous       the voice ids chained behind one sound handle (PLAYMACRO adds children)
 *   +0x18                            the handle's node (voices.c)
 *   +0x1C trapAddr, +0x20 trapCurAddr where a key off jumps to (TRAP_KEY_OFF; 0: no trap)
 *   +0x24 flags                      VOICE_* below
 *   +0x28 age, +0x2C ageSpeed        the voice-stealing age (15-bit fraction) and how fast it falls per tick
 *   +0x2E priority, +0x2F track      the allocation priority; the track whose volume applies (255: none)
 *   +0x30 volume, +0x34 orgVolume    16.16, 0..0x7F0000 (0..127): now and as started
 *   +0x38 panning, +0x3C panDelta    16.16; a PANNING sweep's step
 *   +0x40 sweepOff[2], +0x48 sweepNum[2]  the two pitch sweeps' offsets and step counts
 *   +0x4A channel, +0x4B set         the MIDI channel (255: none) and whose controllers it reads (VOICE_OWN_SET:
 *                                    the voice's own, an effect; else a song player's)
 *   +0x4C fxFlag                     an effect voice (the key's bit 7 at start)
 *   +0x4E orgNote, +0x50 curNote     the key it started on; the note now
 *   +0x52..+0x56 setupVol, setupPan, setupChannel, setupSet, setupTrack: what sound_macro_start stores and the first
 *                                    run applies
 *   +0x58 sampleAddress, +0x5C sampleInfo  the sample playing; its root key (bits 31..24) and rate (23..0)
 *   +0x60 id                         (macro << 16) | (key << 8) | voice index
 *   +0x64 macroId, +0x68 loop        the macro's id; the LOOP counter
 *   +0x6A, +0x6B                     the pitch wheel's range down and up, in semitones
 *   +0x6C, +0x70, +0x78, +0x79       vibrato: time, period, key range, cent range
 *   +0x7C sweepAdd[2]                the pitch sweeps' steps
 *   +0x84 envDelta, +0x88 envTarget  the volume envelope's step and target
 *   +0x8C, +0x90, +0x98              portamento: time, duration, mode
 *   +0x99, +0x9A                     the reverb send's scale and offset
 *   +0x9C wait                       when the current WAIT ends (0: not waiting; MACRO_WAIT_FOREVER)
 *   +0xA0 waitTime, +0xA4 time       when the last wait ended (tick waits count from it); the voice's clock
 *   +0xA8 panTime, +0xAC panTarget   the PANNING sweep's time left and end
 *   +0xB0 sweepCnt[2], +0xB4 envTime the pitch sweeps' counters; the envelope's time left
 *   +0xB8 pbLast                     the last pitch-wheel value (0x2000: centre)
 *   +0xBA allocId, +0xBD blocked     the allocation id; set while the voice starts a child (not to be stolen)
 *   +0xBE vGroup, +0xBF setupVGroup  the volume group (sound_volume_faders) now and as started
 *   +0xC0 curDetune, +0xC1 lastNote  cents; the channel's previous key (for LAST_NOTE)
 *   +0xC2 curOutputVolume
 *   +0xC4 inputs[9]                  18 bytes each: the controller mixes of SYNTH_INPUT_* (controllers.c)
 *   +0x168 lfo[2]                    12 bytes each: time, period, value
 *   +0x180 localVars[16]             the macro's variables (the globals are sound_global_variables) */

/* The voice's controller inputs (+0xC4), in controllers.c's order. */
enum SynthInput {
    SYNTH_INPUT_VOLUME, SYNTH_INPUT_PAN, SYNTH_INPUT_SPAN, SYNTH_INPUT_PITCH_BEND, SYNTH_INPUT_DOPPLER,
    SYNTH_INPUT_MODULATION, SYNTH_INPUT_PEDAL, SYNTH_INPUT_PORTAMENTO, SYNTH_INPUT_REVERB, SYNTH_INPUT_COUNT
};

#define MACRO_WAIT_FOREVER        0xFFFFFFFFU /* +0x9C: wait until key off or the sample's end */
#define MACRO_KEY_EFFECT          128         /* sound_macro_start's key: an effect, with its own controllers */
#define MACRO_STEPS_PER_RUN       32          /* sound_macro_run gives up after this many steps in one tick */

/* The opcodes of a macro step: the low 7 bits of word[0]; the parameters are its other bytes (each handler says
 * which). Other values do nothing. */
#define MACRO_OPCODE_MASK         0x7F
enum MacroOpcode {
    MACRO_END = 0x00, MACRO_RETURN = 0x01, MACRO_SPLIT_KEY = 0x02, MACRO_SPLIT_VELOCITY = 0x03,
    MACRO_WAIT = 0x04, MACRO_LOOP = 0x05, MACRO_GOSUB = 0x06, MACRO_WAIT_MS = 0x07,
    MACRO_PLAY_MACRO = 0x08, MACRO_SEND_KEY_OFF = 0x09, MACRO_SPLIT_MODULATION = 0x0A, MACRO_PIANO_PAN = 0x0B,
    MACRO_SET_ADSR = 0x0C, MACRO_SCALE_VOLUME = 0x0D, MACRO_PANNING = 0x0E, MACRO_ENVELOPE = 0x0F,
    MACRO_START_SAMPLE = 0x10, MACRO_STOP_SAMPLE = 0x11, MACRO_KEY_OFF = 0x12, MACRO_SPLIT_RANDOM = 0x13,
    MACRO_FADE_IN = 0x14, MACRO_RANDOM_NOTE = 0x17, MACRO_ADD_NOTE = 0x18, MACRO_SET_NOTE = 0x19,
    MACRO_LAST_NOTE = 0x1A, MACRO_PORTAMENTO = 0x1B, MACRO_VIBRATO = 0x1C, MACRO_PITCH_SWEEP_1 = 0x1D,
    MACRO_PITCH_SWEEP_2 = 0x1E, MACRO_SET_PITCH = 0x1F, MACRO_TRAP_KEY_OFF = 0x28, MACRO_UNTRAP_KEY_OFF = 0x29,
    MACRO_ADD_AGE = 0x30, MACRO_SET_AGE = 0x31, MACRO_SEND_FLAG = 0x32, MACRO_SET_PITCH_WHEEL_RANGE = 0x33,
    MACRO_SET_REVERB_SEND = 0x34, MACRO_TRACK_PITCH_WHEEL = 0x35, MACRO_SET_PRIORITY = 0x36,
    MACRO_ADD_PRIORITY = 0x37, MACRO_SET_AGE_SPEED = 0x38, MACRO_SET_AGE_BY_VELOCITY = 0x39,
    MACRO_SELECT_VOLUME = 0x40, MACRO_SELECT_PAN = 0x41, MACRO_SELECT_PITCH_BEND = 0x42,
    MACRO_SELECT_MODULATION = 0x43, MACRO_SELECT_PEDAL = 0x44, MACRO_SELECT_PORTAMENTO = 0x45,
    MACRO_SELECT_REVERB = 0x46, MACRO_SELECT_SPAN = 0x47, MACRO_SELECT_DOPPLER = 0x48,
    MACRO_SETUP_LFO = 0x50,
    MACRO_ADD_VARIABLES = 0x60, MACRO_SUBTRACT_VARIABLES = 0x61, MACRO_MULTIPLY_VARIABLES = 0x62,
    MACRO_DIVIDE_VARIABLES = 0x63, MACRO_ADD_TO_VARIABLE = 0x64, MACRO_SET_VARIABLE = 0x65,
    MACRO_IF_EQUAL = 0x70, MACRO_IF_LESS = 0x71
};

/* sound_macro_variable_arithmetic's operations and sound_macro_compare's comparisons. */
enum MacroArithmetic { MACRO_ARITH_ADD, MACRO_ARITH_SUBTRACT, MACRO_ARITH_MULTIPLY, MACRO_ARITH_DIVIDE, MACRO_ARITH_ADD_IMMEDIATE };
enum MacroComparison { MACRO_COMPARE_EQUAL, MACRO_COMPARE_LESS };
/* How a controller source combines with the ones before it in an input's mix (controllers.c). */
enum MacroCombine { MACRO_COMBINE_SET, MACRO_COMBINE_ADD, MACRO_COMBINE_MULTIPLY };

/* The view of the voice the channel-owner functions and sound_synth_key_off_voice use. */
#pragma pack(1)
typedef struct VoiceState { u8 unknown00[36]; u32 flags24; u8 unknown28[34]; u8 channel4A; u8 set4B; u8 unknown4C[20]; u32 id60; u8 unknown64[268]; s16 lfo170; u8 unknown172[10]; s16 lfo17C; u8 unknown17E[34]; } VoiceState;
#pragma pack(0)
/* On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9), seen here in this file's views: the voices and an effect voice's legato owner.
 * PC_MX_LOOP(32) is the run's voices where the N64 clears its 32 (docs/port/AUDIO.md 10). */
#define sound_synth_voices (*(VoiceState (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)
#define sound_voice_channel_owners (*(u8 (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_legato)

/* The generic voice (the handlers' first parameter where they need no fields) and a macro step. */
typedef struct MacroState { u8 unknown00[0x30]; u32 volume30; u8 unknown34[0x2C]; u32 id60; } MacroState;
typedef struct MacroCommand { u32 word[2]; } MacroCommand;
/* PC: the voice's macro and trap pointers (+0, +4, +8, +0xC, +0x1C, +0x20) point into the pool, in the game heap, so
 * on the PC they are 32-bit window addresses: N64P(x) is such a field as a pointer, PC_N64W(x) a pointer stored into
 * one, and N64_NEXT(x) moves one to the next step (docs/port/MUSYX_DATA.md 8.3). */
enum n64p_MacroCommand { n64p_MacroCommand_ };
#define N64P(x) _Generic((x), enum n64p_MacroCommand: (MacroCommand *)PC_N64P(x), default: (x))
#define N64_NEXT(x) ((x) = PC_N64W(N64P(x) + 1))

/* ---- What the file uses from the rest of the library ---- */
struct MacroStateSplitModulation;
struct MacroStatePlayMacro;
struct MacroStatePortamento;
struct MacroStateStart;
struct MacroSampleInfo;
struct Envelope;
struct VoiceControls;
/* data */
extern u8 sound_midi_channel_owners[8][16];
extern u8 sound_voice_count;
extern u16 sound_global_variables[];
extern s16 sound_seq_current_index[];      /* only to reach sound_global_variables: see sound_macro_get_variable */
extern u32 sound_tick_step;
/* voices.c */
extern int sound_alloc_voice(u8, u8, u16, u8);
extern void sound_unlink_voice(MacroState *);
extern void sound_free_voice(MacroState *);
extern void sound_set_voice_priority(MacroState *, u8);
extern u32 sound_new_handle(struct MacroStateStart *);
/* synth.c */
extern void sound_synth_copy_controllers(struct MacroStatePlayMacro *, struct MacroStatePlayMacro *);
extern void sound_synth_start_portamento(struct MacroStatePortamento *);
/* rate.c */
extern u32 sound_rate_to_pitch(u32);
/* controllers.c */
extern u16 sound_synth_get_modulation_input(struct MacroStateSplitModulation *);
extern void sound_synth_init_controller_inputs(struct VoiceControls *);
extern void sound_midi_reset_channel(u8, u8);
extern void sound_midi_set_last_key(u8, u8, u8);
extern unsigned char sound_midi_get_last_key(unsigned char channel, unsigned char set);
extern void sound_midi_set_portamento_state(u8, u8, u8);
extern u8 sound_macro_map_controller(u8);
extern u16 sound_macro_get_controller(VoiceState *voice, u8 controller);
extern void sound_macro_set_controller(VoiceState *voice, u8 controller, s16 value);
/* the registries (registry.c) */
extern MacroCommand *sound_find_macro(u16);
extern int sound_find_sample(u16, struct MacroSampleInfo *);
extern void *sound_find_table(u16);
/* the hardware layer (hardware.c) */
extern u8 sound_hw_is_voice_active(int);
extern void sound_hw_set_voice_sample(u32, struct MacroSampleInfo *, u8);
extern void sound_hw_stop_sample(u32, u8);
extern void sound_hw_set_envelope(u32, const struct Envelope *);
extern void sound_hw_break_voice(int);
/* time and randomness (util.c) */
extern u16 sound_random(void);
extern void sound_ms_to_time(u32 *);
extern void sound_ticks_to_time(u32 *, MacroState *);
extern u32 sound_time_to_ms(u32);
/* this file, used before its definition */
extern u32 sound_macro_start(u32 packed, u16 allocation, u8 key, u8 volume, u8 panning, u8 channel, u8 set, u16 offset, u16 track, u8 start, u8 group);

/* Keys off the voice `id` (a voice id, not a handle): flags it VOICE_KEY_OFF, which ends a WAIT for key off, jumps to
 * a key-off trap or ends a loop at the next run. 0, or -1 if no such voice plays (the voice was reused). */
int sound_synth_key_off_voice(u32 id)
{
    VoiceState *voice;
    if (id != VOICE_NONE) {
        voice = &sound_synth_voices[VOICE_INDEX(id)];
        if (voice->id60 == id) {
            voice->flags24 |= VOICE_KEY_OFF;
            return 0;
        }
    }
    return -1;
}

/* A channel's legato owner: the voice last started on a MIDI channel owns it (sound_midi_channel_owners[set][channel],
 * or for an effect voice, which is its own channel, sound_voice_channel_owners[voice]); only the owner records the
 * notes it plays as the channel's last key. */

/* Whether the voice owns its MIDI channel (1) or not, or has none (0). */
int sound_synth_owns_channel(struct MacroState *state)
{
    u8 index;
    if (((VoiceState *)state)->id60 != VOICE_NONE && ((VoiceState *)state)->channel4A != NO_CHANNEL) {
        index = ((VoiceState *)state)->id60 & 0xFF;
        if (((VoiceState *)state)->set4B == VOICE_OWN_SET) {
            if (sound_voice_channel_owners[index] == index) return 1;
        } else {
            if (sound_midi_channel_owners[((VoiceState *)state)->set4B][((VoiceState *)state)->channel4A] == index) return 1;
        }
    }
    return 0;
}

/* Makes the voice its MIDI channel's owner (when its macro starts). */
void sound_synth_claim_channel(VoiceState *voice)
{
    u8 index;
    if (voice->id60 != VOICE_NONE && voice->channel4A != NO_CHANNEL) {
        index = voice->id60 & 0xFF;
        if (voice->set4B == VOICE_OWN_SET) {
            sound_voice_channel_owners[index] = index;
        } else {
            sound_midi_channel_owners[voice->set4B][voice->channel4A] = index;
        }
    }
}

/* Gives up the voice's MIDI channel if the voice still owns it (255: no owner). */
void sound_synth_release_channel(VoiceState *voice)
{
    u8 index;
    if (voice->id60 != VOICE_NONE && voice->channel4A != NO_CHANNEL) {
        index = voice->id60 & 0xFF;
        if (voice->set4B == VOICE_OWN_SET) {
            if (sound_voice_channel_owners[index] == index) sound_voice_channel_owners[index] = 255;
        } else {
            if (sound_midi_channel_owners[voice->set4B][voice->channel4A] == index) sound_midi_channel_owners[voice->set4B][voice->channel4A] = 255;
        }
    }
}

/* No channel has an owner: the 8 players' 16 channels and every effect voice's own. */
void sound_synth_init_channel_owners(void)
{
    u32 row;
    u32 index;
    for (row = 0; row < 8; row++) {
        for (index = 0; index < 16; index++) {
            sound_midi_channel_owners[row][index] = 255;
        }
    }
    for (index = 0; index < PC_MX_LOOP(32); index++) {
        sound_voice_channel_owners[index] = 255;
    }
}

#pragma pack(1)
typedef struct MacroStateWait { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x38]; u32 id60; u8 unknown64[0x38]; u32 wait9C; u32 waitTimeA0; u32 timeA4; } MacroStateWait;
#pragma pack()

/* The handlers. Each takes the voice (in its own view) and the step, and returns nonzero when the voice stops
 * running steps for this tick (it waits, or it ended); sound_macro_run dispatches to them. The step's parameters
 * are its bytes: "byte 1" is (word[0] >> 8) & 0xFF, "byte 2" (word[0] >> 16) & 0xFF, "byte 3" word[0] >> 24, and
 * word[1] holds the rest (often a 16-bit value in its top half). */

/* WAIT (and the waits other steps end with): waits word[1]'s top 16 bits in ticks, or in milliseconds when byte 1
 * of word[1] has bit 0 (WAIT_MS sets it); 0xFFFF waits forever. Byte 1 bit 0: the wait also ends at key off (and is
 * skipped if the key is already off); byte 3 bit 0: it also ends when the sample does (and is skipped if no sample
 * plays); byte 2 bit 0: a random time below the given one. A tick wait counts from when the last wait ended, so a
 * macro keeps its rhythm. Returns whether the voice now waits. */
u8 sound_macro_wait(MacroState *state, MacroCommand *command)
{
    u32 time;
    u8 milliseconds;
    time = (u16)(command->word[1] >> 16);
    if (time != 0) {
        if ((u8)(command->word[0] >> 8) & 1) {
            if (((MacroStateWait *)state)->flags24 & VOICE_KEY_OFF) return 0;
            ((MacroStateWait *)state)->flags24 |= VOICE_WAIT_KEY_OFF;
        } else {
            ((MacroStateWait *)state)->flags24 &= ~VOICE_WAIT_KEY_OFF;
        }
        if ((u8)(command->word[0] >> 24) & 1) {
            if (!(((MacroStateWait *)state)->flags24 & VOICE_START_SAMPLE) && !sound_hw_is_voice_active(VOICE_INDEX(((MacroStateWait *)state)->id60))) return 0;
            ((MacroStateWait *)state)->flags24 |= VOICE_WAIT_SAMPLE_END;
        } else {
            ((MacroStateWait *)state)->flags24 &= ~VOICE_WAIT_SAMPLE_END;
        }
        if ((u8)(command->word[0] >> 16) & 1) time = sound_random() % time;
        if (time != 0xFFFF) {
            milliseconds = (u8)(command->word[1] >> 8) & 1;
            if (milliseconds) sound_ms_to_time(&time);
            else sound_ticks_to_time(&time, state);
            if (milliseconds) {
                ((MacroStateWait *)state)->wait9C = ((MacroStateWait *)state)->timeA4 + time;
            } else {
                ((MacroStateWait *)state)->wait9C = ((MacroStateWait *)state)->waitTimeA0 + time;
                if (((MacroStateWait *)state)->wait9C <= ((MacroStateWait *)state)->timeA4) {
                    /* already due (the macro is behind): no wait, and the next one counts from here */
                    ((MacroStateWait *)state)->waitTimeA0 = ((MacroStateWait *)state)->wait9C;
                    ((MacroStateWait *)state)->wait9C = 0;
                }
            }
        } else {
            ((MacroStateWait *)state)->wait9C = MACRO_WAIT_FOREVER;
        }
        return ((MacroStateWait *)state)->wait9C != 0;
    }
    return 0;
}

/* WAIT_MS: WAIT with the time in milliseconds (sets the milliseconds flag in the step's copy).
 * PC: the step's words are in host order (port/assets/musyx.c swaps the pool's macros), so the N64's byte 6 is
 * not bits 15..8 of word[1], the milliseconds flag sound_macro_wait tests. The same store, on the word. */
u8 sound_macro_wait_ms(MacroState *state, MacroCommand *command)
{
    command->word[1] = (command->word[1] & ~0xFF00U) | 0x100U;
    return sound_macro_wait(state, command);
}

/* END: the macro is over; the voice leaves its handle's chain and is freed. */
u8 sound_macro_end(MacroState *state, MacroCommand *command)
{
    sound_unlink_voice(state);
    sound_free_voice(state);
    return 1;
}

#pragma pack(1)
typedef struct MacroStateReturn { u32 addr00; u32 curAddr04; u32 retAddr08; u32 retCurAddr0C; u8 unknown10[0xC]; enum n64p_MacroCommand trapAddr1C; enum n64p_MacroCommand trapCurAddr20; u8 unknown24[0xA]; u8 priority2E; } MacroStateReturn;
#pragma pack(0)
/* RETURN: back to where the last GOSUB left; with byte 1 zero, or no GOSUB to return from, it ends the macro as END
 * does. */
u8 sound_macro_return(MacroStateReturn *state, MacroCommand *command)
{
    if (((command->word[0] >> 8) & 0xFF) == 0 || state->retAddr08 == 0) {
        return sound_macro_end((MacroState *)state, command);
    }
    state->addr00 = state->retAddr08;
    state->curAddr04 = state->retCurAddr0C;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSplitKey { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; enum n64p_MacroCommand retAddr08; enum n64p_MacroCommand retCurAddr0C; u8 unknown10[0x20]; u32 volume30; u8 unknown34[0x16]; u8 channel4A; u8 unknown4B[5]; u16 curNote50; } MacroStateSplitKey;
#pragma pack(0)
/* The SPLIT_* steps jump to another macro (bytes 2-3: its id; word[1]'s low half: the step to start at) when a value
 * reaches byte 1, else go on. SPLIT_KEY: when the note does. */
u8 sound_macro_split_key(MacroStateSplitKey *state, MacroCommand *command)
{
    MacroCommand *address;
    if (state->curNote50 >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->addr00 = PC_N64W(address);
            state->curAddr04 = PC_N64W(address + (command->word[1] & 0xFFFF));
        }
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSplitVelocity { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; enum n64p_MacroCommand retAddr08; enum n64p_MacroCommand retCurAddr0C; u8 unknown10[0x20]; u32 volume30; u8 unknown34[0x16]; u8 channel4A; u8 unknown4B[5]; u16 curNote50; } MacroStateSplitVelocity;
#pragma pack(0)
/* SPLIT_VELOCITY: jumps when the volume (the note's velocity, 0..127) reaches byte 1. */
u8 sound_macro_split_velocity(MacroStateSplitVelocity *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((state->volume30 >> 16) >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->addr00 = PC_N64W(address);
            state->curAddr04 = PC_N64W(address + (command->word[1] & 0xFFFF));
        }
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSplitModulation { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; enum n64p_MacroCommand retAddr08; enum n64p_MacroCommand retCurAddr0C; u8 unknown10[0x20]; u32 volume30; u8 unknown34[0x16]; u8 channel4A; u8 unknown4B[5]; u16 curNote50; } MacroStateSplitModulation;
#pragma pack(0)
/* SPLIT_MODULATION: jumps when the modulation input (0..127) reaches byte 1; never without a MIDI channel. */
u8 sound_macro_split_modulation(MacroStateSplitModulation *state, MacroCommand *command)
{
    MacroCommand *address;
    u8 modulation;
    if (state->channel4A != NO_CHANNEL) {
        modulation = sound_synth_get_modulation_input(state) >> 7;
        if (modulation >= (u8)(command->word[0] >> 8)) {
            if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
                state->addr00 = PC_N64W(address);
                state->curAddr04 = PC_N64W(address + (command->word[1] & 0xFFFF));
            }
        }
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSplitRandom { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; enum n64p_MacroCommand retAddr08; enum n64p_MacroCommand retCurAddr0C; u8 unknown10[0x20]; u32 volume30; u8 unknown34[0x16]; u8 channel4A; u8 unknown4B[5]; u16 curNote50; } MacroStateSplitRandom;
#pragma pack(0)
/* SPLIT_RANDOM: jumps when a random byte reaches byte 1. */
u8 sound_macro_split_random(MacroStateSplitRandom *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((u8)sound_random() >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->addr00 = PC_N64W(address);
            state->curAddr04 = PC_N64W(address + (command->word[1] & 0xFFFF));
        }
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateGosub { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; enum n64p_MacroCommand retAddr08; enum n64p_MacroCommand retCurAddr0C; u8 unknown10[0x20]; u32 volume30; u8 unknown34[0x16]; u8 channel4A; u8 unknown4B[5]; u16 curNote50; } MacroStateGosub;
#pragma pack(0)
/* GOSUB: runs another macro (bytes 2-3: its id; word[1]'s low half: the step), remembering where to RETURN (one
 * level). A missing macro ends this one. */
u8 sound_macro_gosub(MacroStateGosub *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((address = sound_find_macro(command->word[0] >> 16)) != 0) {
        state->retAddr08 = state->addr00;
        state->retCurAddr0C = state->curAddr04;
        state->addr00 = PC_N64W(address);
        state->curAddr04 = PC_N64W(address + (command->word[1] & 0xFFFF));
        return 0;
    }
    return sound_macro_end((MacroState *)state, command);
}

#pragma pack(1)
typedef struct MacroStateTrapKeyOff { u32 addr00; u32 curAddr04; u32 retAddr08; u32 retCurAddr0C; u8 unknown10[0xC]; enum n64p_MacroCommand trapAddr1C; enum n64p_MacroCommand trapCurAddr20; u8 unknown24[0xA]; u8 priority2E; } MacroStateTrapKeyOff;
#pragma pack(0)
/* TRAP_KEY_OFF: from now on a key off jumps to the macro of bytes 2-3 at word[1]'s low half (sound_macro_run takes
 * the jump), unless the sustain pedal holds the key. */
u8 sound_macro_trap_key_off(MacroStateTrapKeyOff *state, MacroCommand *command)
{
    MacroCommand *start;
    start = sound_find_macro((command->word[0] >> 16) & 0xFFFF);
    if (start != 0) {
        state->trapAddr1C = PC_N64W(start);
        state->trapCurAddr20 = PC_N64W(start + (command->word[1] & 0xFFFF));
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateUntrapKeyOff { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateUntrapKeyOff;
#pragma pack(0)
typedef struct MacroCommandUntrapKeyOff { u32 word[2]; } MacroCommandUntrapKeyOff;
/* UNTRAP_KEY_OFF: a key off no longer jumps. */
u8 sound_macro_untrap_key_off(MacroStateUntrapKeyOff *state, MacroCommandUntrapKeyOff *command)
{
    state->trapAddr1C = state->trapCurAddr20 = 0;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateLoop { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; u8 unknown08[0x1C]; u32 flags24; u8 unknown28[0x38]; u32 id60; u8 unknown64[4]; u16 loop68; } MacroStateLoop;
#pragma pack()
/* LOOP: jumps back to a step of this macro (word[1]'s low half) a number of times (word[1]'s top half; byte 2 bit 0:
 * a random number below it; 0xFFFF: for ever). Byte 1 bit 0: stop looping once the key is off (and the pedal up);
 * byte 3 bit 0: stop looping once the sample has ended. The counter (+0x68) is 0 between loops. */
u8 sound_macro_loop(MacroStateLoop *state, MacroCommand *command)
{
    if (state->loop68 == 0) {
        if ((u8)(command->word[0] >> 16) & 1) {
            state->loop68 = sound_random() % (u16)(command->word[1] >> 16);
        } else {
            state->loop68 = command->word[1] >> 16;
        }
        if (state->loop68 == 0xFFFF) goto skip;
        ++state->loop68;
    } else if (state->loop68 == 0xFFFF) {
        goto skip;
    }
    if (--state->loop68 != 0) {
skip:
        if (((u8)(command->word[0] >> 8) & 1) != 0 &&
            !(state->flags24 & VOICE_PEDAL_DOWN) && (state->flags24 & VOICE_KEY_OFF)) {
            state->loop68 = 0;
            return 0;
        }
        if (((u8)(command->word[0] >> 24) & 1) &&
            !(state->flags24 & VOICE_START_SAMPLE) && !sound_hw_is_voice_active(VOICE_INDEX(state->id60))) {
            state->loop68 = 0;
            return 0;
        }
        state->curAddr04 = PC_N64W(N64P(state->addr00) + (command->word[1] & 0xFFFF));
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStatePlayMacro { u8 unknown00[0x10]; u32 next10; u32 previous14; u8 unknown18[0x17]; u8 track2F; u32 volume30; u8 unknown34[4]; u32 panning38; u8 unknown3C[0xE]; u8 channel4A; u8 set4B; u8 fxFlag4C; u8 unknown4D; u16 orgNote4E; u8 unknown50[0x10]; u32 id60; u8 unknown64[0x56]; u16 allocIdBA; u8 unknownBC; u8 blockedBD; u8 vGroupBE; u8 unknownBF[0xE1]; } MacroStatePlayMacro;
#pragma pack()
/* PLAY_MACRO: starts another macro on a new voice that joins this voice's handle: its id (bytes 2-3), priority and
 * voice limit (word[1]'s bytes 2 and 3) and first step (word[1]'s low half), on this voice's key plus byte 1 (signed),
 * with its volume, pan, channel, track and volume group. The new voice goes into the chain right after this one; an
 * effect voice's child starts with its controllers. The voice is blocked meanwhile, so the allocation can't take it. */
u8 sound_macro_play_macro(MacroStatePlayMacro *state, MacroCommand *command)
{
    s32 key;
    u32 macro;
    u32 child;
    macro = (u16)(command->word[0] >> 16) << 16 | ((command->word[1] >> 16) & 0xFF) << 8 | ((command->word[1] >> 24) & 0xFF);
    key = state->orgNote4E + (s8)(u8)(command->word[0] >> 8);
    key = key < 0 ? 0 : key > 127 ? 127 : key;
    if (state->fxFlag4C) key |= 128;
    state->blockedBD = 1;
    child = sound_macro_start(macro, state->allocIdBA, key,
        state->volume30 >> 16, state->panning38 >> 16, state->channel4A,
        state->set4B, command->word[1], state->track2F, 0, state->vGroupBE);
    state->blockedBD = 0;
    if (child != VOICE_NONE) {
        ((MacroStatePlayMacro *)&sound_synth_voices[VOICE_INDEX(child)])->previous14 = state->id60;
        if (state->next10 != VOICE_NONE) {
            ((MacroStatePlayMacro *)&sound_synth_voices[VOICE_INDEX(child)])->next10 = state->next10;
            ((MacroStatePlayMacro *)&sound_synth_voices[VOICE_INDEX(state->next10)])->previous14 = child;
        }
        state->next10 = child;
        if (state->fxFlag4C) sound_synth_copy_controllers((MacroStatePlayMacro *)&sound_synth_voices[VOICE_INDEX(child)], state);
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSendKeyOff { u8 unknown00[0x4E]; u16 orgNote4E; } MacroStateSendKeyOff;
#pragma pack(0)
/* SEND_KEY_OFF: keys off every voice that plays the macro of bytes 2-3 on this voice's key plus byte 1 (a voice id
 * is (macro << 16) | (key << 8) | voice). */
u8 sound_macro_send_key_off(MacroStateSendKeyOff *voice, MacroCommand *command)
{
    u32 i;
    u32 id;
    id = (((command->word[0] >> 8) & 255) + voice->orgNote4E) << 8;
    id |= (u32)(u16)(command->word[0] >> 16) << 16;
    for (i = 0; i < sound_voice_count; ++i) {
        if ((id | i) == sound_synth_voices[i].id60) {
            sound_synth_key_off_voice(id | i);
        }
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateAddAge { u8 unknown00[0x28]; u32 age28; u16 ageSpeed2C; u8 unknown2E[2]; u32 volume30; u8 unknown34[4]; s32 panning38; u8 unknown3C[0x14]; u16 curNote50; } MacroStateAddAge;
#pragma pack(0)
typedef struct MacroCommandAddAge { u32 word[2]; } MacroCommandAddAge;
/* The age steps: a voice's age (+0x28, 15 bits of fraction) falls by ageSpeed each tick (synth.c), and among the
 * voices of the lowest priority the allocator steals the one of the lowest age (voices.c). A voice starts at 60000.
 * ADD_AGE: adds bytes 2-3 (signed) to it, clamped to 0..65535. */
u8 sound_macro_add_age(MacroStateAddAge *state, MacroCommandAddAge *command)
{
    s16 delta;
    s32 age;
    delta = command->word[0] >> 16;
    age = (state->age28 >> 15) + delta;
    if (age < 0) {
        state->age28 = 0;
    } else if (age > 65535) {
        state->age28 = 0x7FFF8000;
    } else {
        state->age28 = (u32)age << 15;
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetAge { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateSetAge;
#pragma pack(0)
typedef struct MacroCommandSetAge { u32 word[2]; } MacroCommandSetAge;
/* SET_AGE: the age becomes bytes 2-3. */
u8 sound_macro_set_age(MacroStateSetAge *state, MacroCommandSetAge *command)
{
    state->age28 = ((u16)(command->word[0] >> 16)) << 15;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetAgeSpeed { u8 unknown00[0x28]; u32 age28; u16 ageSpeed2C; u8 unknown2E[2]; u32 volume30; u8 unknown34[4]; s32 panning38; u8 unknown3C[0x14]; u16 curNote50; } MacroStateSetAgeSpeed;
#pragma pack(0)
typedef struct MacroCommandSetAgeSpeed { u32 word[2]; } MacroCommandSetAgeSpeed;
/* SET_AGE_SPEED: the age falls to 0 in word[1] ticks (0: it stays). */
u8 sound_macro_set_age_speed(MacroStateSetAgeSpeed *state, MacroCommandSetAgeSpeed *command)
{
    u32 divisor;
    divisor = command->word[1];
    if (divisor != 0) {
        state->ageSpeed2C = (state->age28 >> 8) / divisor;
    } else {
        state->ageSpeed2C = 0;
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetAgeByVelocity { u8 unknown00[0x28]; u32 age28; u16 ageSpeed2C; u8 unknown2E[2]; u32 volume30; u8 unknown34[4]; s32 panning38; u8 unknown3C[0x14]; u16 curNote50; } MacroStateSetAgeByVelocity;
#pragma pack(0)
typedef struct MacroCommandSetAgeByVelocity { u32 word[2]; } MacroCommandSetAgeByVelocity;
/* SET_AGE_BY_VELOCITY: the age becomes bytes 2-3 plus word[1]'s low half scaled by the velocity (the volume, /128),
 * at most 60000. */
u8 sound_macro_set_age_by_velocity(MacroStateSetAgeByVelocity *state, MacroCommandSetAgeByVelocity *command)
{
    u32 age;
    age = (u16)(command->word[0] >> 16)
          + (((command->word[1] & 0xFFFF) * (state->volume30 >> 16)) >> 7);
    if (age > 60000) {
        state->age28 = 0x75300000;
    } else {
        state->age28 = age << 15;
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateAddPriority { u32 addr00; u32 curAddr04; u32 retAddr08; u32 retCurAddr0C; u8 unknown10[0xC]; enum n64p_MacroCommand trapAddr1C; enum n64p_MacroCommand trapCurAddr20; u8 unknown24[0xA]; u8 priority2E; } MacroStateAddPriority;
#pragma pack(0)
/* ADD_PRIORITY: adds bytes 2-3 (signed) to the voice's priority, clamped to 0..255. */
u8 sound_macro_add_priority(MacroStateAddPriority *state, MacroCommand *command)
{
    s32 delta;
    s32 priority;
    delta = (s16)(command->word[0] >> 16);
    priority = (s16)(state->priority2E + delta);
    if (priority < 0) {
        priority = 0;
    } else {
        priority = (s16)(priority > 255 ? 255 : priority);
    }
    sound_set_voice_priority((MacroState *)state, (u8)priority);
    return 0;
}

/* SET_PRIORITY: the voice's priority becomes byte 1. */
u8 sound_macro_set_priority(MacroState *state, MacroCommand *command)
{
    sound_set_voice_priority(state, (u8)(command->word[0] >> 8));
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSendFlag { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateSendFlag;
#pragma pack(0)
typedef struct MacroCommandSendFlag { u32 word[2]; } MacroCommandSendFlag;
/* SEND_FLAG: sets the global macro variable of byte 1 to byte 2 (the variables macros share,
 * sound_global_variables). */
u8 sound_macro_send_flag(MacroStateSendFlag *state, MacroCommandSendFlag *command)
{
    sound_global_variables[(u8)(command->word[0] >> 8)] = (u8)(command->word[0] >> 16);
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetPitchWheelRange { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateSetPitchWheelRange;
#pragma pack(0)
typedef struct MacroCommandSetPitchWheelRange { u32 word[2]; } MacroCommandSetPitchWheelRange;
/* SET_PITCH_WHEEL_RANGE: how many semitones the pitch wheel bends down (byte 2) and up (byte 1). */
u8 sound_macro_set_pitch_wheel_range(MacroStateSetPitchWheelRange *state, MacroCommandSetPitchWheelRange *command)
{
    state->pbLowerKeyRange6A = command->word[0] >> 16;
    state->pbUpperKeyRange6B = command->word[0] >> 8;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetNote { u8 unknown00[0x4A]; u8 channel4A; u8 set4B; u8 unknown4C[4]; u16 curNote50; u8 unknown52[0x6E]; s8 curDetuneC0; u8 lastNoteC1; } MacroStateSetNote;
#pragma pack()
/* The note steps set the note and detune (byte 2, cents, signed), make it the channel's last key if the voice owns
 * the channel, then wait as WAIT does with word[1] (the step's word[0] becomes a plain WAIT's).
 * SET_NOTE: the note is byte 1. */
u8 sound_macro_set_note(MacroStateSetNote *state, MacroCommand *command)
{
    state->curNote50 = (u8)(command->word[0] >> 8) & 0x7F;
    state->curDetuneC0 = command->word[0] >> 16;
    if (sound_synth_owns_channel((MacroState *)state)) {
        sound_midi_set_last_key(state->channel4A, state->set4B, state->curNote50);
    }
    command->word[0] = MACRO_WAIT;
    return sound_macro_wait((MacroState *)state, command);
}

#pragma pack(1)
typedef struct MacroStateAddNote { u8 unknown00[0x4A]; u8 channel4A; u8 set4B; u8 unknown4C[2]; u16 orgNote4E; u16 curNote50; u8 unknown52[0x6E]; s8 curDetuneC0; } MacroStateAddNote;
#pragma pack()
/* ADD_NOTE: adds byte 1 (signed) to the note, or with byte 3 set to the key the voice started on; clamped to
 * 0..127. */
u8 sound_macro_add_note(MacroStateAddNote *state, MacroCommand *command)
{
    if ((u8)(command->word[0] >> 24) == 0) {
        state->curNote50 += (s8)(u8)(command->word[0] >> 8);
    } else {
        state->curNote50 = state->orgNote4E + (s8)(u8)(command->word[0] >> 8);
    }
    state->curNote50 = (s16)state->curNote50 < 0 ? 0 : state->curNote50 > 127 ? 127 : state->curNote50;
    state->curDetuneC0 = command->word[0] >> 16;
    if (sound_synth_owns_channel((MacroState *)state)) {
        sound_midi_set_last_key(state->channel4A, state->set4B, state->curNote50);
    }
    command->word[0] = MACRO_WAIT;
    return sound_macro_wait((MacroState *)state, command);
}

#pragma pack(1)
typedef struct MacroStateLastNote { u8 unknown00[0x4A]; u8 channel4A; u8 set4B; u8 unknown4C[4]; u16 curNote50; u8 unknown52[0x6E]; s8 curDetuneC0; u8 lastNoteC1; } MacroStateLastNote;
#pragma pack()
/* LAST_NOTE: the note becomes the channel's previous key (when the voice started) plus byte 1 (signed), clamped
 * to 0..127. Unlike the others it records the note whether or not the voice owns the channel. */
u8 sound_macro_last_note(MacroStateLastNote *state, MacroCommand *command)
{
    state->curNote50 = state->lastNoteC1 + (s8)(u8)(command->word[0] >> 8);
    state->curNote50 = (s16)state->curNote50 < 0 ? 0 : state->curNote50 > 127 ? 127 : state->curNote50;
    state->curDetuneC0 = command->word[0] >> 16;
    if (state->channel4A != NO_CHANNEL) {
        sound_midi_set_last_key(state->channel4A, state->set4B, state->curNote50);
    }
    command->word[0] = MACRO_WAIT;
    return sound_macro_wait((MacroState *)state, command);
}

/* PC: the record sound_find_sample fills and sound_hw_set_voice_sample reads, both with a pointer-sized, packed address field
 * (a stream's host buffer in sound_stream_start's call); here it holds the sample's cartridge address. */
#pragma pack(1)
typedef struct MacroSampleInfo { u32 info; void *address; u32 offset; u32 length; u32 loopStart; u32 loopLength; u8 type; } MacroSampleInfo;
#pragma pack()
#pragma pack(1)
typedef struct MacroStateStartSample { u8 unknown00[0x24]; u32 flags24; u8 unknown28[8]; u32 volume30; u8 unknown34[0x24]; u32 sampleAddress58; u32 sampleInfo5C; u32 id60; } MacroStateStartSample;
#pragma pack(0)
/* Steps sound_macro_run has run in this tick (a file static, not a local, as in the original). */
static u8 macro_steps_this_update;
/* START_SAMPLE: gives the hardware voice the sample of bytes 1-2, starting word[1] samples in (byte 3: 0 as given,
 * 1 scaled down by the velocity, 2 scaled up by it, else 0; at most the sample's last), and marks it for the synth's
 * tick to start. Without a SET_ADSR the hardware voice's envelope is reset too. */
u8 sound_macro_start_sample(MacroStateStartSample *state, MacroCommand *command)
{
    static MacroSampleInfo sample;
    u32 index;
    if (sound_find_sample(command->word[0] >> 8, &sample) == 0) {
        switch ((u8)(command->word[0] >> 24)) {
        case 0:
            sample.offset = command->word[1];
            break;
        case 1:
            sample.offset = (command->word[1] * (127U - (state->volume30 >> 16))) / 127U;
            break;
        case 2:
            sample.offset = (command->word[1] * (state->volume30 >> 16)) / 127U;
            break;
        default:
            sample.offset = 0;
        }
        if (sample.offset >= sample.length) {
            sample.offset = sample.length - 1;
        }
        index = VOICE_INDEX(state->id60);
        if (!(state->flags24 & VOICE_ADSR_SET)) {
            sound_hw_set_voice_sample(index, &sample, 1);
        } else {
            sound_hw_set_voice_sample(index, &sample, 0);
        }
        state->sampleInfo5C = sample.info;
        state->sampleAddress58 = sample.address;
        state->flags24 |= VOICE_START_SAMPLE;
    }
    return 0;
}

#pragma pack(1)
typedef struct MacroStateStopSample { u8 unknown00[0x30]; u32 volume30; u8 unknown34[0x2C]; u32 id60; } MacroStateStopSample;
#pragma pack(0)
/* STOP_SAMPLE: stops the hardware voice's sample (through its release). */
u8 sound_macro_stop_sample(MacroStateStopSample *state, MacroCommand *command)
{
    sound_hw_stop_sample(state->id60 & 0xFF, 1);
    return 0;
}

#pragma pack(1)
typedef struct MacroStateKeyOff { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateKeyOff;
#pragma pack(0)
typedef struct MacroCommandKeyOff { u32 word[2]; } MacroCommandKeyOff;
/* KEY_OFF: the synth's next tick releases the hardware voice (its envelope's release), once the pedal is up. */
u8 sound_macro_key_off(MacroStateKeyOff *state, MacroCommandKeyOff *command)
{
    state->flags24 |= VOICE_RELEASE;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateVibrato { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x44]; u32 vibCurTime6C; u32 vibPeriod70; u8 unknown74[4]; u8 vibKeyRange78; u8 vibCentRange79; } MacroStateVibrato;
#pragma pack()
/* VIBRATO: depth byte 1 semitones and byte 2 cents (signed: a negative depth starts half a period in, the other way
 * round), period word[1]'s top half in ticks or milliseconds (word[1] byte 1 bit 0); a period of 0 turns it off.
 * Byte 3 bits 0-1: the modulation wheel scales the depth. */
u8 sound_macro_vibrato(MacroStateVibrato *state, MacroCommand *command)
{
    u32 time;
    s8 keyRange;
    s8 centRange;
    if ((u8)(command->word[0] >> 24) & 3) {
        state->flags24 |= VOICE_VIBRATO_BY_MODULATION;
    } else {
        state->flags24 &= ~VOICE_VIBRATO_BY_MODULATION;
    }
    time = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&time);
    } else {
        sound_ticks_to_time(&time, (MacroState *)state);
    }
    if (time) {
        state->flags24 |= VOICE_VIBRATO;
        state->vibPeriod70 = time;
        keyRange = command->word[0] >> 8;
        centRange = command->word[0] >> 16;
        if (keyRange < 0) {
            if (centRange < 0) state->vibCentRange79 = -centRange;
            else state->vibCentRange79 = centRange;
            state->vibKeyRange78 = -keyRange;
            state->vibCurTime6C = state->vibPeriod70 / 2;
        } else {
            if (centRange < 0) {
                if (keyRange == 0) {
                    state->vibCentRange79 = -centRange;
                    state->vibCurTime6C = state->vibPeriod70 / 2;
                } else {
                    /* Original game, perhaps: a positive key range with negative cents becomes one key less and
                     * 100 - cents (1 key -30 cents: 0 keys 130 cents, 1.3 semitones), where 100 + cents (0.7)
                     * would keep the depth. */
                    --keyRange;
                    state->vibCentRange79 = 100 - centRange;
                    state->vibCurTime6C = 0;
                }
            } else {
                state->vibCentRange79 = centRange;
                state->vibCurTime6C = 0;
            }
            state->vibKeyRange78 = keyRange;
        }
    } else {
        state->flags24 &= ~VOICE_VIBRATO;
    }
    return 0;
}

typedef struct MacroCommandSetupLfo { u32 word[2]; } MacroCommandSetupLfo;
#pragma pack(1)
typedef struct MacroLfoControl { u32 time; u32 period; s16 value; s16 lastValue; } MacroLfoControl;
typedef struct MacroStateSetupLfo { u8 unknown00[0x168]; MacroLfoControl lfo168[2]; } MacroStateSetupLfo;
#pragma pack(0)
/* SETUP_LFO: sets the period of the voice's LFO of byte 1 (0 or 1) to bytes 2-3 milliseconds (0: off). The synth
 * turns each LFO into a sine wave the inputs can read as the controllers 0xA0 and 0xA1. A running LFO restarts its
 * phase. */
u8 sound_macro_setup_lfo(MacroStateSetupLfo *state, MacroCommandSetupLfo *command)
{
    u32 duration;
    u8 index;
    MacroLfoControl *control;
    index = command->word[0] >> 8;
    duration = (u16)(command->word[0] >> 16);
    sound_ms_to_time(&duration);
    control = state->lfo168 + index;
    if (control->period != 0) {
        control->time = 0;
    }
    control->period = duration;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetPitch { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x28]; u16 curNote50; u8 unknown52[0xA]; u32 sampleInfo5C; u8 unknown60[0x60]; s8 curDetuneC0; } MacroStateSetPitch;
#pragma pack(0)
typedef struct MacroCommandSetPitch { u32 word[2]; } MacroCommandSetPitch;
/* The semitone search's end: i once it has run past kf[0] (the N64's unsigned i at -1). */
#define PITCH_BELOW_TABLE 0xFFFFFFFFU
/* SET_PITCH: sets the note and detune that play the sample (its root key and rate, +0x5C) at the rate of bytes 1-2
 * (Hz): the ratio's octaves, then its semitone from the table of 2^(n/12) (in 4096ths) and the cents between. */
u8 sound_macro_set_pitch(MacroStateSetPitch *state, MacroCommandSetPitch *command)
{
    u32 ratio;
    u32 octaves_ratio;
    u32 i;
    u32 rate;
    u32 sample_rate;
    u32 octaves;
    /* 2^(n/12) for n = 0..12, in 4096ths. (Its name stays: renaming this static changes GCC's -O2 code.) */
    static u16 kf[13] = {
        4096, 4339, 4597, 4871, 5160, 5467, 5792, 6137, 6502, 6888, 7298, 7732, 8192,
    };

    rate = (command->word[0] >> 8) & 0xFFFF;
    sample_rate = state->sampleInfo5C & 0xFFFFFF;
    if (sample_rate == rate) {
        state->curNote50 = (u8)(state->sampleInfo5C >> 24);
        state->curDetuneC0 = 0;
    } else if (sample_rate < rate) {
        ratio = (rate << 12) / sample_rate;
        octaves_ratio = ratio >> 12;
        for (octaves = 0; octaves < 11; octaves++) {
            if (octaves_ratio < (1 << (octaves + 1))) {
                break;
            }
        }
        ratio /= 1 << octaves;
        /* Original game: the search is `for (i = 11; i >= 0; i--)` with an unsigned i. A rate an exact power-of-two
         * multiple of the sample's leaves ratio exactly 4096, which no entry lies below, and i runs on to -1: the
         * N64 reads the halfword before the table (another unit's data) as kf[-1] and, as long as that is below
         * 4096, sets the note one lower and the detune to (4096 - kf[-1]) * 100 / (4096 - kf[-1]) = 100 cents up,
         * the exact pitch (the synth adds detune / 100 to the note). The PC read kf[0xFFFFFFFF], 8 GiB past the
         * table, and crashed; it now stops at -1 and keeps the N64's result. The same below. */
        for (i = 11; i != PITCH_BELOW_TABLE; i--) {
            if (ratio > kf[i]) {
                break;
            }
        }
        if (i == PITCH_BELOW_TABLE) {
            state->curNote50 = (u8)(state->sampleInfo5C >> 24) + octaves * 12 - 1;
            state->curDetuneC0 = 100;
        } else {
            state->curNote50 = (u8)(state->sampleInfo5C >> 24) + octaves * 12 + i;
            state->curDetuneC0 = ((ratio - kf[i]) * 100) / (kf[i + 1] - kf[i]);
        }
    } else {
        ratio = (sample_rate << 12) / rate;
        octaves_ratio = ratio >> 12;
        for (octaves = 0; octaves < 11; octaves++) {
            if (octaves_ratio < (1 << (octaves + 1))) {
                break;
            }
        }
        ratio /= 1 << octaves;
        for (i = 11; i != PITCH_BELOW_TABLE; i--) {
            if (ratio > kf[i]) {
                break;
            }
        }
        if (i == PITCH_BELOW_TABLE) {
            state->curNote50 = (u8)(state->sampleInfo5C >> 24) - octaves * 12 + 1;
            state->curDetuneC0 = -100;
        } else {
            state->curNote50 = (u8)(state->sampleInfo5C >> 24) - octaves * 12 - i;
            state->curDetuneC0 = -(s8)(((ratio - kf[i]) * 100) / (kf[i + 1] - kf[i]));
        }
    }
    state->flags24 |= VOICE_PITCH_FROM_RATE;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetAdsr { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x38]; u32 id60; } MacroStateSetAdsr;
typedef struct PackedEnvelope { u16 attack, decay, sustain, release; } PackedEnvelope;
#pragma pack()
typedef struct Envelope { u16 attack, decay, sustain, release; } Envelope;
/* SET_ADSR: gives the hardware voice the envelope (attack, decay, sustain, release) of the table of bytes 1-2; the
 * project stores it big-endian, so each halfword is swapped. A START_SAMPLE then keeps it. */
u8 sound_macro_set_adsr(MacroStateSetAdsr *state, MacroCommand *command)
{
    Envelope envelope;
    PackedEnvelope *table;
    if ((table = sound_find_table(command->word[0] >> 8)) != 0) {
        envelope.attack = (table->attack >> 8) | (table->attack << 8);
        envelope.decay = (table->decay >> 8) | (table->decay << 8);
        envelope.sustain = (table->sustain >> 8) | (table->sustain << 8);
        envelope.release = (table->release >> 8) | (table->release << 8);
        sound_hw_set_envelope(state->id60 & 0xFF, &envelope);
        state->flags24 |= VOICE_ADSR_SET;
    }
    return 0;
}

typedef struct MacroStatePitchSweep { u8 unknown00[0x40]; u32 sweepOff40[2]; u8 sweepNum48[2]; u8 unknown4A[0x32]; u32 sweepAdd7C[2]; u8 unknown84[0x2C]; u8 sweepCntB0[2]; } MacroStatePitchSweep;
/* PITCH_SWEEP_1 and _2 (index 0, 1): each tick the synth adds a step (bytes 2-3, signed, as a pitch rate) to the
 * sweep's pitch offset, and back to 0 every byte-1 ticks: a repeating ramp. Then waits as WAIT does with word[1]. */
u8 sound_macro_pitch_sweep(MacroStatePitchSweep *state, MacroCommand *command, int index)
{
    s32 delta;
    state->sweepOff40[index] = 0;
    state->sweepNum48[index] = (u8)(command->word[0] >> 8);
    state->sweepCntB0[index] = state->sweepNum48[index];
    delta = (s16)(command->word[0] >> 16);
    if (delta >= 0) {
        delta = (s32)sound_rate_to_pitch(delta);
    } else {
        delta = (s32)(0U - sound_rate_to_pitch(-delta));
    }
    state->sweepAdd7C[index] = (u32)delta << 16;
    command->word[0] = 0;   /* a plain WAIT: no key-off, random or sample flags */
    return sound_macro_wait((MacroState *)state, command);
}

#pragma pack(1)
typedef struct SynthVoicePanView { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x10]; u32 panning38; u32 panDelta3C; u8 unknown40[0x68]; u32 panTimeA8; u32 panTargetAC; } SynthVoicePanView;
#pragma pack(0)
/* PANNING: pans from byte 1 (0..127, 64 centre) by word[1]'s low byte (signed) over bytes 2-3 milliseconds; the
 * synth moves it each tick. */
u8 sound_macro_panning(SynthVoicePanView *voice, MacroCommand *command)
{
    u32 unused0, unused1;
    u32 time;
    s8 offset;
    voice->flags24 |= VOICE_PAN_SWEEP;
    time = (command->word[0] >> 16) & 65535;
    voice->panTimeA8 = time;
    sound_ms_to_time(&voice->panTimeA8);
    offset = command->word[1] & 0xFF;
    voice->panning38 = ((command->word[0] >> 8) & 255) << 16;
    voice->panTargetAC = ((u32)offset << 16) + voice->panning38;
    if (voice->panTimeA8 != 0) {
        voice->panDelta3C = ((u32)offset << 16) / time;
    } else {
        voice->panDelta3C = (u32)offset << 16;
    }
    return 0;
}

/* PIANO_PAN: pans by the note, like a piano's keys: byte 3 (the pan at the centre key) plus (note - byte 2, the
 * centre key) times byte 1 (signed) / 128, clamped to 0..127. */
u8 sound_macro_piano_pan(MacroStateAddAge *state, MacroCommand *command)
{
    s32 pan;
    s8 gain;
    pan = ((u32)state->curNote50 << 16) - (((command->word[0] >> 16) & 0xFF) << 16);
    gain = command->word[0] >> 8;
    pan = (s32)((u32)pan * gain);
    pan >>= 7;
    pan += (u8)(command->word[0] >> 24) << 16;
    pan = pan < 0 ? 0 : (pan > 0x7F0000 ? 0x7F0000 : pan);
    state->panning38 = pan;
    return 0;
}

/* Maps a 16.16 volume (0..127) through a curve: a table of the project (id `curve`, 0: none) of 128 bytes, read with
 * linear interpolation. Without the table the volume is returned as it is. */
u32 sound_macro_apply_curve(u32 volume, u16 curve)
{
    u8 *table;
    u32 high;
    u32 low;
    u32 difference;
    if (curve != 0) {
        if ((table = sound_find_table(curve)) != 0) {
            high = volume >> 16;
            low = volume & 0xFFFF;
            if (high < 127) {
                difference = low * (table[high + 1] - table[high]);
                volume = difference + (table[high] << 16);
            } else {
                volume = table[high] << 16;
            }
        }
    }
    return volume;
}

#pragma pack(1)
typedef struct MacroStateScaleVolume { u8 unknown00[0x30]; u32 volume30; u32 orgVolume34; } MacroStateScaleVolume;
#pragma pack()
/* SCALE_VOLUME: the volume becomes the volume (with word[1] byte 1 set: the volume it started with) times byte 1
 * / 128, plus byte 2, at most 127, through the curve of byte 3 and word[1]'s low byte (sound_macro_apply_curve). */
u8 sound_macro_scale_volume(MacroStateScaleVolume *state, MacroCommand *command)
{
    u16 curve;
    if ((u8)(command->word[1] >> 8) == 0) {
        state->volume30 = ((u8)(command->word[0] >> 8) * state->volume30) >> 7;
    } else {
        state->volume30 = ((u8)(command->word[0] >> 8) * state->orgVolume34) >> 7;
    }
    state->volume30 += (u8)(command->word[0] >> 16) << 16;
    if (state->volume30 > 0x7F0000) {
        state->volume30 = 0x7F0000;
    }
    curve = (u8)command->word[1];
    curve |= (u8)(command->word[0] >> 24) << 8;
    state->volume30 = sound_macro_apply_curve(state->volume30, curve);
    return 0;
}

#pragma pack(1)
typedef struct MacroStateStartEnvelope { u8 unknown00[0x24]; u32 flags24; u8 unknown28[8]; u32 volume30; u8 unknown34[0x50]; s32 envDelta84; u32 envTarget88; u8 unknown8C[0x28]; u32 envTimeB4; } MacroStateStartEnvelope;
#pragma pack()
#define ENVELOPE_VOICE ((MacroStateStartEnvelope *)state)
/* Starts a volume envelope from `start` to a target, over word[1]'s top half in ticks or milliseconds (word[1]
 * byte 1 bit 0): the target is the current volume times byte 1 / 128 plus byte 2, at most 127, through the curve of
 * byte 3 and word[1]'s low byte. The synth moves the volume each tick (envDelta per millisecond). */
u8 sound_macro_start_envelope(MacroState *state, MacroCommand *command, u32 start)
{
    u32 target;
    s32 milliseconds;
    u16 curve;
    ENVELOPE_VOICE->envTimeB4 = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&ENVELOPE_VOICE->envTimeB4);
    } else {
        sound_ticks_to_time(&ENVELOPE_VOICE->envTimeB4, state);
    }
    milliseconds = sound_time_to_ms(ENVELOPE_VOICE->envTimeB4);
    if (milliseconds == 0) milliseconds = 1;
    target = ((u8)(command->word[0] >> 8) * ENVELOPE_VOICE->volume30) >> 7;
    target += (u8)(command->word[0] >> 16) << 16;
    if (target > 0x7F0000) target = 0x7F0000;
    curve = (u8)command->word[1];
    curve |= (u8)(command->word[0] >> 24) << 8;
    target = sound_macro_apply_curve(target, curve);
    ENVELOPE_VOICE->envTarget88 = target;
    ENVELOPE_VOICE->envDelta84 = (s32)(target - start) / milliseconds;
    ENVELOPE_VOICE->volume30 = start;
    ENVELOPE_VOICE->flags24 |= VOICE_VOLUME_SWEEP;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateEnvelope { u8 unknown00[0x30]; u32 volume30; u8 unknown34[0x2C]; u32 id60; } MacroStateEnvelope;
#pragma pack(0)
/* ENVELOPE: a volume envelope from the current volume. */
u8 sound_macro_envelope(MacroStateEnvelope *state, MacroCommand *command)
{
    return sound_macro_start_envelope((MacroState *)state, command, state->volume30);
}

/* FADE_IN: a volume envelope from silence. */
u8 sound_macro_fade_in(MacroState *state, MacroCommand *command)
{
    return sound_macro_start_envelope(state, command, 0);
}

/* RANDOM_NOTE: plays a random note (as SET_NOTE, without its wait) between byte 1 and byte 3, or with word[1] byte 1
 * set, from byte 1 below the current note to byte 3 above it; the detune is byte 2, or random (-100..100 cents) when
 * word[1]'s low byte is set. */
u8 sound_macro_random_note(MacroStateSetNote *state, MacroCommand *command)
{
    u8 low;
    u8 high;
    u8 temporary;
    s16 lower;
    s16 upper;
    u8 detune;
    if ((u8)(command->word[1] >> 8) == 0) {
        low = command->word[0] >> 8;
        high = command->word[0] >> 24;
        if (low > high) {
            temporary = low;
            low = high;
            high = temporary;
        }
    } else {
        lower = state->curNote50 - (u8)(command->word[0] >> 8);
        upper = state->curNote50 + (u8)(command->word[0] >> 24);
        low = lower < 0 ? 0 : lower > 127 ? 127 : (u8)lower;
        high = upper < 0 ? 0 : upper > 127 ? 127 : (u8)upper;
    }
    if (command->word[1] & 0xFF) {
        detune = (sound_random() % 201) - 100;
    } else {
        detune = command->word[0] >> 16;
    }
    command->word[0] = ((low + (sound_random() % ((high - low) + 1))) * 256) | MACRO_SET_NOTE | (detune << 16);
    command->word[1] = 0;
    sound_macro_set_note(state, command);
    return 0;
}

#pragma pack(1)
typedef struct MacroStateTrackPitchWheel { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateTrackPitchWheel;
#pragma pack(0)
typedef struct MacroCommandTrackPitchWheel { u32 word[2]; } MacroCommandTrackPitchWheel;
/* TRACK_PITCH_WHEEL: the pitch wheel bends this voice. */
u8 sound_macro_track_pitch_wheel(MacroStateTrackPitchWheel *state, MacroCommandTrackPitchWheel *command)
{
    state->flags24 |= VOICE_TRACK_PITCH_WHEEL;
    return 0;
}

#pragma pack(1)
typedef struct MacroStateSetReverbSend { u8 unknown00[0x1C]; u32 trapAddr1C; u32 trapCurAddr20; u32 flags24; u32 age28; u8 unknown2C[0x3E]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x2D]; u8 revVolScale99; u8 revVolOffset9A; } MacroStateSetReverbSend;
#pragma pack(0)
typedef struct MacroCommandSetReverbSend { u32 word[2]; } MacroCommandSetReverbSend;
/* SET_REVERB_SEND: the reverb send's scale (byte 1) and offset (byte 2). */
u8 sound_macro_set_reverb_send(MacroStateSetReverbSend *state, MacroCommandSetReverbSend *command)
{
    state->revVolScale99 = command->word[0] >> 8;
    state->revVolOffset9A = command->word[0] >> 16;
    return 0;
}

typedef struct MacroStateSelectVolume MacroStateSelectVolume;
typedef struct MacroInput MacroInput;
typedef struct MacroCommandSelectVolume { u32 word[2]; } MacroCommandSelectVolume;
/* One of the voice's inputs (controllers.c's ControlInput): up to four sources, each a controller, how it combines
 * with the sources before it (0 set, 1 add, 2 multiply) and its scale (8.8). 18 bytes. */
#pragma pack(1)
typedef struct MacroInputSource { u8 controller; u8 combination; s16 scale; } MacroInputSource;
struct MacroInput { MacroInputSource entries[4]; u8 count; u8 unknown11; };
struct MacroStateSelectVolume { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x9C]; MacroInput inputsC4[SYNTH_INPUT_COUNT]; };
#pragma pack()
/* The SELECT_* steps add a source to one of the voice's inputs (`control`): the controller of byte 1 (a macro's
 * number, sound_macro_map_controller), scaled by bytes 2-3 (signed percent), combined as word[1]'s low byte says.
 * The input's first SELECT since the voice started (its VOICE_INPUT_* `flag` clear), or a source that sets, starts
 * the input's mix afresh, replacing its default (sound_synth_init_controller_inputs). At most four sources. */
void sound_macro_add_input_source(MacroStateSelectVolume *state, MacroInput *control,
                  MacroCommandSelectVolume *command, u32 flag)
{
    u8 index;
    u8 controller;
    u8 combination;
    combination = command->word[1] & 0xFF;
    if (!(state->flags24 & flag) || combination == MACRO_COMBINE_SET) {
        state->flags24 |= flag;
        control->count = 0;
    }
    if (control->count < 4) {
        index = control->count++;
        controller = sound_macro_map_controller(command->word[0] >> 8);
        control->entries[index].controller = controller;
        control->entries[index].combination = combination;
        control->entries[index].scale = ((s32)(s16)(command->word[0] >> 16) * 256) / 100;
    }
}

/* SELECT_VOLUME .. SELECT_DOPPLER: a source for the volume, pan, pitch-bend, modulation, pedal, portamento, reverb,
 * span or doppler input. */
u8 sound_macro_select_volume(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_VOLUME],
                  command, VOICE_INPUT_VOLUME);
    return 0;
}

u8 sound_macro_select_pan(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_PAN],
                  command, VOICE_INPUT_PAN);
    return 0;
}

u8 sound_macro_select_pitch_bend(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_PITCH_BEND],
                  command, VOICE_INPUT_PITCH_BEND);
    return 0;
}

u8 sound_macro_select_modulation(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_MODULATION],
                  command, VOICE_INPUT_MODULATION);
    return 0;
}

u8 sound_macro_select_pedal(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_PEDAL],
                  command, VOICE_INPUT_PEDAL);
    return 0;
}

u8 sound_macro_select_portamento(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_PORTAMENTO],
                  command, VOICE_INPUT_PORTAMENTO);
    return 0;
}

u8 sound_macro_select_reverb(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_REVERB],
                  command, VOICE_INPUT_REVERB);
    return 0;
}

u8 sound_macro_select_span(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_SPAN],
                  command, VOICE_INPUT_SPAN);
    return 0;
}

u8 sound_macro_select_doppler(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, &state->inputsC4[SYNTH_INPUT_DOPPLER],
                  command, VOICE_INPUT_DOPPLER);
    return 0;
}

#pragma pack(1)
typedef struct MacroStatePortamento { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x22]; u8 channel4A; u8 set4B; u8 unknown4C[0x44]; u32 portDuration90; u8 unknown94[4]; u8 portType98; } MacroStatePortamento;
#pragma pack()
/* PORTAMENTO: byte 1 0 turns portamento off, 1 on (both take it from the portamento controller: the macro decides
 * now); byte 2 is the glide's mode, word[1]'s top half its time in ticks or milliseconds (word[1] byte 1 bit 0). */
u8 sound_macro_portamento(MacroStatePortamento *state, MacroCommand *command)
{
    u32 time;
    state->portType98 = command->word[0] >> 16;
    time = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&time);
    } else {
        sound_ticks_to_time(&time, (MacroState *)state);
    }
    state->portDuration90 = time;
    switch ((u8)(command->word[0] >> 8)) {
    case 0:
        state->flags24 &= ~VOICE_PORTAMENTO;
        state->flags24 |= VOICE_PORTAMENTO_BY_MACRO;
        if (state->channel4A != NO_CHANNEL) sound_midi_set_portamento_state(state->channel4A, state->set4B, 0);
        break;
    case 1:
        if (!(state->flags24 & VOICE_PORTAMENTO)) sound_synth_start_portamento(state);
        state->flags24 |= VOICE_PORTAMENTO_BY_MACRO | VOICE_PORTAMENTO;
        break;
    }
    return 0;
}

#pragma pack(1)
typedef struct SynthVoiceVariablesView { u8 unknown000[0x180]; s16 localVars180[16]; } SynthVoiceVariablesView;
#pragma pack(0)
/* The variables of the arithmetic and comparison steps: with isController, a controller (a macro's number,
 * sound_macro_get_controller); else variable `index` & 31: 0..15 the voice's own, 16..31 the globals
 * (sound_global_variables, which SEND_FLAG sets too).
 * N64 layout: the globals are reached as 32 bytes past sound_seq_current_index, where the N64's data puts them;
 * this needs that layout (wave 3: name sound_global_variables, as sound_macro_send_flag does). */
s16 sound_macro_get_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index)
{
    s32 value;
    if (isController) {
        return sound_macro_get_controller((VoiceState *)voice, index);
    }
    index &= 31;
    value = index < 16 ? voice->localVars180[index] : (&sound_seq_current_index[16])[index - 16];
    return value;
}

/* Writes a variable (as sound_macro_get_variable reads it). */
void sound_macro_set_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index, s16 value)
{
    if (isController) {
        sound_macro_set_controller((VoiceState *)voice, index, value);
    } else {
        index &= 31;
        if (index < 16) {
            voice->localVars180[index] = value;
        } else {
            (&sound_seq_current_index[16])[index - 16] = value;
        }
    }
}

/* ADD_VARIABLES .. DIVIDE_VARIABLES, ADD_TO_VARIABLE: the variable of bytes 1-2 (byte 1: a controller?, byte 2: its
 * number) becomes the variable of byte 3 and word[1]'s low byte, plus, minus, times or divided by (0 for a division by
 * 0) the variable of word[1]'s bytes 1-2, or for ADD_TO_VARIABLE the constant word[1] bits 8..23; clamped to 16 bits. */
u8 sound_macro_variable_arithmetic(SynthVoiceVariablesView *voice, MacroCommand *command, u8 operation)
{
    s16 left;
    s16 right;
    s32 result;

    left = sound_macro_get_variable(voice, (command->word[0] >> 24) & 255, command->word[1] & 255);
    if (operation == MACRO_ARITH_ADD_IMMEDIATE) {
        right = (s16)(command->word[1] >> 8);
    } else {
        right = sound_macro_get_variable(voice, (command->word[1] >> 8) & 255, (command->word[1] >> 16) & 255);
    }
    switch (operation) {
    case MACRO_ARITH_ADD_IMMEDIATE:
    case MACRO_ARITH_ADD: result = left + right; break;
    case MACRO_ARITH_SUBTRACT: result = left - right; break;
    case MACRO_ARITH_MULTIPLY: result = left * right; break;
    case MACRO_ARITH_DIVIDE: result = right != 0 ? left / right : 0; break;
    }
    sound_macro_set_variable(voice, (command->word[0] >> 8) & 255, (command->word[0] >> 16) & 255,
                 result < -32768 ? -32768 : result > 32767 ? 32767 : result);
    return 0;
}

/* SET_VARIABLE: the variable of bytes 1-2 becomes word[1]'s low half. */
u8 sound_macro_set_variable_immediate(MacroState *state, MacroCommand *command)
{
    sound_macro_set_variable((SynthVoiceVariablesView *)state, command->word[0] >> 8, command->word[0] >> 16,
                  (s16)(command->word[1] & 0xFFFF));
    return 0;
}

#pragma pack(1)
typedef struct MacroStateCompare { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; } MacroStateCompare;
#pragma pack(0)
/* IF_EQUAL, IF_LESS: compares the variable of bytes 1-2 with the variable of byte 3 and word[1]'s low byte; when it
 * holds (or with word[1] byte 1 set, when it doesn't) jumps to the step of word[1]'s top half in this macro. */
u8 sound_macro_compare(MacroStateCompare *voice, MacroCommand *command, u8 comparison)
{
    s16 left;
    s16 right;
    u8 condition;
    left = sound_macro_get_variable((SynthVoiceVariablesView *)voice, command->word[0] >> 8, command->word[0] >> 16);
    right = sound_macro_get_variable((SynthVoiceVariablesView *)voice, command->word[0] >> 24, command->word[1] & 0xFF);
    switch (comparison) {
    case MACRO_COMPARE_EQUAL: condition = left == right; break;
    case MACRO_COMPARE_LESS: condition = left < right; break;
    }
    if ((command->word[1] >> 8) & 255) {
        condition = !condition;
    }
    if (condition) {
        voice->curAddr04 = PC_N64W(((command->word[1] >> 16) & 65535) + N64P(voice->addr00));
    }
    return 0;
}

#pragma pack(1)
typedef struct SynthVoiceMacroView { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; u8 unknown08[0x14]; enum n64p_MacroCommand trapAddr1C; enum n64p_MacroCommand trapCurAddr20; u32 flags24; u8 unknown28[7]; u8 track2F; u32 volume30; u32 orgVolume34; u32 panning38; u8 unknown3C[4]; u32 sweepOff40[2]; u8 sweepNum48[2]; u8 channel4A; u8 set4B; u8 unknown4C[2]; u16 orgNote4E; u8 unknown50[2]; u8 setupVol52; u8 setupPan53; u8 setupChannel54; u8 setupSet55; u8 setupTrack56; u8 unknown57[5]; u32 sampleInfo5C; u32 id60; u8 unknown64[4]; u16 loop68; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x20]; u32 portTime8C; u32 portDuration90; u8 unknown94[4]; u8 portType98; u8 revVolScale99; u8 revVolOffset9A; u8 unknown9B; u32 wait9C; u32 waitTimeA0; u32 timeA4; u8 unknownA8[0x10]; u16 pbLastB8; u8 unknownBA[4]; u8 vGroupBE; u8 setupVGroupBF; u8 unknownC0; u8 lastNoteC1; u16 curOutputVolumeC2; u8 unknownC4[0xA8]; u32 lfoPeriod16C; s16 lfoValue170; u8 unknown172[6]; u32 lfoPeriod178; s16 lfoValue17C; u8 unknown17E[2]; s16 localVars180[16]; } SynthVoiceMacroView;
#pragma pack(0)
/* Runs a voice's macro for one tick (synth.c calls it for every active voice). A just-started voice is first set up
 * from what sound_macro_start stored: default inputs, volume, pan, channel, track, volume group, last key, pitch-wheel
 * range, reverb send; no sweeps, LFOs, trap or variables; its clock at 0. Then a pending key-off trap is taken, and
 * if the voice isn't waiting, steps run until a handler says stop (MACRO_STEPS_PER_RUN at most). A waiting voice
 * only advances its clock and ends its wait at key off, at the sample's end or when it's due. */
void sound_macro_run(SynthVoiceMacroView *voice)
{
    static MacroCommand macro_step_copy;   /* the step being run (a copy: some handlers rewrite it) */
    u8 i;
    u8 lastNote;
    u8 stop;

    if (voice->flags24 & (VOICE_HW_BUSY_AT_START | VOICE_STARTING)) {
        if (voice->flags24 & VOICE_HW_BUSY_AT_START) {
            voice->flags24 &= ~VOICE_HW_BUSY_AT_START;
            sound_hw_break_voice(voice->id60 & 0xFF);
        }
        /* 0x40: nothing in the library sets it; with it the voice is freed instead of starting */
        if (!(voice->flags24 & 0x40)) {
            sound_synth_init_controller_inputs((struct VoiceControls *)voice);
            voice->panning38 = voice->setupPan53 << 16;
            voice->volume30 = voice->setupVol52 << 16;
            voice->orgVolume34 = voice->volume30;
            voice->channel4A = voice->setupChannel54;
            voice->set4B = voice->setupSet55;
            voice->track2F = voice->setupTrack56;
            if ((lastNote = sound_midi_get_last_key(voice->channel4A, voice->set4B)) != NO_LAST_KEY) {
                voice->lastNoteC1 = lastNote;
            } else {
                voice->lastNoteC1 = voice->orgNote4E;
            }
            sound_midi_set_last_key(voice->channel4A, voice->set4B, voice->orgNote4E);
            sound_synth_claim_channel((struct VoiceState *)voice);
            voice->vGroupBE = voice->setupVGroupBF;
            voice->portTime8C = 0;
            voice->portDuration90 = 100;
            voice->portType98 = 0;
            voice->pbLowerKeyRange6A = 2;
            voice->pbUpperKeyRange6B = 2;
            voice->revVolScale99 = 128;
            voice->revVolOffset9A = 0;
            voice->loop68 = 0;
            voice->sweepNum48[0] = 0;
            voice->sweepNum48[1] = 0;
            voice->sweepOff40[0] = 0;
            voice->sweepOff40[1] = 0;
            voice->lfoPeriod16C = 0;
            voice->lfoValue170 = 0;
            voice->lfoPeriod178 = 0;
            voice->lfoValue17C = 0;
            voice->trapAddr1C = 0;
            voice->trapCurAddr20 = 0;
            voice->sampleInfo5C = (64 << 24) | 44100;   /* root key 64 at 44,100 Hz until a sample says otherwise */
            voice->pbLastB8 = 0x2000;                   /* the pitch wheel centred */
            voice->curOutputVolumeC2 = 0;
            voice->flags24 &= VOICE_KEY_OFF;            /* keeps only a key off that came before the start */
            for (i = 0; i < 16; i++) {
                voice->localVars180[i] = 0;
            }
            voice->waitTimeA0 = 0;
            voice->timeA4 = 0;
        } else {
            voice->flags24 = 0;
            sound_free_voice((MacroState *)voice);
            return;
        }
    }
    if (voice->trapCurAddr20 != 0 && !(voice->flags24 & VOICE_PEDAL_DOWN) && (voice->flags24 & VOICE_KEY_OFF)) {
        voice->addr00 = voice->trapAddr1C;
        voice->curAddr04 = voice->trapCurAddr20;
        voice->wait9C = 0;
        voice->trapAddr1C = voice->trapCurAddr20 = 0;
    }
    if (voice->wait9C == 0) {
        macro_steps_this_update = 0;
        do {
            if (++macro_steps_this_update > MACRO_STEPS_PER_RUN) {
                break;
            }
            macro_step_copy.word[0] = N64P(voice->curAddr04)->word[0];
            macro_step_copy.word[1] = N64P(voice->curAddr04)->word[1];
            N64_NEXT(voice->curAddr04);
            switch (macro_step_copy.word[0] & MACRO_OPCODE_MASK) {
            case MACRO_END:
                stop = sound_macro_end((MacroState *)voice, &macro_step_copy);
                break;
            case MACRO_RETURN:
                stop = sound_macro_return((struct MacroStateReturn *)voice, &macro_step_copy);
                break;
            case MACRO_SPLIT_KEY:
                stop = sound_macro_split_key((struct MacroStateSplitKey *)voice, &macro_step_copy);
                break;
            case MACRO_SPLIT_VELOCITY:
                stop = sound_macro_split_velocity((struct MacroStateSplitVelocity *)voice, &macro_step_copy);
                break;
            case MACRO_WAIT:
                stop = sound_macro_wait((MacroState *)voice, &macro_step_copy);
                break;
            case MACRO_LOOP:
                stop = sound_macro_loop((struct MacroStateLoop *)voice, &macro_step_copy);
                break;
            case MACRO_GOSUB:
                stop = sound_macro_gosub((struct MacroStateGosub *)voice, &macro_step_copy);
                break;
            case MACRO_WAIT_MS:
                stop = sound_macro_wait_ms((MacroState *)voice, &macro_step_copy);
                break;
            case MACRO_PLAY_MACRO:
                stop = sound_macro_play_macro((struct MacroStatePlayMacro *)voice, &macro_step_copy);
                break;
            case MACRO_SEND_KEY_OFF:
                stop = sound_macro_send_key_off((struct MacroStateSendKeyOff *)voice, &macro_step_copy);
                break;
            case MACRO_SPLIT_MODULATION:
                stop = sound_macro_split_modulation((struct MacroStateSplitModulation *)voice, &macro_step_copy);
                break;
            case MACRO_PIANO_PAN:
                stop = sound_macro_piano_pan((struct MacroStateAddAge *)voice, &macro_step_copy);
                break;
            case MACRO_SET_ADSR:
                stop = sound_macro_set_adsr((struct MacroStateSetAdsr *)voice, &macro_step_copy);
                break;
            case MACRO_SCALE_VOLUME:
                stop = sound_macro_scale_volume((struct MacroStateScaleVolume *)voice, &macro_step_copy);
                break;
            case MACRO_PANNING:
                stop = sound_macro_panning((struct SynthVoicePanView *)voice, &macro_step_copy);
                break;
            case MACRO_ENVELOPE:
                stop = sound_macro_envelope((struct MacroStateEnvelope *)voice, &macro_step_copy);
                break;
            case MACRO_START_SAMPLE:
                stop = sound_macro_start_sample((struct MacroStateStartSample *)voice, &macro_step_copy);
                break;
            case MACRO_STOP_SAMPLE:
                stop = sound_macro_stop_sample((struct MacroStateStopSample *)voice, &macro_step_copy);
                break;
            case MACRO_KEY_OFF:
                stop = sound_macro_key_off((struct MacroStateKeyOff *)voice, (struct MacroCommandKeyOff *)&macro_step_copy);
                break;
            case MACRO_SPLIT_RANDOM:
                stop = sound_macro_split_random((struct MacroStateSplitRandom *)voice, &macro_step_copy);
                break;
            case MACRO_FADE_IN:
                stop = sound_macro_fade_in((MacroState *)voice, &macro_step_copy);
                break;
            case MACRO_RANDOM_NOTE:
                stop = sound_macro_random_note((struct MacroStateSetNote *)voice, &macro_step_copy);
                break;
            case MACRO_ADD_NOTE:
                stop = sound_macro_add_note((struct MacroStateAddNote *)voice, &macro_step_copy);
                break;
            case MACRO_SET_NOTE:
                stop = sound_macro_set_note((struct MacroStateSetNote *)voice, &macro_step_copy);
                break;
            case MACRO_LAST_NOTE:
                stop = sound_macro_last_note((struct MacroStateLastNote *)voice, &macro_step_copy);
                break;
            case MACRO_PORTAMENTO:
                stop = sound_macro_portamento((struct MacroStatePortamento *)voice, &macro_step_copy);
                break;
            case MACRO_VIBRATO:
                stop = sound_macro_vibrato((struct MacroStateVibrato *)voice, &macro_step_copy);
                break;
            case MACRO_PITCH_SWEEP_1:
                stop = sound_macro_pitch_sweep((struct MacroStatePitchSweep *)voice, &macro_step_copy, 0);
                break;
            case MACRO_PITCH_SWEEP_2:
                stop = sound_macro_pitch_sweep((struct MacroStatePitchSweep *)voice, &macro_step_copy, 1);
                break;
            case MACRO_SET_PITCH:
                stop = sound_macro_set_pitch((struct MacroStateSetPitch *)voice, (struct MacroCommandSetPitch *)&macro_step_copy);
                break;
            case MACRO_TRAP_KEY_OFF:
                stop = sound_macro_trap_key_off((struct MacroStateTrapKeyOff *)voice, &macro_step_copy);
                break;
            case MACRO_UNTRAP_KEY_OFF:
                stop = sound_macro_untrap_key_off((struct MacroStateUntrapKeyOff *)voice, (struct MacroCommandUntrapKeyOff *)&macro_step_copy);
                break;
            case MACRO_ADD_AGE:
                stop = sound_macro_add_age((struct MacroStateAddAge *)voice, (struct MacroCommandAddAge *)&macro_step_copy);
                break;
            case MACRO_SET_AGE:
                stop = sound_macro_set_age((struct MacroStateSetAge *)voice, (struct MacroCommandSetAge *)&macro_step_copy);
                break;
            case MACRO_SEND_FLAG:
                stop = sound_macro_send_flag((struct MacroStateSendFlag *)voice, (struct MacroCommandSendFlag *)&macro_step_copy);
                break;
            case MACRO_SET_PITCH_WHEEL_RANGE:
                stop = sound_macro_set_pitch_wheel_range((struct MacroStateSetPitchWheelRange *)voice, (struct MacroCommandSetPitchWheelRange *)&macro_step_copy);
                break;
            case MACRO_SET_REVERB_SEND:
                stop = sound_macro_set_reverb_send((struct MacroStateSetReverbSend *)voice, (struct MacroCommandSetReverbSend *)&macro_step_copy);
                break;
            case MACRO_TRACK_PITCH_WHEEL:
                stop = sound_macro_track_pitch_wheel((struct MacroStateTrackPitchWheel *)voice, (struct MacroCommandTrackPitchWheel *)&macro_step_copy);
                break;
            case MACRO_SET_PRIORITY:
                stop = sound_macro_set_priority((MacroState *)voice, &macro_step_copy);
                break;
            case MACRO_ADD_PRIORITY:
                stop = sound_macro_add_priority((struct MacroStateAddPriority *)voice, &macro_step_copy);
                break;
            case MACRO_SET_AGE_SPEED:
                stop = sound_macro_set_age_speed((struct MacroStateSetAgeSpeed *)voice, (struct MacroCommandSetAgeSpeed *)&macro_step_copy);
                break;
            case MACRO_SET_AGE_BY_VELOCITY:
                stop = sound_macro_set_age_by_velocity((struct MacroStateSetAgeByVelocity *)voice, (struct MacroCommandSetAgeByVelocity *)&macro_step_copy);
                break;
            case MACRO_SELECT_VOLUME:
                stop = sound_macro_select_volume((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_PAN:
                stop = sound_macro_select_pan((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_PITCH_BEND:
                stop = sound_macro_select_pitch_bend((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_MODULATION:
                stop = sound_macro_select_modulation((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_PEDAL:
                stop = sound_macro_select_pedal((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_PORTAMENTO:
                stop = sound_macro_select_portamento((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_REVERB:
                stop = sound_macro_select_reverb((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_SPAN:
                stop = sound_macro_select_span((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SELECT_DOPPLER:
                stop = sound_macro_select_doppler((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&macro_step_copy);
                break;
            case MACRO_SETUP_LFO:
                stop = sound_macro_setup_lfo((struct MacroStateSetupLfo *)voice, (struct MacroCommandSetupLfo *)&macro_step_copy);
                break;
            case MACRO_ADD_VARIABLES:
                stop = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &macro_step_copy, MACRO_ARITH_ADD);
                break;
            case MACRO_SUBTRACT_VARIABLES:
                stop = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &macro_step_copy, MACRO_ARITH_SUBTRACT);
                break;
            case MACRO_MULTIPLY_VARIABLES:
                stop = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &macro_step_copy, MACRO_ARITH_MULTIPLY);
                break;
            case MACRO_DIVIDE_VARIABLES:
                stop = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &macro_step_copy, MACRO_ARITH_DIVIDE);
                break;
            case MACRO_ADD_TO_VARIABLE:
                stop = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &macro_step_copy, MACRO_ARITH_ADD_IMMEDIATE);
                break;
            case MACRO_SET_VARIABLE:
                stop = sound_macro_set_variable_immediate((MacroState *)voice, &macro_step_copy);
                break;
            case MACRO_IF_EQUAL:
                stop = sound_macro_compare((struct MacroStateCompare *)voice, &macro_step_copy, MACRO_COMPARE_EQUAL);
                break;
            case MACRO_IF_LESS:
                stop = sound_macro_compare((struct MacroStateCompare *)voice, &macro_step_copy, MACRO_COMPARE_LESS);
                break;
            default:
                stop = 0;
            }
        } while (!stop);
        voice->timeA4 += sound_tick_step;
    } else {
        voice->timeA4 += sound_tick_step;
        if ((voice->flags24 & VOICE_WAIT_KEY_OFF) && !(voice->flags24 & VOICE_PEDAL_DOWN) && (voice->flags24 & VOICE_KEY_OFF)) {
            voice->flags24 &= ~VOICE_WAIT_KEY_OFF;
            voice->waitTimeA0 = voice->timeA4;
            voice->wait9C = 0;
        }
        if ((voice->flags24 & VOICE_WAIT_SAMPLE_END) && !sound_hw_is_voice_active(voice->id60 & 0xFF)) {
            voice->flags24 &= ~VOICE_WAIT_SAMPLE_END;
            voice->wait9C = 0;
            voice->waitTimeA0 = voice->timeA4;
        }
        if (voice->wait9C != MACRO_WAIT_FOREVER && voice->timeA4 >= voice->wait9C) {
            voice->waitTimeA0 = voice->wait9C;
            voice->wait9C = 0;
        }
    }
}

#pragma pack(1)
typedef struct MacroStateStart { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; enum n64p_MacroCommand retAddr08; u8 unknown0C[4]; u32 next10; u32 previous14; u8 unknown18[12]; u32 flags24; u32 age28; u16 ageSpeed2C; u8 unknown2E[0x1E]; u8 fxFlag4C; u8 unknown4D; u16 orgNote4E; u16 curNote50; u8 setupVol52; u8 setupPan53; u8 setupChannel54; u8 setupSet55; u8 setupTrack56; u8 unknown57[9]; u32 id60; u16 macroId64; u8 unknown66[0x36]; u32 wait9C; u8 unknownA0[0x1A]; u16 allocIdBA; u8 unknownBC[3]; u8 setupVGroupBF; u8 curDetuneC0; u8 unknownC1[0xDF]; } MacroStateStart;
#pragma pack()
/* Starts a macro on a voice. packed: the macro's id (bits 31..16), the voice's priority (15..8) and the most voices
 * of this allocation id that may play (7..0); allocation: the allocation id; key: the note, with bit 7 (MACRO_KEY_EFFECT)
 * for an effect, which gets its own controllers (the set VOICE_OWN_SET, reset) instead of channel/set; volume and
 * panning (0..127); offset: the step to start at; track: whose volume applies (255: none); group: the volume group;
 * start: 1 to return a new sound handle for the voice (sound_new_handle), 0 to return its voice id (PLAY_MACRO).
 * The voice is set up here only as far as its first run needs (VOICE_STARTING; sound_macro_run does the rest).
 * Returns VOICE_NONE when the macro is missing or no voice can be had. */
u32 sound_macro_start(u32 packed, u16 allocation, u8 key, u8 volume,
                 u8 panning, u8 channel, u8 set, u16 offset,
                 u16 track, u8 start, u8 group)
{
    MacroStateStart *state;
    u16 macro;
    u8 priority;
    MacroCommand *program;
    int index;
    macro = packed >> 16;
    program = sound_find_macro(macro);
    if (program != 0) {
        priority = packed >> 8;
        index = sound_alloc_voice(priority, packed & 255, allocation, (key & MACRO_KEY_EFFECT) ? 1 : 0);
        if (index != -1) {
            state = (MacroStateStart *)&sound_synth_voices[index];
            sound_unlink_voice((MacroState *)state);
            state->flags24 = (state->flags24 & VOICE_PLAYING) | VOICE_STARTING;
            if (sound_hw_is_voice_active(index)) state->flags24 |= VOICE_HW_BUSY_AT_START;
            state->wait9C = 0;
            if (key & MACRO_KEY_EFFECT) {
                /* an effect voice is its own MIDI channel, in the set VOICE_OWN_SET */
                state->fxFlag4C = 1;
                key &= 127;
                sound_midi_reset_channel(index, VOICE_OWN_SET);
                state->setupChannel54 = index;
                state->setupSet55 = VOICE_OWN_SET;
            } else {
                state->fxFlag4C = 0;
                state->setupChannel54 = channel;
                state->setupSet55 = set;
            }
            state->macroId64 = macro;
            state->allocIdBA = allocation;
            state->age28 = 60000U << 15;
            state->ageSpeed2C = 1024;
            state->addr00 = PC_N64W(program);
            state->curAddr04 = PC_N64W(program + offset);
            state->orgNote4E = key;
            state->curNote50 = key;
            state->curDetuneC0 = 0;
            state->setupVol52 = volume;
            state->setupPan53 = panning;
            state->setupTrack56 = track;
            state->retAddr08 = 0;
            state->next10 = VOICE_NONE;
            state->previous14 = VOICE_NONE;
            state->setupVGroupBF = group;
            state->id60 = (macro << 16) | ((u32)key << 8) | (u32)index;
            sound_set_voice_priority((MacroState *)state, priority);
            if (start) return sound_new_handle(state);
            return state->id60;
        }
    }
    return VOICE_NONE;
}
