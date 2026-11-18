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
 * counterpart. The voice is musyx.h's SynthVoice: the N64's layout after its head of pointers (the macro's place and
 * the voice chain). */
#include "../rom_tu.h"
#include "musyx.h"
#include "sound_sample_record.h"

/* The voice is musyx.h's SynthVoice (sound_synth_voices[i]), whose fields the handlers below read and set; its
 * controller inputs are SYNTH_INPUT_* there. */

#define MACRO_WAIT_FOREVER        0xFFFFFFFFU /* SynthVoice.wait: until key off or the sample's end */
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

/* On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9): an effect voice's legato owner, by voice. PC_MX_LOOP(32) is the run's voices where the
 * N64 clears its 32 (docs/port/AUDIO.md 10). */
#define sound_voice_channel_owners (*(u8 (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_legato)

/* ---- What the file uses from the rest of the library ---- */
struct Envelope;
/* data */
extern u8 sound_midi_channel_owners[8][16];
extern u8 sound_voice_count;
extern u16 sound_global_variables[];
extern u32 sound_tick_step;
/* voices.c */
extern int sound_alloc_voice(u8, u8, u16, u8);
extern void sound_unlink_voice(SynthVoice *);
extern void sound_free_voice(SynthVoice *);
extern void sound_set_voice_priority(SynthVoice *, u8);
extern u32 sound_new_handle(SynthVoice *);
/* synth.c */
extern void sound_synth_copy_controllers(SynthVoice *, SynthVoice *);
extern void sound_synth_start_portamento(SynthVoice *);
/* rate.c */
extern u32 sound_rate_to_pitch(u32);
/* controllers.c */
extern u16 sound_synth_get_modulation_input(SynthVoice *);
extern void sound_synth_init_controller_inputs(SynthVoice *);
extern void sound_midi_reset_channel(u8, u8);
extern void sound_midi_set_last_key(u8, u8, u8);
extern unsigned char sound_midi_get_last_key(unsigned char channel, unsigned char set);
extern void sound_midi_set_portamento_state(u8, u8, u8);
extern u8 sound_macro_map_controller(u8);
extern u16 sound_macro_get_controller(SynthVoice *voice, u8 controller);
extern void sound_macro_set_controller(SynthVoice *voice, u8 controller, s16 value);
/* the registries (registry.c) */
extern MacroCommand *sound_find_macro(u16);
extern int sound_find_sample(u16, SoundSampleInfo *);
extern void *sound_find_table(u16);
/* the hardware layer (hardware.c) */
extern u8 sound_hw_is_voice_active(int);
extern void sound_hw_set_voice_sample(u32, SoundSampleInfo *, u8);
extern void sound_hw_stop_sample(u32, u8);
extern void sound_hw_set_envelope(u32, const struct Envelope *);
extern void sound_hw_break_voice(int);
/* time and randomness (util.c) */
extern u16 sound_random(void);
extern void sound_ms_to_time(u32 *);
extern void sound_ticks_to_time(u32 *, SynthVoice *);
extern u32 sound_time_to_ms(u32);
/* this file, used before its definition */
extern u32 sound_macro_start(u32 packed, u16 allocation, u8 key, u8 volume, u8 panning, u8 channel, u8 set, u16 offset, u16 track, u8 start, u8 group);

/* Keys off the voice `id` (a voice id, not a handle): flags it VOICE_KEY_OFF, which ends a WAIT for key off, jumps to
 * a key-off trap or ends a loop at the next run. 0, or -1 if no such voice plays (the voice was reused). */
int sound_synth_key_off_voice(u32 id)
{
    SynthVoice *voice;
    if (id != VOICE_NONE) {
        voice = &sound_synth_voices[VOICE_INDEX(id)];
        if (voice->id == id) {
            voice->flags |= VOICE_KEY_OFF;
            return 0;
        }
    }
    return -1;
}

/* A channel's legato owner: the voice last started on a MIDI channel owns it (sound_midi_channel_owners[set][channel],
 * or for an effect voice, which is its own channel, sound_voice_channel_owners[voice]); only the owner records the
 * notes it plays as the channel's last key. */

/* Whether the voice owns its MIDI channel (1) or not, or has none (0). */
int sound_synth_owns_channel(SynthVoice *state)
{
    u8 index;
    if (state->id != VOICE_NONE && state->channel != NO_CHANNEL) {
        index = state->id & 0xFF;
        if (state->set == VOICE_OWN_SET) {
            if (sound_voice_channel_owners[index] == index) return 1;
        } else {
            if (sound_midi_channel_owners[state->set][state->channel] == index) return 1;
        }
    }
    return 0;
}

/* Makes the voice its MIDI channel's owner (when its macro starts). */
void sound_synth_claim_channel(SynthVoice *voice)
{
    u8 index;
    if (voice->id != VOICE_NONE && voice->channel != NO_CHANNEL) {
        index = voice->id & 0xFF;
        if (voice->set == VOICE_OWN_SET) {
            sound_voice_channel_owners[index] = index;
        } else {
            sound_midi_channel_owners[voice->set][voice->channel] = index;
        }
    }
}

/* Gives up the voice's MIDI channel if the voice still owns it (255: no owner). */
void sound_synth_release_channel(SynthVoice *voice)
{
    u8 index;
    if (voice->id != VOICE_NONE && voice->channel != NO_CHANNEL) {
        index = voice->id & 0xFF;
        if (voice->set == VOICE_OWN_SET) {
            if (sound_voice_channel_owners[index] == index) sound_voice_channel_owners[index] = 255;
        } else {
            if (sound_midi_channel_owners[voice->set][voice->channel] == index) sound_midi_channel_owners[voice->set][voice->channel] = 255;
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


/* The handlers. Each takes the voice and the step, and returns nonzero when the voice stops
 * running steps for this tick (it waits, or it ended); sound_macro_run dispatches to them. The step's parameters
 * are its bytes: "byte 1" is (word[0] >> 8) & 0xFF, "byte 2" (word[0] >> 16) & 0xFF, "byte 3" word[0] >> 24, and
 * word[1] holds the rest (often a 16-bit value in its top half). */

/* WAIT (and the waits other steps end with): waits word[1]'s top 16 bits in ticks, or in milliseconds when byte 1
 * of word[1] has bit 0 (WAIT_MS sets it); 0xFFFF waits forever. Byte 1 bit 0: the wait also ends at key off (and is
 * skipped if the key is already off); byte 3 bit 0: it also ends when the sample does (and is skipped if no sample
 * plays); byte 2 bit 0: a random time below the given one. A tick wait counts from when the last wait ended, so a
 * macro keeps its rhythm. Returns whether the voice now waits. */
u8 sound_macro_wait(SynthVoice *state, MacroCommand *command)
{
    u32 time;
    u8 milliseconds;
    time = (u16)(command->word[1] >> 16);
    if (time != 0) {
        if ((u8)(command->word[0] >> 8) & 1) {
            if (state->flags & VOICE_KEY_OFF) return 0;
            state->flags |= VOICE_WAIT_KEY_OFF;
        } else {
            state->flags &= ~VOICE_WAIT_KEY_OFF;
        }
        if ((u8)(command->word[0] >> 24) & 1) {
            if (!(state->flags & VOICE_START_SAMPLE) && !sound_hw_is_voice_active(VOICE_INDEX(state->id))) return 0;
            state->flags |= VOICE_WAIT_SAMPLE_END;
        } else {
            state->flags &= ~VOICE_WAIT_SAMPLE_END;
        }
        if ((u8)(command->word[0] >> 16) & 1) time = sound_random() % time;
        if (time != 0xFFFF) {
            milliseconds = (u8)(command->word[1] >> 8) & 1;
            if (milliseconds) sound_ms_to_time(&time);
            else sound_ticks_to_time(&time, state);
            if (milliseconds) {
                state->wait = state->time + time;
            } else {
                state->wait = state->waitTime + time;
                if (state->wait <= state->time) {
                    /* already due (the macro is behind): no wait, and the next one counts from here */
                    state->waitTime = state->wait;
                    state->wait = 0;
                }
            }
        } else {
            state->wait = MACRO_WAIT_FOREVER;
        }
        return state->wait != 0;
    }
    return 0;
}

/* WAIT_MS: WAIT with the time in milliseconds (sets the milliseconds flag in the step's copy).
 * PC: the step's words are in host order (port/assets/musyx.c swaps the pool's macros), so the N64's byte 6 is
 * not bits 15..8 of word[1], the milliseconds flag sound_macro_wait tests. The same store, on the word. */
u8 sound_macro_wait_ms(SynthVoice *state, MacroCommand *command)
{
    command->word[1] = (command->word[1] & ~0xFF00U) | 0x100U;
    return sound_macro_wait(state, command);
}

/* END: the macro is over; the voice leaves its handle's chain and is freed. */
u8 sound_macro_end(SynthVoice *state, MacroCommand *command)
{
    sound_unlink_voice(state);
    sound_free_voice(state);
    return 1;
}

/* RETURN: back to where the last GOSUB left; with byte 1 zero, or no GOSUB to return from, it ends the macro as END
 * does. */
u8 sound_macro_return(SynthVoice *state, MacroCommand *command)
{
    if (((command->word[0] >> 8) & 0xFF) == 0 || state->head.retAddr == 0) {
        return sound_macro_end(state, command);
    }
    state->head.addr = state->head.retAddr;
    state->head.curAddr = state->head.retCurAddr;
    return 0;
}

/* The SPLIT_* steps jump to another macro (bytes 2-3: its id; word[1]'s low half: the step to start at) when a value
 * reaches byte 1, else go on. SPLIT_KEY: when the note does. */
u8 sound_macro_split_key(SynthVoice *state, MacroCommand *command)
{
    MacroCommand *address;
    if (state->curNote >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->head.addr = address;
            state->head.curAddr = address + (command->word[1] & 0xFFFF);
        }
    }
    return 0;
}

/* SPLIT_VELOCITY: jumps when the volume (the note's velocity, 0..127) reaches byte 1. */
u8 sound_macro_split_velocity(SynthVoice *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((state->volume >> 16) >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->head.addr = address;
            state->head.curAddr = address + (command->word[1] & 0xFFFF);
        }
    }
    return 0;
}

/* SPLIT_MODULATION: jumps when the modulation input (0..127) reaches byte 1; never without a MIDI channel. */
u8 sound_macro_split_modulation(SynthVoice *state, MacroCommand *command)
{
    MacroCommand *address;
    u8 modulation;
    if (state->channel != NO_CHANNEL) {
        modulation = sound_synth_get_modulation_input(state) >> 7;
        if (modulation >= (u8)(command->word[0] >> 8)) {
            if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
                state->head.addr = address;
                state->head.curAddr = address + (command->word[1] & 0xFFFF);
            }
        }
    }
    return 0;
}

/* SPLIT_RANDOM: jumps when a random byte reaches byte 1. */
u8 sound_macro_split_random(SynthVoice *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((u8)sound_random() >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->head.addr = address;
            state->head.curAddr = address + (command->word[1] & 0xFFFF);
        }
    }
    return 0;
}

/* GOSUB: runs another macro (bytes 2-3: its id; word[1]'s low half: the step), remembering where to RETURN (one
 * level). A missing macro ends this one. */
u8 sound_macro_gosub(SynthVoice *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((address = sound_find_macro(command->word[0] >> 16)) != 0) {
        state->head.retAddr = state->head.addr;
        state->head.retCurAddr = state->head.curAddr;
        state->head.addr = address;
        state->head.curAddr = address + (command->word[1] & 0xFFFF);
        return 0;
    }
    return sound_macro_end(state, command);
}

/* TRAP_KEY_OFF: from now on a key off jumps to the macro of bytes 2-3 at word[1]'s low half (sound_macro_run takes
 * the jump), unless the sustain pedal holds the key. */
u8 sound_macro_trap_key_off(SynthVoice *state, MacroCommand *command)
{
    MacroCommand *start;
    start = sound_find_macro((command->word[0] >> 16) & 0xFFFF);
    if (start != 0) {
        state->head.trapAddr = start;
        state->head.trapCurAddr = start + (command->word[1] & 0xFFFF);
    }
    return 0;
}

/* UNTRAP_KEY_OFF: a key off no longer jumps. */
u8 sound_macro_untrap_key_off(SynthVoice *state, MacroCommand *command)
{
    state->head.trapAddr = state->head.trapCurAddr = 0;
    return 0;
}

/* LOOP: jumps back to a step of this macro (word[1]'s low half) a number of times (word[1]'s top half; byte 2 bit 0:
 * a random number below it; 0xFFFF: for ever). Byte 1 bit 0: stop looping once the key is off (and the pedal up);
 * byte 3 bit 0: stop looping once the sample has ended. The counter (loop) is 0 between loops. */
u8 sound_macro_loop(SynthVoice *state, MacroCommand *command)
{
    if (state->loop == 0) {
        if ((u8)(command->word[0] >> 16) & 1) {
            state->loop = sound_random() % (u16)(command->word[1] >> 16);
        } else {
            state->loop = command->word[1] >> 16;
        }
        if (state->loop == 0xFFFF) goto skip;
        ++state->loop;
    } else if (state->loop == 0xFFFF) {
        goto skip;
    }
    if (--state->loop != 0) {
skip:
        if (((u8)(command->word[0] >> 8) & 1) != 0 &&
            !(state->flags & VOICE_PEDAL_DOWN) && (state->flags & VOICE_KEY_OFF)) {
            state->loop = 0;
            return 0;
        }
        if (((u8)(command->word[0] >> 24) & 1) &&
            !(state->flags & VOICE_START_SAMPLE) && !sound_hw_is_voice_active(VOICE_INDEX(state->id))) {
            state->loop = 0;
            return 0;
        }
        state->head.curAddr = state->head.addr + (command->word[1] & 0xFFFF);
    }
    return 0;
}

/* PLAY_MACRO: starts another macro on a new voice that joins this voice's handle: its id (bytes 2-3), priority and
 * voice limit (word[1]'s bytes 2 and 3) and first step (word[1]'s low half), on this voice's key plus byte 1 (signed),
 * with its volume, pan, channel, track and volume group. The new voice goes into the chain right after this one; an
 * effect voice's child starts with its controllers. The voice is blocked meanwhile, so the allocation can't take it. */
u8 sound_macro_play_macro(SynthVoice *state, MacroCommand *command)
{
    s32 key;
    u32 macro;
    u32 child;
    macro = (u16)(command->word[0] >> 16) << 16 | ((command->word[1] >> 16) & 0xFF) << 8 | ((command->word[1] >> 24) & 0xFF);
    key = state->orgNote + (s8)(u8)(command->word[0] >> 8);
    key = key < 0 ? 0 : key > 127 ? 127 : key;
    if (state->fxFlag) key |= 128;
    state->blocked = 1;
    child = sound_macro_start(macro, state->allocId, key,
        state->volume >> 16, state->panning >> 16, state->channel,
        state->set, command->word[1], state->track, 0, state->vGroup);
    state->blocked = 0;
    if (child != VOICE_NONE) {
        /* The links go through the voices' addresses, as the voice's own view was cast from them: GCC addresses them
         * otherwise through the array. */
        (&sound_synth_voices[VOICE_INDEX(child)])->head.previous = state->id;
        if (state->head.next != VOICE_NONE) {
            (&sound_synth_voices[VOICE_INDEX(child)])->head.next = state->head.next;
            (&sound_synth_voices[VOICE_INDEX(state->head.next)])->head.previous = child;
        }
        state->head.next = child;
        if (state->fxFlag) sound_synth_copy_controllers(&sound_synth_voices[VOICE_INDEX(child)], state);
    }
    return 0;
}

/* SEND_KEY_OFF: keys off every voice that plays the macro of bytes 2-3 on this voice's key plus byte 1 (a voice id
 * is (macro << 16) | (key << 8) | voice). */
u8 sound_macro_send_key_off(SynthVoice *voice, MacroCommand *command)
{
    u32 i;
    u32 id;
    id = (((command->word[0] >> 8) & 255) + voice->orgNote) << 8;
    id |= (u32)(u16)(command->word[0] >> 16) << 16;
    for (i = 0; i < sound_voice_count; ++i) {
        if ((id | i) == sound_synth_voices[i].id) {
            sound_synth_key_off_voice(id | i);
        }
    }
    return 0;
}

/* The age steps: a voice's age (15 bits of fraction) falls by ageSpeed each tick (synth.c), and among the
 * voices of the lowest priority the allocator steals the one of the lowest age (voices.c). A voice starts at 60000.
 * ADD_AGE: adds bytes 2-3 (signed) to it, clamped to 0..65535. */
u8 sound_macro_add_age(SynthVoice *state, MacroCommand *command)
{
    s16 delta;
    s32 age;
    delta = command->word[0] >> 16;
    age = (state->age >> 15) + delta;
    if (age < 0) {
        state->age = 0;
    } else if (age > 65535) {
        state->age = 0x7FFF8000;
    } else {
        state->age = (u32)age << 15;
    }
    return 0;
}

/* SET_AGE: the age becomes bytes 2-3. */
u8 sound_macro_set_age(SynthVoice *state, MacroCommand *command)
{
    state->age = ((u16)(command->word[0] >> 16)) << 15;
    return 0;
}

/* SET_AGE_SPEED: the age falls to 0 in word[1] ticks (0: it stays). */
u8 sound_macro_set_age_speed(SynthVoice *state, MacroCommand *command)
{
    u32 divisor;
    divisor = command->word[1];
    if (divisor != 0) {
        state->ageSpeed = (state->age >> 8) / divisor;
    } else {
        state->ageSpeed = 0;
    }
    return 0;
}

/* SET_AGE_BY_VELOCITY: the age becomes bytes 2-3 plus word[1]'s low half scaled by the velocity (the volume, /128),
 * at most 60000. */
u8 sound_macro_set_age_by_velocity(SynthVoice *state, MacroCommand *command)
{
    u32 age;
    age = (u16)(command->word[0] >> 16)
          + (((command->word[1] & 0xFFFF) * (state->volume >> 16)) >> 7);
    if (age > 60000) {
        state->age = 0x75300000;
    } else {
        state->age = age << 15;
    }
    return 0;
}

/* ADD_PRIORITY: adds bytes 2-3 (signed) to the voice's priority, clamped to 0..255. */
u8 sound_macro_add_priority(SynthVoice *state, MacroCommand *command)
{
    s32 delta;
    s32 priority;
    delta = (s16)(command->word[0] >> 16);
    priority = (s16)(state->priority + delta);
    if (priority < 0) {
        priority = 0;
    } else {
        priority = (s16)(priority > 255 ? 255 : priority);
    }
    sound_set_voice_priority(state, (u8)priority);
    return 0;
}

/* SET_PRIORITY: the voice's priority becomes byte 1. */
u8 sound_macro_set_priority(SynthVoice *state, MacroCommand *command)
{
    sound_set_voice_priority(state, (u8)(command->word[0] >> 8));
    return 0;
}

/* SEND_FLAG: sets the global macro variable of byte 1 to byte 2 (the variables macros share,
 * sound_global_variables). */
u8 sound_macro_send_flag(SynthVoice *state, MacroCommand *command)
{
    sound_global_variables[(u8)(command->word[0] >> 8)] = (u8)(command->word[0] >> 16);
    return 0;
}

/* SET_PITCH_WHEEL_RANGE: how many semitones the pitch wheel bends down (byte 2) and up (byte 1). */
u8 sound_macro_set_pitch_wheel_range(SynthVoice *state, MacroCommand *command)
{
    state->pbLowerKeyRange = command->word[0] >> 16;
    state->pbUpperKeyRange = command->word[0] >> 8;
    return 0;
}

/* The note steps set the note and detune (byte 2, cents, signed), make it the channel's last key if the voice owns
 * the channel, then wait as WAIT does with word[1] (the step's word[0] becomes a plain WAIT's).
 * SET_NOTE: the note is byte 1. */
u8 sound_macro_set_note(SynthVoice *state, MacroCommand *command)
{
    state->curNote = (u8)(command->word[0] >> 8) & 0x7F;
    state->curDetune = command->word[0] >> 16;
    if (sound_synth_owns_channel(state)) {
        sound_midi_set_last_key(state->channel, state->set, state->curNote);
    }
    command->word[0] = MACRO_WAIT;
    return sound_macro_wait(state, command);
}

/* ADD_NOTE: adds byte 1 (signed) to the note, or with byte 3 set to the key the voice started on; clamped to
 * 0..127. */
u8 sound_macro_add_note(SynthVoice *state, MacroCommand *command)
{
    if ((u8)(command->word[0] >> 24) == 0) {
        state->curNote += (s8)(u8)(command->word[0] >> 8);
    } else {
        state->curNote = state->orgNote + (s8)(u8)(command->word[0] >> 8);
    }
    state->curNote = (s16)state->curNote < 0 ? 0 : state->curNote > 127 ? 127 : state->curNote;
    state->curDetune = command->word[0] >> 16;
    if (sound_synth_owns_channel(state)) {
        sound_midi_set_last_key(state->channel, state->set, state->curNote);
    }
    command->word[0] = MACRO_WAIT;
    return sound_macro_wait(state, command);
}

/* LAST_NOTE: the note becomes the channel's previous key (when the voice started) plus byte 1 (signed), clamped
 * to 0..127. Unlike the others it records the note whether or not the voice owns the channel. */
u8 sound_macro_last_note(SynthVoice *state, MacroCommand *command)
{
    state->curNote = state->lastNote + (s8)(u8)(command->word[0] >> 8);
    state->curNote = (s16)state->curNote < 0 ? 0 : state->curNote > 127 ? 127 : state->curNote;
    state->curDetune = command->word[0] >> 16;
    if (state->channel != NO_CHANNEL) {
        sound_midi_set_last_key(state->channel, state->set, state->curNote);
    }
    command->word[0] = MACRO_WAIT;
    return sound_macro_wait(state, command);
}

/* Steps sound_macro_run has run in this tick (a file static, not a local, as in the original). */
static u8 macro_steps_this_update;
/* START_SAMPLE: gives the hardware voice the sample of bytes 1-2, starting word[1] samples in (byte 3: 0 as given,
 * 1 scaled down by the velocity, 2 scaled up by it, else 0; at most the sample's last), and marks it for the synth's
 * tick to start. Without a SET_ADSR the hardware voice's envelope is reset too. */
u8 sound_macro_start_sample(SynthVoice *state, MacroCommand *command)
{
    static SoundSampleInfo sample;
    u32 index;
    if (sound_find_sample(command->word[0] >> 8, &sample) == 0) {
        switch ((u8)(command->word[0] >> 24)) {
        case 0:
            sample.offset = command->word[1];
            break;
        case 1:
            sample.offset = (command->word[1] * (127U - (state->volume >> 16))) / 127U;
            break;
        case 2:
            sample.offset = (command->word[1] * (state->volume >> 16)) / 127U;
            break;
        default:
            sample.offset = 0;
        }
        if (sample.offset >= sample.length) {
            sample.offset = sample.length - 1;
        }
        index = VOICE_INDEX(state->id);
        if (!(state->flags & VOICE_ADSR_SET)) {
            sound_hw_set_voice_sample(index, &sample, 1);
        } else {
            sound_hw_set_voice_sample(index, &sample, 0);
        }
        state->sampleInfo = sample.info;
        state->sampleAddress = sample.cart_address;
        state->flags |= VOICE_START_SAMPLE;
    }
    return 0;
}

/* STOP_SAMPLE: stops the hardware voice's sample (through its release). */
u8 sound_macro_stop_sample(SynthVoice *state, MacroCommand *command)
{
    sound_hw_stop_sample(state->id & 0xFF, 1);
    return 0;
}

/* KEY_OFF: the synth's next tick releases the hardware voice (its envelope's release), once the pedal is up. */
u8 sound_macro_key_off(SynthVoice *state, MacroCommand *command)
{
    state->flags |= VOICE_RELEASE;
    return 0;
}

/* VIBRATO: depth byte 1 semitones and byte 2 cents (signed: a negative depth starts half a period in, the other way
 * round), period word[1]'s top half in ticks or milliseconds (word[1] byte 1 bit 0); a period of 0 turns it off.
 * Byte 3 bits 0-1: the modulation wheel scales the depth. */
u8 sound_macro_vibrato(SynthVoice *state, MacroCommand *command)
{
    u32 time;
    s8 keyRange;
    s8 centRange;
    if ((u8)(command->word[0] >> 24) & 3) {
        state->flags |= VOICE_VIBRATO_BY_MODULATION;
    } else {
        state->flags &= ~VOICE_VIBRATO_BY_MODULATION;
    }
    time = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&time);
    } else {
        sound_ticks_to_time(&time, state);
    }
    if (time) {
        state->flags |= VOICE_VIBRATO;
        state->vibPeriod = time;
        keyRange = command->word[0] >> 8;
        centRange = command->word[0] >> 16;
        if (keyRange < 0) {
            if (centRange < 0) state->vibCentRange = -centRange;
            else state->vibCentRange = centRange;
            state->vibKeyRange = -keyRange;
            state->vibCurTime = state->vibPeriod / 2;
        } else {
            if (centRange < 0) {
                if (keyRange == 0) {
                    state->vibCentRange = -centRange;
                    state->vibCurTime = state->vibPeriod / 2;
                } else {
                    /* Original game, perhaps: a positive key range with negative cents becomes one key less and
                     * 100 - cents (1 key -30 cents: 0 keys 130 cents, 1.3 semitones), where 100 + cents (0.7)
                     * would keep the depth. */
                    --keyRange;
                    state->vibCentRange = 100 - centRange;
                    state->vibCurTime = 0;
                }
            } else {
                state->vibCentRange = centRange;
                state->vibCurTime = 0;
            }
            state->vibKeyRange = keyRange;
        }
    } else {
        state->flags &= ~VOICE_VIBRATO;
    }
    return 0;
}

/* SETUP_LFO: sets the period of the voice's LFO of byte 1 (0 or 1) to bytes 2-3 milliseconds (0: off). The synth
 * turns each LFO into a sine wave the inputs can read as the controllers 0xA0 and 0xA1. A running LFO restarts its
 * phase. */
u8 sound_macro_setup_lfo(SynthVoice *state, MacroCommand *command)
{
    u32 duration;
    u8 index;
    SynthLfo *control;
    index = command->word[0] >> 8;
    duration = (u16)(command->word[0] >> 16);
    sound_ms_to_time(&duration);
    control = state->lfo + index;
    if (control->period != 0) {
        control->time = 0;
    }
    control->period = duration;
    return 0;
}

/* The semitone search's end: i once it has run past kf[0] (the N64's unsigned i at -1). */
#define PITCH_BELOW_TABLE 0xFFFFFFFFU
/* SET_PITCH: sets the note and detune that play the sample (its root key and rate, sampleInfo) at the rate of bytes 1-2
 * (Hz): the ratio's octaves, then its semitone from the table of 2^(n/12) (in 4096ths) and the cents between. */
u8 sound_macro_set_pitch(SynthVoice *state, MacroCommand *command)
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
    sample_rate = state->sampleInfo & 0xFFFFFF;
    if (sample_rate == rate) {
        state->curNote = (u8)(state->sampleInfo >> 24);
        state->curDetune = 0;
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
            state->curNote = (u8)(state->sampleInfo >> 24) + octaves * 12 - 1;
            state->curDetune = 100;
        } else {
            state->curNote = (u8)(state->sampleInfo >> 24) + octaves * 12 + i;
            state->curDetune = ((ratio - kf[i]) * 100) / (kf[i + 1] - kf[i]);
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
            state->curNote = (u8)(state->sampleInfo >> 24) - octaves * 12 + 1;
            state->curDetune = -100;
        } else {
            state->curNote = (u8)(state->sampleInfo >> 24) - octaves * 12 - i;
            state->curDetune = -(s8)(((ratio - kf[i]) * 100) / (kf[i + 1] - kf[i]));
        }
    }
    state->flags |= VOICE_PITCH_FROM_RATE;
    return 0;
}

#pragma pack(1)
typedef struct PackedEnvelope { u16 attack, decay, sustain, release; } PackedEnvelope;
#pragma pack()
typedef struct Envelope { u16 attack, decay, sustain, release; } Envelope;
/* SET_ADSR: gives the hardware voice the envelope (attack, decay, sustain, release) of the table of bytes 1-2; the
 * project stores it big-endian, so each halfword is swapped. A START_SAMPLE then keeps it. */
u8 sound_macro_set_adsr(SynthVoice *state, MacroCommand *command)
{
    Envelope envelope;
    PackedEnvelope *table;
    if ((table = sound_find_table(command->word[0] >> 8)) != 0) {
        envelope.attack = (table->attack >> 8) | (table->attack << 8);
        envelope.decay = (table->decay >> 8) | (table->decay << 8);
        envelope.sustain = (table->sustain >> 8) | (table->sustain << 8);
        envelope.release = (table->release >> 8) | (table->release << 8);
        sound_hw_set_envelope(state->id & 0xFF, &envelope);
        state->flags |= VOICE_ADSR_SET;
    }
    return 0;
}

/* PITCH_SWEEP_1 and _2 (index 0, 1): each tick the synth adds a step (bytes 2-3, signed, as a pitch rate) to the
 * sweep's pitch offset, and back to 0 every byte-1 ticks: a repeating ramp. Then waits as WAIT does with word[1]. */
u8 sound_macro_pitch_sweep(SynthVoice *state, MacroCommand *command, int index)
{
    s32 delta;
    state->sweepOff[index] = 0;
    state->sweepNum[index] = (u8)(command->word[0] >> 8);
    state->sweepCnt[index] = state->sweepNum[index];
    delta = (s16)(command->word[0] >> 16);
    if (delta >= 0) {
        delta = (s32)sound_rate_to_pitch(delta);
    } else {
        delta = (s32)(0U - sound_rate_to_pitch(-delta));
    }
    state->sweepAdd[index] = (u32)delta << 16;
    command->word[0] = 0;   /* a plain WAIT: no key-off, random or sample flags */
    return sound_macro_wait(state, command);
}

/* PANNING: pans from byte 1 (0..127, 64 centre) by word[1]'s low byte (signed) over bytes 2-3 milliseconds; the
 * synth moves it each tick. */
u8 sound_macro_panning(SynthVoice *voice, MacroCommand *command)
{
    u32 unused0, unused1;
    u32 time;
    s8 offset;
    voice->flags |= VOICE_PAN_SWEEP;
    time = (command->word[0] >> 16) & 65535;
    voice->panTime = time;
    sound_ms_to_time(&voice->panTime);
    offset = command->word[1] & 0xFF;
    voice->panning = ((command->word[0] >> 8) & 255) << 16;
    voice->panTarget = ((u32)offset << 16) + voice->panning;
    if (voice->panTime != 0) {
        voice->panDelta = ((u32)offset << 16) / time;
    } else {
        voice->panDelta = (u32)offset << 16;
    }
    return 0;
}

/* PIANO_PAN: pans by the note, like a piano's keys: byte 3 (the pan at the centre key) plus (note - byte 2, the
 * centre key) times byte 1 (signed) / 128, clamped to 0..127. */
u8 sound_macro_piano_pan(SynthVoice *state, MacroCommand *command)
{
    s32 pan;
    s8 gain;
    pan = ((u32)state->curNote << 16) - (((command->word[0] >> 16) & 0xFF) << 16);
    gain = command->word[0] >> 8;
    pan = (s32)((u32)pan * gain);
    pan >>= 7;
    pan += (u8)(command->word[0] >> 24) << 16;
    pan = pan < 0 ? 0 : (pan > 0x7F0000 ? 0x7F0000 : pan);
    state->panning = pan;
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

/* SCALE_VOLUME: the volume becomes the volume (with word[1] byte 1 set: the volume it started with) times byte 1
 * / 128, plus byte 2, at most 127, through the curve of byte 3 and word[1]'s low byte (sound_macro_apply_curve). */
u8 sound_macro_scale_volume(SynthVoice *state, MacroCommand *command)
{
    u16 curve;
    if ((u8)(command->word[1] >> 8) == 0) {
        state->volume = ((u8)(command->word[0] >> 8) * state->volume) >> 7;
    } else {
        state->volume = ((u8)(command->word[0] >> 8) * state->orgVolume) >> 7;
    }
    state->volume += (u8)(command->word[0] >> 16) << 16;
    if (state->volume > 0x7F0000) {
        state->volume = 0x7F0000;
    }
    curve = (u8)command->word[1];
    curve |= (u8)(command->word[0] >> 24) << 8;
    state->volume = sound_macro_apply_curve(state->volume, curve);
    return 0;
}

/* Starts a volume envelope from `start` to a target, over word[1]'s top half in ticks or milliseconds (word[1]
 * byte 1 bit 0): the target is the current volume times byte 1 / 128 plus byte 2, at most 127, through the curve of
 * byte 3 and word[1]'s low byte. The synth moves the volume each tick (envDelta per millisecond). */
u8 sound_macro_start_envelope(SynthVoice *state, MacroCommand *command, u32 start)
{
    u32 target;
    s32 milliseconds;
    u16 curve;
    state->envTime = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&state->envTime);
    } else {
        sound_ticks_to_time(&state->envTime, state);
    }
    milliseconds = sound_time_to_ms(state->envTime);
    if (milliseconds == 0) milliseconds = 1;
    target = ((u8)(command->word[0] >> 8) * state->volume) >> 7;
    target += (u8)(command->word[0] >> 16) << 16;
    if (target > 0x7F0000) target = 0x7F0000;
    curve = (u8)command->word[1];
    curve |= (u8)(command->word[0] >> 24) << 8;
    target = sound_macro_apply_curve(target, curve);
    state->envTarget = target;
    state->envDelta = (s32)(target - start) / milliseconds;
    state->volume = start;
    state->flags |= VOICE_VOLUME_SWEEP;
    return 0;
}

/* ENVELOPE: a volume envelope from the current volume. */
u8 sound_macro_envelope(SynthVoice *state, MacroCommand *command)
{
    return sound_macro_start_envelope(state, command, state->volume);
}

/* FADE_IN: a volume envelope from silence. */
u8 sound_macro_fade_in(SynthVoice *state, MacroCommand *command)
{
    return sound_macro_start_envelope(state, command, 0);
}

/* RANDOM_NOTE: plays a random note (as SET_NOTE, without its wait) between byte 1 and byte 3, or with word[1] byte 1
 * set, from byte 1 below the current note to byte 3 above it; the detune is byte 2, or random (-100..100 cents) when
 * word[1]'s low byte is set. */
u8 sound_macro_random_note(SynthVoice *state, MacroCommand *command)
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
        lower = state->curNote - (u8)(command->word[0] >> 8);
        upper = state->curNote + (u8)(command->word[0] >> 24);
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

/* TRACK_PITCH_WHEEL: the pitch wheel bends this voice. */
u8 sound_macro_track_pitch_wheel(SynthVoice *state, MacroCommand *command)
{
    state->flags |= VOICE_TRACK_PITCH_WHEEL;
    return 0;
}

/* SET_REVERB_SEND: the reverb send's scale (byte 1) and offset (byte 2). */
u8 sound_macro_set_reverb_send(SynthVoice *state, MacroCommand *command)
{
    state->revVolScale = command->word[0] >> 8;
    state->revVolOffset = command->word[0] >> 16;
    return 0;
}

/* The SELECT_* steps add a source to one of the voice's inputs (`control`): the controller of byte 1 (a macro's
 * number, sound_macro_map_controller), scaled by bytes 2-3 (signed percent), combined as word[1]'s low byte says.
 * The input's first SELECT since the voice started (its VOICE_INPUT_* `flag` clear), or a source that sets, starts
 * the input's mix afresh, replacing its default (sound_synth_init_controller_inputs). At most four sources. */
void sound_macro_add_input_source(SynthVoice *state, SynthInputMix *control,
                  MacroCommand *command, u32 flag)
{
    u8 index;
    u8 controller;
    u8 combination;
    combination = command->word[1] & 0xFF;
    if (!(state->flags & flag) || combination == MACRO_COMBINE_SET) {
        state->flags |= flag;
        control->count = 0;
    }
    if (control->count < 4) {
        index = control->count++;
        controller = sound_macro_map_controller(command->word[0] >> 8);
        control->source[index].controller = controller;
        control->source[index].combine = combination;
        control->source[index].scale = ((s32)(s16)(command->word[0] >> 16) * 256) / 100;
    }
}

/* SELECT_VOLUME .. SELECT_DOPPLER: a source for the volume, pan, pitch-bend, modulation, pedal, portamento, reverb,
 * span or doppler input. */
u8 sound_macro_select_volume(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_VOLUME],
                  command, VOICE_INPUT_VOLUME);
    return 0;
}

u8 sound_macro_select_pan(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_PAN],
                  command, VOICE_INPUT_PAN);
    return 0;
}

u8 sound_macro_select_pitch_bend(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_PITCH_BEND],
                  command, VOICE_INPUT_PITCH_BEND);
    return 0;
}

u8 sound_macro_select_modulation(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_MODULATION],
                  command, VOICE_INPUT_MODULATION);
    return 0;
}

u8 sound_macro_select_pedal(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_PEDAL],
                  command, VOICE_INPUT_PEDAL);
    return 0;
}

u8 sound_macro_select_portamento(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_PORTAMENTO],
                  command, VOICE_INPUT_PORTAMENTO);
    return 0;
}

u8 sound_macro_select_reverb(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_REVERB],
                  command, VOICE_INPUT_REVERB);
    return 0;
}

u8 sound_macro_select_span(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_SPAN],
                  command, VOICE_INPUT_SPAN);
    return 0;
}

u8 sound_macro_select_doppler(SynthVoice *state, MacroCommand *command)
{
    sound_macro_add_input_source(state, &state->inputs[SYNTH_INPUT_DOPPLER],
                  command, VOICE_INPUT_DOPPLER);
    return 0;
}

/* PORTAMENTO: byte 1 0 turns portamento off, 1 on (both take it from the portamento controller: the macro decides
 * now); byte 2 is the glide's mode, word[1]'s top half its time in ticks or milliseconds (word[1] byte 1 bit 0). */
u8 sound_macro_portamento(SynthVoice *state, MacroCommand *command)
{
    u32 time;
    state->portType = command->word[0] >> 16;
    time = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&time);
    } else {
        sound_ticks_to_time(&time, state);
    }
    state->portDuration = time;
    switch ((u8)(command->word[0] >> 8)) {
    case 0:
        state->flags &= ~VOICE_PORTAMENTO;
        state->flags |= VOICE_PORTAMENTO_BY_MACRO;
        if (state->channel != NO_CHANNEL) sound_midi_set_portamento_state(state->channel, state->set, 0);
        break;
    case 1:
        if (!(state->flags & VOICE_PORTAMENTO)) sound_synth_start_portamento(state);
        state->flags |= VOICE_PORTAMENTO_BY_MACRO | VOICE_PORTAMENTO;
        break;
    }
    return 0;
}

/* The variables of the arithmetic and comparison steps: with isController, a controller (a macro's number,
 * sound_macro_get_controller); else variable `index` & 31: 0..15 the voice's own, 16..31 the globals
 * (sound_global_variables, which SEND_FLAG sets too). (The N64's code reached the globals as 32 bytes past
 * sound_seq_current_index, the datum before them; they are named here.) */
s16 sound_macro_get_variable(SynthVoice *voice, u8 isController, u8 index)
{
    s32 value;
    if (isController) {
        return sound_macro_get_controller(voice, index);
    }
    index &= 31;
    value = index < 16 ? voice->localVars[index] : (s16)sound_global_variables[index - 16];
    return value;
}

/* Writes a variable (as sound_macro_get_variable reads it). */
void sound_macro_set_variable(SynthVoice *voice, u8 isController, u8 index, s16 value)
{
    if (isController) {
        sound_macro_set_controller(voice, index, value);
    } else {
        index &= 31;
        if (index < 16) {
            voice->localVars[index] = value;
        } else {
            sound_global_variables[index - 16] = value;
        }
    }
}

/* ADD_VARIABLES .. DIVIDE_VARIABLES, ADD_TO_VARIABLE: the variable of bytes 1-2 (byte 1: a controller?, byte 2: its
 * number) becomes the variable of byte 3 and word[1]'s low byte, plus, minus, times or divided by (0 for a division by
 * 0) the variable of word[1]'s bytes 1-2, or for ADD_TO_VARIABLE the constant word[1] bits 8..23; clamped to 16 bits. */
u8 sound_macro_variable_arithmetic(SynthVoice *voice, MacroCommand *command, u8 operation)
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
u8 sound_macro_set_variable_immediate(SynthVoice *state, MacroCommand *command)
{
    sound_macro_set_variable(state, command->word[0] >> 8, command->word[0] >> 16,
                  (s16)(command->word[1] & 0xFFFF));
    return 0;
}

/* IF_EQUAL, IF_LESS: compares the variable of bytes 1-2 with the variable of byte 3 and word[1]'s low byte; when it
 * holds (or with word[1] byte 1 set, when it doesn't) jumps to the step of word[1]'s top half in this macro. */
u8 sound_macro_compare(SynthVoice *voice, MacroCommand *command, u8 comparison)
{
    s16 left;
    s16 right;
    u8 condition;
    left = sound_macro_get_variable(voice, command->word[0] >> 8, command->word[0] >> 16);
    right = sound_macro_get_variable(voice, command->word[0] >> 24, command->word[1] & 0xFF);
    switch (comparison) {
    case MACRO_COMPARE_EQUAL: condition = left == right; break;
    case MACRO_COMPARE_LESS: condition = left < right; break;
    }
    if ((command->word[1] >> 8) & 255) {
        condition = !condition;
    }
    if (condition) {
        voice->head.curAddr = ((command->word[1] >> 16) & 65535) + voice->head.addr;
    }
    return 0;
}

/* Runs a voice's macro for one tick (synth.c calls it for every active voice). A just-started voice is first set up
 * from what sound_macro_start stored: default inputs, volume, pan, channel, track, volume group, last key, pitch-wheel
 * range, reverb send; no sweeps, LFOs, trap or variables; its clock at 0. Then a pending key-off trap is taken, and
 * if the voice isn't waiting, steps run until a handler says stop (MACRO_STEPS_PER_RUN at most). A waiting voice
 * only advances its clock and ends its wait at key off, at the sample's end or when it's due. */
void sound_macro_run(SynthVoice *voice)
{
    static MacroCommand macro_step_copy;   /* the step being run (a copy: some handlers rewrite it) */
    u8 i;
    u8 lastNote;
    u8 stop;

    if (voice->flags & (VOICE_HW_BUSY_AT_START | VOICE_STARTING)) {
        if (voice->flags & VOICE_HW_BUSY_AT_START) {
            voice->flags &= ~VOICE_HW_BUSY_AT_START;
            sound_hw_break_voice(voice->id & 0xFF);
        }
        /* 0x40: nothing in the library sets it; with it the voice is freed instead of starting */
        if (!(voice->flags & 0x40)) {
            sound_synth_init_controller_inputs(voice);
            voice->panning = voice->setupPan << 16;
            voice->volume = voice->setupVol << 16;
            voice->orgVolume = voice->volume;
            voice->channel = voice->setupChannel;
            voice->set = voice->setupSet;
            voice->track = voice->setupTrack;
            if ((lastNote = sound_midi_get_last_key(voice->channel, voice->set)) != NO_LAST_KEY) {
                voice->lastNote = lastNote;
            } else {
                voice->lastNote = voice->orgNote;
            }
            sound_midi_set_last_key(voice->channel, voice->set, voice->orgNote);
            sound_synth_claim_channel(voice);
            voice->vGroup = voice->setupVGroup;
            voice->portTime = 0;
            voice->portDuration = 100;
            voice->portType = 0;
            voice->pbLowerKeyRange = 2;
            voice->pbUpperKeyRange = 2;
            voice->revVolScale = 128;
            voice->revVolOffset = 0;
            voice->loop = 0;
            voice->sweepNum[0] = 0;
            voice->sweepNum[1] = 0;
            voice->sweepOff[0] = 0;
            voice->sweepOff[1] = 0;
            voice->lfo[0].period = 0;
            voice->lfo[0].value = 0;
            voice->lfo[1].period = 0;
            voice->lfo[1].value = 0;
            voice->head.trapAddr = 0;
            voice->head.trapCurAddr = 0;
            voice->sampleInfo = (64 << 24) | 44100;   /* root key 64 at 44,100 Hz until a sample says otherwise */
            voice->pbLast = 0x2000;                   /* the pitch wheel centred */
            voice->curOutputVolume = 0;
            voice->flags &= VOICE_KEY_OFF;            /* keeps only a key off that came before the start */
            for (i = 0; i < 16; i++) {
                voice->localVars[i] = 0;
            }
            voice->waitTime = 0;
            voice->time = 0;
        } else {
            voice->flags = 0;
            sound_free_voice(voice);
            return;
        }
    }
    if (voice->head.trapCurAddr != 0 && !(voice->flags & VOICE_PEDAL_DOWN) && (voice->flags & VOICE_KEY_OFF)) {
        voice->head.addr = voice->head.trapAddr;
        voice->head.curAddr = voice->head.trapCurAddr;
        voice->wait = 0;
        voice->head.trapAddr = voice->head.trapCurAddr = 0;
    }
    if (voice->wait == 0) {
        macro_steps_this_update = 0;
        do {
            if (++macro_steps_this_update > MACRO_STEPS_PER_RUN) {
                break;
            }
            macro_step_copy.word[0] = voice->head.curAddr->word[0];
            macro_step_copy.word[1] = voice->head.curAddr->word[1];
            voice->head.curAddr++;
            switch (macro_step_copy.word[0] & MACRO_OPCODE_MASK) {
            case MACRO_END:
                stop = sound_macro_end(voice, &macro_step_copy);
                break;
            case MACRO_RETURN:
                stop = sound_macro_return(voice, &macro_step_copy);
                break;
            case MACRO_SPLIT_KEY:
                stop = sound_macro_split_key(voice, &macro_step_copy);
                break;
            case MACRO_SPLIT_VELOCITY:
                stop = sound_macro_split_velocity(voice, &macro_step_copy);
                break;
            case MACRO_WAIT:
                stop = sound_macro_wait(voice, &macro_step_copy);
                break;
            case MACRO_LOOP:
                stop = sound_macro_loop(voice, &macro_step_copy);
                break;
            case MACRO_GOSUB:
                stop = sound_macro_gosub(voice, &macro_step_copy);
                break;
            case MACRO_WAIT_MS:
                stop = sound_macro_wait_ms(voice, &macro_step_copy);
                break;
            case MACRO_PLAY_MACRO:
                stop = sound_macro_play_macro(voice, &macro_step_copy);
                break;
            case MACRO_SEND_KEY_OFF:
                stop = sound_macro_send_key_off(voice, &macro_step_copy);
                break;
            case MACRO_SPLIT_MODULATION:
                stop = sound_macro_split_modulation(voice, &macro_step_copy);
                break;
            case MACRO_PIANO_PAN:
                stop = sound_macro_piano_pan(voice, &macro_step_copy);
                break;
            case MACRO_SET_ADSR:
                stop = sound_macro_set_adsr(voice, &macro_step_copy);
                break;
            case MACRO_SCALE_VOLUME:
                stop = sound_macro_scale_volume(voice, &macro_step_copy);
                break;
            case MACRO_PANNING:
                stop = sound_macro_panning(voice, &macro_step_copy);
                break;
            case MACRO_ENVELOPE:
                stop = sound_macro_envelope(voice, &macro_step_copy);
                break;
            case MACRO_START_SAMPLE:
                stop = sound_macro_start_sample(voice, &macro_step_copy);
                break;
            case MACRO_STOP_SAMPLE:
                stop = sound_macro_stop_sample(voice, &macro_step_copy);
                break;
            case MACRO_KEY_OFF:
                stop = sound_macro_key_off(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SPLIT_RANDOM:
                stop = sound_macro_split_random(voice, &macro_step_copy);
                break;
            case MACRO_FADE_IN:
                stop = sound_macro_fade_in(voice, &macro_step_copy);
                break;
            case MACRO_RANDOM_NOTE:
                stop = sound_macro_random_note(voice, &macro_step_copy);
                break;
            case MACRO_ADD_NOTE:
                stop = sound_macro_add_note(voice, &macro_step_copy);
                break;
            case MACRO_SET_NOTE:
                stop = sound_macro_set_note(voice, &macro_step_copy);
                break;
            case MACRO_LAST_NOTE:
                stop = sound_macro_last_note(voice, &macro_step_copy);
                break;
            case MACRO_PORTAMENTO:
                stop = sound_macro_portamento(voice, &macro_step_copy);
                break;
            case MACRO_VIBRATO:
                stop = sound_macro_vibrato(voice, &macro_step_copy);
                break;
            case MACRO_PITCH_SWEEP_1:
                stop = sound_macro_pitch_sweep(voice, &macro_step_copy, 0);
                break;
            case MACRO_PITCH_SWEEP_2:
                stop = sound_macro_pitch_sweep(voice, &macro_step_copy, 1);
                break;
            case MACRO_SET_PITCH:
                stop = sound_macro_set_pitch(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_TRAP_KEY_OFF:
                stop = sound_macro_trap_key_off(voice, &macro_step_copy);
                break;
            case MACRO_UNTRAP_KEY_OFF:
                stop = sound_macro_untrap_key_off(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_ADD_AGE:
                stop = sound_macro_add_age(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SET_AGE:
                stop = sound_macro_set_age(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SEND_FLAG:
                stop = sound_macro_send_flag(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SET_PITCH_WHEEL_RANGE:
                stop = sound_macro_set_pitch_wheel_range(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SET_REVERB_SEND:
                stop = sound_macro_set_reverb_send(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_TRACK_PITCH_WHEEL:
                stop = sound_macro_track_pitch_wheel(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SET_PRIORITY:
                stop = sound_macro_set_priority(voice, &macro_step_copy);
                break;
            case MACRO_ADD_PRIORITY:
                stop = sound_macro_add_priority(voice, &macro_step_copy);
                break;
            case MACRO_SET_AGE_SPEED:
                stop = sound_macro_set_age_speed(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SET_AGE_BY_VELOCITY:
                stop = sound_macro_set_age_by_velocity(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_VOLUME:
                stop = sound_macro_select_volume(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_PAN:
                stop = sound_macro_select_pan(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_PITCH_BEND:
                stop = sound_macro_select_pitch_bend(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_MODULATION:
                stop = sound_macro_select_modulation(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_PEDAL:
                stop = sound_macro_select_pedal(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_PORTAMENTO:
                stop = sound_macro_select_portamento(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_REVERB:
                stop = sound_macro_select_reverb(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_SPAN:
                stop = sound_macro_select_span(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SELECT_DOPPLER:
                stop = sound_macro_select_doppler(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_SETUP_LFO:
                stop = sound_macro_setup_lfo(voice, (MacroCommand *)&macro_step_copy);
                break;
            case MACRO_ADD_VARIABLES:
                stop = sound_macro_variable_arithmetic(voice, &macro_step_copy, MACRO_ARITH_ADD);
                break;
            case MACRO_SUBTRACT_VARIABLES:
                stop = sound_macro_variable_arithmetic(voice, &macro_step_copy, MACRO_ARITH_SUBTRACT);
                break;
            case MACRO_MULTIPLY_VARIABLES:
                stop = sound_macro_variable_arithmetic(voice, &macro_step_copy, MACRO_ARITH_MULTIPLY);
                break;
            case MACRO_DIVIDE_VARIABLES:
                stop = sound_macro_variable_arithmetic(voice, &macro_step_copy, MACRO_ARITH_DIVIDE);
                break;
            case MACRO_ADD_TO_VARIABLE:
                stop = sound_macro_variable_arithmetic(voice, &macro_step_copy, MACRO_ARITH_ADD_IMMEDIATE);
                break;
            case MACRO_SET_VARIABLE:
                stop = sound_macro_set_variable_immediate(voice, &macro_step_copy);
                break;
            case MACRO_IF_EQUAL:
                stop = sound_macro_compare(voice, &macro_step_copy, MACRO_COMPARE_EQUAL);
                break;
            case MACRO_IF_LESS:
                stop = sound_macro_compare(voice, &macro_step_copy, MACRO_COMPARE_LESS);
                break;
            default:
                stop = 0;
            }
        } while (!stop);
        voice->time += sound_tick_step;
    } else {
        voice->time += sound_tick_step;
        if ((voice->flags & VOICE_WAIT_KEY_OFF) && !(voice->flags & VOICE_PEDAL_DOWN) && (voice->flags & VOICE_KEY_OFF)) {
            voice->flags &= ~VOICE_WAIT_KEY_OFF;
            voice->waitTime = voice->time;
            voice->wait = 0;
        }
        if ((voice->flags & VOICE_WAIT_SAMPLE_END) && !sound_hw_is_voice_active(voice->id & 0xFF)) {
            voice->flags &= ~VOICE_WAIT_SAMPLE_END;
            voice->wait = 0;
            voice->waitTime = voice->time;
        }
        if (voice->wait != MACRO_WAIT_FOREVER && voice->time >= voice->wait) {
            voice->waitTime = voice->wait;
            voice->wait = 0;
        }
    }
}

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
    SynthVoice *state;
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
            state = &sound_synth_voices[index];
            sound_unlink_voice(state);
            state->flags = (state->flags & VOICE_PLAYING) | VOICE_STARTING;
            if (sound_hw_is_voice_active(index)) state->flags |= VOICE_HW_BUSY_AT_START;
            state->wait = 0;
            if (key & MACRO_KEY_EFFECT) {
                /* an effect voice is its own MIDI channel, in the set VOICE_OWN_SET */
                state->fxFlag = 1;
                key &= 127;
                sound_midi_reset_channel(index, VOICE_OWN_SET);
                state->setupChannel = index;
                state->setupSet = VOICE_OWN_SET;
            } else {
                state->fxFlag = 0;
                state->setupChannel = channel;
                state->setupSet = set;
            }
            state->macroId = macro;
            state->allocId = allocation;
            state->age = 60000U << 15;
            state->ageSpeed = 1024;
            state->head.addr = program;
            state->head.curAddr = program + offset;
            state->orgNote = key;
            state->curNote = key;
            state->curDetune = 0;
            state->setupVol = volume;
            state->setupPan = panning;
            state->setupTrack = track;
            state->head.retAddr = 0;
            state->head.next = VOICE_NONE;
            state->head.previous = VOICE_NONE;
            state->setupVGroup = group;
            state->id = (macro << 16) | ((u32)key << 8) | (u32)index;
            sound_set_voice_priority(state, priority);
            if (start) return sound_new_handle(state);
            return state->id;
        }
    }
    return VOICE_NONE;
}
