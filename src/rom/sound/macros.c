/* GENERATED ROM-aligned TU — segment 0x22300 (rom/lib_22300)
 * layout map 54d665d6b39f1b38a2856abd36b6c977ef6f9441dc6ab90962db9c986228773b.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-04 — sound_synth_key_off_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct VoiceState { u8 unknown00[36]; u32 flags24; u8 unknown28[34]; u8 channel4A; u8 set4B; u8 unknown4C[20]; u32 id60; u8 unknown64[268]; s16 lfo170; u8 unknown172[10]; s16 lfo17C; u8 unknown17E[34]; } VoiceState;
#pragma pack(0)
/* On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9), seen here in this file's views: the voices and an effect voice's legato owner.
 * PC_MX_LOOP(32) is the run's voices where the N64 clears its 32 (docs/port/AUDIO.md 10). */
#define sound_synth_voices (*(VoiceState (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)
#define sound_voice_channel_owners (*(u8 (*)[PC_MX_VOICE_SLOTS])pc_mx_fx_legato)
#define PC_MX_LOOP(n) PC_MX_SLOTS
int sound_synth_key_off_voice(u32 id)
{
    VoiceState *voice;
    if (id != 0xFFFFFFFFU) {
        voice = &sound_synth_voices[id & 255];
        if (voice->id60 == id) {
            voice->flags24 |= 8;
            return 0;
        }
    }
    return -1;
}

/* PROMOTED 2026-10-11 — sound_synth_owns_channel
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern u8 sound_midi_channel_owners[][16];
struct MacroState;
int sound_synth_owns_channel(struct MacroState *state)
{
    u8 index;
    if (((VoiceState *)state)->id60 != 0xFFFFFFFFU && ((VoiceState *)state)->channel4A != 255) {
        index = ((VoiceState *)state)->id60 & 0xFF;
        if (((VoiceState *)state)->set4B == 255) {
            if (sound_voice_channel_owners[index] == index) return 1;
        } else {
            if (sound_midi_channel_owners[((VoiceState *)state)->set4B][((VoiceState *)state)->channel4A] == index) return 1;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_synth_claim_channel
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
void sound_synth_claim_channel(VoiceState *voice)
{
    u8 index;
    if (voice->id60 != 0xFFFFFFFFU && voice->channel4A != 255) {
        index = voice->id60 & 0xFF;
        if (voice->set4B == 255) {
            sound_voice_channel_owners[index] = index;
        } else {
            sound_midi_channel_owners[voice->set4B][voice->channel4A] = index;
        }
    }
}

/* PROMOTED 2026-10-11 — sound_synth_release_channel
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
void sound_synth_release_channel(VoiceState *voice)
{
    u8 index;
    if (voice->id60 != 0xFFFFFFFFU && voice->channel4A != 255) {
        index = voice->id60 & 0xFF;
        if (voice->set4B == 255) {
            if (sound_voice_channel_owners[index] == index) sound_voice_channel_owners[index] = 255;
        } else {
            if (sound_midi_channel_owners[voice->set4B][voice->channel4A] == index) sound_midi_channel_owners[voice->set4B][voice->channel4A] = 255;
        }
    }
}

/* PROMOTED 2026-10-05 — sound_synth_init_channel_owners
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_midi_channel_owners[8][16];
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

typedef struct MacroState { u8 unknown00[0x30]; u32 field30; u8 unknown34[0x2C]; u32 field60; } MacroState;
typedef struct MacroCommand { u32 word[2]; } MacroCommand;
/* PC: the voice state (sound_synth_voices) keeps its N64 layout, 0x1A0 bytes, in every view. Its macro and trap
 * pointers (+0, +4, +8, +0xC, +0x1C, +0x20) point into the pool, in the game heap, so on the PC they are
 * 32-bit window addresses: N64P(x) is such a field as a pointer, N64W(x) a pointer stored into one, and
 * N64_NEXT(x) moves one to the next step (docs/port/MUSYX_DATA.md 8.3). */
enum n64p_MacroCommand { n64p_MacroCommand_ };
#define N64P(x) _Generic((x), enum n64p_MacroCommand: (MacroCommand *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
#define N64_NEXT(x) ((x) = N64W(N64P(x) + 1))
/* PROMOTED 2026-10-11 — sound_macro_wait
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
#pragma pack(1)
typedef struct MacroStateWait { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x38]; u32 identifier60; u8 unknown64[0x38]; u32 deadline9C; u32 baseA0; u32 clockA4; } MacroStateWait;
#pragma pack()
extern u8 sound_hw_is_voice_active(int);
extern u16 sound_random(void);
extern void sound_ms_to_time(u32 *);
extern void sound_ticks_to_time(u32 *, MacroState *);
u8 sound_macro_wait(MacroState *state, MacroCommand *command)
{
    u32 time;
    u8 milliseconds;
    time = (u16)(command->word[1] >> 16);
    if (time != 0) {
        if ((u8)(command->word[0] >> 8) & 1) {
            if (((MacroStateWait *)state)->flags24 & 8) return 0;
            ((MacroStateWait *)state)->flags24 |= 4;
        } else {
            ((MacroStateWait *)state)->flags24 &= ~4U;
        }
        if ((u8)(command->word[0] >> 24) & 1) {
            if (!(((MacroStateWait *)state)->flags24 & 0x20) && !sound_hw_is_voice_active(((MacroStateWait *)state)->identifier60 & 255)) return 0;
            ((MacroStateWait *)state)->flags24 |= 0x100000;
        } else {
            ((MacroStateWait *)state)->flags24 &= ~0x100000U;
        }
        if ((u8)(command->word[0] >> 16) & 1) time = sound_random() % time;
        if (time != 0xFFFF) {
            milliseconds = (u8)(command->word[1] >> 8) & 1;
            if (milliseconds) sound_ms_to_time(&time);
            else sound_ticks_to_time(&time, state);
            if (milliseconds) {
                ((MacroStateWait *)state)->deadline9C = ((MacroStateWait *)state)->clockA4 + time;
            } else {
                ((MacroStateWait *)state)->deadline9C = ((MacroStateWait *)state)->baseA0 + time;
                if (((MacroStateWait *)state)->deadline9C <= ((MacroStateWait *)state)->clockA4) {
                    ((MacroStateWait *)state)->baseA0 = ((MacroStateWait *)state)->deadline9C;
                    ((MacroStateWait *)state)->deadline9C = 0;
                }
            }
        } else {
            ((MacroStateWait *)state)->deadline9C = 0xFFFFFFFFU;
        }
        return ((MacroStateWait *)state)->deadline9C != 0;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_wait_ms
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_macro_wait(MacroState *, MacroCommand *);
/* PC: the step's words are in host order (port/assets/musyx.c swaps the pool's macros), so the N64's byte 6 is
 * not bits 15..8 of word[1], the milliseconds flag sound_macro_wait tests. The same store, on the word. */
u8 sound_macro_wait_ms(MacroState *state, MacroCommand *command)
{
    command->word[1] = (command->word[1] & ~0xFF00U) | 0x100U;
    return sound_macro_wait(state, command);
}

/* PROMOTED 2026-10-04 — sound_macro_end
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_unlink_voice(MacroState *);
extern void sound_free_voice(MacroState *);
u8 sound_macro_end(MacroState *state, MacroCommand *command)
{
    sound_unlink_voice(state);
    sound_free_voice(state);
    return 1;
}

/* PROMOTED 2026-10-05 — sound_macro_return
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateReturn { u32 field00; u32 field04; u32 field08; u32 field0C; u8 unknown10[0xC]; enum n64p_MacroCommand field1C; enum n64p_MacroCommand field20; u8 unknown24[0xA]; u8 field2E; } MacroStateReturn;
typedef struct MacroControlReturn { u32 field0; u32 field4; u32 field8; } MacroControlReturn;
#pragma pack(0)
extern u8 sound_macro_end(MacroState *, MacroCommand *);
u8 sound_macro_return(MacroStateReturn *state, MacroCommand *command)
{
    if (((command->word[0] >> 8) & 0xFF) == 0 || state->field08 == 0) {
        return sound_macro_end((MacroState *)state, command);
    }
    state->field00 = state->field08;
    state->field04 = state->field0C;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_split_key
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSplitKey { enum n64p_MacroCommand field00; enum n64p_MacroCommand field04; enum n64p_MacroCommand field08; enum n64p_MacroCommand field0C; u8 unknown10[0x20]; u32 field30; u8 unknown34[0x16]; u8 field4A; u8 unknown4B[5]; u16 field50; } MacroStateSplitKey;
#pragma pack(0)
extern MacroCommand *sound_find_macro(u16);
u8 sound_macro_split_key(MacroStateSplitKey *state, MacroCommand *command)
{
    MacroCommand *address;
    if (state->field50 >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->field00 = N64W(address);
            state->field04 = N64W(address + (command->word[1] & 0xFFFF));
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_split_velocity
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSplitVelocity { enum n64p_MacroCommand field00; enum n64p_MacroCommand field04; enum n64p_MacroCommand field08; enum n64p_MacroCommand field0C; u8 unknown10[0x20]; u32 field30; u8 unknown34[0x16]; u8 field4A; u8 unknown4B[5]; u16 field50; } MacroStateSplitVelocity;
#pragma pack(0)
u8 sound_macro_split_velocity(MacroStateSplitVelocity *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((state->field30 >> 16) >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->field00 = N64W(address);
            state->field04 = N64W(address + (command->word[1] & 0xFFFF));
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_split_modulation
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSplitModulation { enum n64p_MacroCommand field00; enum n64p_MacroCommand field04; enum n64p_MacroCommand field08; enum n64p_MacroCommand field0C; u8 unknown10[0x20]; u32 field30; u8 unknown34[0x16]; u8 field4A; u8 unknown4B[5]; u16 field50; } MacroStateSplitModulation;
#pragma pack(0)
extern u16 sound_synth_get_modulation_input(MacroStateSplitModulation *);
u8 sound_macro_split_modulation(MacroStateSplitModulation *state, MacroCommand *command)
{
    MacroCommand *address;
    u8 modulation;
    if (state->field4A != 255) {
        modulation = sound_synth_get_modulation_input(state) >> 7;
        if (modulation >= (u8)(command->word[0] >> 8)) {
            if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
                state->field00 = N64W(address);
                state->field04 = N64W(address + (command->word[1] & 0xFFFF));
            }
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_split_random
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSplitRandom { enum n64p_MacroCommand field00; enum n64p_MacroCommand field04; enum n64p_MacroCommand field08; enum n64p_MacroCommand field0C; u8 unknown10[0x20]; u32 field30; u8 unknown34[0x16]; u8 field4A; u8 unknown4B[5]; u16 field50; } MacroStateSplitRandom;
#pragma pack(0)
extern u16 sound_random(void);
u8 sound_macro_split_random(MacroStateSplitRandom *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((u8)sound_random() >= (u8)(command->word[0] >> 8)) {
        if ((address = sound_find_macro((u16)(command->word[0] >> 16))) != 0) {
            state->field00 = N64W(address);
            state->field04 = N64W(address + (command->word[1] & 0xFFFF));
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_gosub
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateGosub { enum n64p_MacroCommand field00; enum n64p_MacroCommand field04; enum n64p_MacroCommand field08; enum n64p_MacroCommand field0C; u8 unknown10[0x20]; u32 field30; u8 unknown34[0x16]; u8 field4A; u8 unknown4B[5]; u16 field50; } MacroStateGosub;
#pragma pack(0)
u8 sound_macro_gosub(MacroStateGosub *state, MacroCommand *command)
{
    MacroCommand *address;
    if ((address = sound_find_macro(command->word[0] >> 16)) != 0) {
        state->field08 = state->field00;
        state->field0C = state->field04;
        state->field00 = N64W(address);
        state->field04 = N64W(address + (command->word[1] & 0xFFFF));
        return 0;
    }
    return sound_macro_end((MacroState *)state, command);
}

/* PROMOTED 2026-10-11 — sound_macro_trap_key_off
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateTrapKeyOff { u32 field00; u32 field04; u32 field08; u32 field0C; u8 unknown10[0xC]; enum n64p_MacroCommand field1C; enum n64p_MacroCommand field20; u8 unknown24[0xA]; u8 field2E; } MacroStateTrapKeyOff;
typedef struct MacroControl { u32 field0; u32 field4; u32 field8; } MacroControl;
#pragma pack(0)
u8 sound_macro_trap_key_off(MacroStateTrapKeyOff *state, MacroCommand *command)
{
    MacroCommand *start;
    start = sound_find_macro((command->word[0] >> 16) & 0xFFFF);
    if (start != 0) {
        state->field1C = N64W(start);
        state->field20 = N64W(start + (command->word[1] & 0xFFFF));
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_untrap_key_off
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateUntrapKeyOff { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateUntrapKeyOff;
#pragma pack(0)
typedef struct MacroCommandUntrapKeyOff { u32 word[2]; } MacroCommandUntrapKeyOff;
u8 sound_macro_untrap_key_off(MacroStateUntrapKeyOff *state, MacroCommandUntrapKeyOff *command)
{
    state->field1C = state->field20 = 0;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_loop
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateLoop { enum n64p_MacroCommand start00; enum n64p_MacroCommand current04; u8 unknown08[0x1C]; u32 flags24; u8 unknown28[0x38]; u32 identifier60; u8 unknown64[4]; u16 loop68; } MacroStateLoop;
#pragma pack()
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
            !(state->flags24 & 0x40000000U) && (state->flags24 & 8)) {
            state->loop68 = 0;
            return 0;
        }
        if (((u8)(command->word[0] >> 24) & 1) &&
            !(state->flags24 & 0x20) && !sound_hw_is_voice_active(state->identifier60 & 255)) {
            state->loop68 = 0;
            return 0;
        }
        state->current04 = N64W(N64P(state->start00) + (command->word[1] & 0xFFFF));
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_play_macro
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
typedef struct MacroStatePlayMacro { u8 unknown00[0x10]; u32 child10; u32 parent14; u8 unknown18[0x17]; u8 section2F; u32 volume30; u8 unknown34[4]; u32 panning38; u8 unknown3C[0xE]; u8 channel4A; u8 set4B; u8 external4C; u8 unknown4D; u16 originalNote4E; u8 unknown50[0x10]; u32 identifier60; u8 unknown64[0x56]; u16 allocationBA; u8 unknownBC; u8 activeBD; u8 groupBE; u8 unknownBF[0xE1]; } MacroStatePlayMacro;
#pragma pack()
extern u32 sound_macro_start(u32 packed, u16 allocation, u8 key, u8 volume, u8 panning, u8 channel, u8 set, u16 offset, u16 section, u8 start, u8 group);
extern void sound_synth_copy_controllers(MacroStatePlayMacro *, MacroStatePlayMacro *);
u8 sound_macro_play_macro(MacroStatePlayMacro *state, MacroCommand *command)
{
    s32 key;
    u32 macro;
    u32 child;
    macro = (u16)(command->word[0] >> 16) << 16 | ((command->word[1] >> 16) & 0xFF) << 8 | ((command->word[1] >> 24) & 0xFF);
    key = state->originalNote4E + (s8)(u8)(command->word[0] >> 8);
    key = key < 0 ? 0 : key > 127 ? 127 : key;
    if (state->external4C) key |= 128;
    state->activeBD = 1;
    child = sound_macro_start(macro, state->allocationBA, key,
        state->volume30 >> 16, state->panning38 >> 16, state->channel4A,
        state->set4B, command->word[1], state->section2F, 0, state->groupBE);
    state->activeBD = 0;
    if (child != 0xFFFFFFFFU) {
        ((MacroStatePlayMacro *)&sound_synth_voices[child & 255])->parent14 = state->identifier60;
        if (state->child10 != 0xFFFFFFFFU) {
            ((MacroStatePlayMacro *)&sound_synth_voices[child & 255])->child10 = state->child10;
            ((MacroStatePlayMacro *)&sound_synth_voices[state->child10 & 255])->parent14 = child;
        }
        state->child10 = child;
        if (state->external4C) sound_synth_copy_controllers((MacroStatePlayMacro *)&sound_synth_voices[child & 255], state);
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_send_key_off
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
typedef struct MacroStateSendKeyOff { u8 unknown00[0x4E]; u16 keyGroup4E; } MacroStateSendKeyOff;
#pragma pack(0)
extern u8 sound_voice_count;
extern int sound_synth_key_off_voice(u32 id);
u8 sound_macro_send_key_off(MacroStateSendKeyOff *voice, MacroCommand *command)
{
    u32 i;
    u32 id;
    id = (((command->word[0] >> 8) & 255) + voice->keyGroup4E) << 8;
    id |= (u32)(u16)(command->word[0] >> 16) << 16;
    for (i = 0; i < sound_voice_count; ++i) {
        if ((id | i) == sound_synth_voices[i].id60) {
            sound_synth_key_off_voice(id | i);
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_add_age
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateAddAge { u8 unknown00[0x28]; u32 field28; u16 field2C; u8 unknown2E[2]; u32 field30; u8 unknown34[4]; s32 field38; u8 unknown3C[0x14]; u16 field50; } MacroStateAddAge;
#pragma pack(0)
typedef struct MacroCommandAddAge { u32 word[2]; } MacroCommandAddAge;
u8 sound_macro_add_age(MacroStateAddAge *state, MacroCommandAddAge *command)
{
    s16 delta;
    s32 value;
    delta = command->word[0] >> 16;
    value = (state->field28 >> 15) + delta;
    if (value < 0) {
        state->field28 = 0;
    } else if (value > 65535) {
        state->field28 = 0x7FFF8000;
    } else {
        state->field28 = (u32)value << 15;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_set_age
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetAge { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateSetAge;
#pragma pack(0)
typedef struct MacroCommandSetAge { u32 word[2]; } MacroCommandSetAge;
u8 sound_macro_set_age(MacroStateSetAge *state, MacroCommandSetAge *command)
{
    state->field28 = ((u16)(command->word[0] >> 16)) << 15;
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_set_age_speed
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetAgeSpeed { u8 unknown00[0x28]; u32 field28; u16 field2C; u8 unknown2E[2]; u32 field30; u8 unknown34[4]; s32 field38; u8 unknown3C[0x14]; u16 field50; } MacroStateSetAgeSpeed;
#pragma pack(0)
typedef struct MacroCommandSetAgeSpeed { u32 word[2]; } MacroCommandSetAgeSpeed;
u8 sound_macro_set_age_speed(MacroStateSetAgeSpeed *state, MacroCommandSetAgeSpeed *command)
{
    u32 divisor;
    divisor = command->word[1];
    if (divisor != 0) {
        state->field2C = (state->field28 >> 8) / divisor;
    } else {
        state->field2C = 0;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_set_age_by_velocity
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetAgeByVelocity { u8 unknown00[0x28]; u32 field28; u16 field2C; u8 unknown2E[2]; u32 field30; u8 unknown34[4]; s32 field38; u8 unknown3C[0x14]; u16 field50; } MacroStateSetAgeByVelocity;
#pragma pack(0)
typedef struct MacroCommandSetAgeByVelocity { u32 word[2]; } MacroCommandSetAgeByVelocity;
u8 sound_macro_set_age_by_velocity(MacroStateSetAgeByVelocity *state, MacroCommandSetAgeByVelocity *command)
{
    u32 value;
    value = (u16)(command->word[0] >> 16)
          + (((command->word[1] & 0xFFFF) * (state->field30 >> 16)) >> 7);
    if (value > 60000) {
        state->field28 = 0x75300000;
    } else {
        state->field28 = value << 15;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_add_priority
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateAddPriority { u32 field00; u32 field04; u32 field08; u32 field0C; u8 unknown10[0xC]; enum n64p_MacroCommand field1C; enum n64p_MacroCommand field20; u8 unknown24[0xA]; u8 field2E; } MacroStateAddPriority;
#pragma pack(0)
extern void sound_set_voice_priority(MacroState *, u8);
u8 sound_macro_add_priority(MacroStateAddPriority *state, MacroCommand *command)
{
    s32 delta;
    s32 value;
    delta = (s16)(command->word[0] >> 16);
    value = (s16)(state->field2E + delta);
    if (value < 0) {
        value = 0;
    } else {
        value = (s16)(value > 255 ? 255 : value);
    }
    sound_set_voice_priority((MacroState *)state, (u8)value);
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_set_priority
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
u8 sound_macro_set_priority(MacroState *state, MacroCommand *command)
{
    sound_set_voice_priority(state, (u8)(command->word[0] >> 8));
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_send_flag
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSendFlag { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateSendFlag;
#pragma pack(0)
typedef struct MacroCommandSendFlag { u32 word[2]; } MacroCommandSendFlag;
extern u16 sound_global_variables[];
u8 sound_macro_send_flag(MacroStateSendFlag *state, MacroCommandSendFlag *command)
{
    sound_global_variables[(u8)(command->word[0] >> 8)] = (u8)(command->word[0] >> 16);
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_set_pitch_wheel_range
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetPitchWheelRange { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateSetPitchWheelRange;
#pragma pack(0)
typedef struct MacroCommandSetPitchWheelRange { u32 word[2]; } MacroCommandSetPitchWheelRange;
u8 sound_macro_set_pitch_wheel_range(MacroStateSetPitchWheelRange *state, MacroCommandSetPitchWheelRange *command)
{
    state->field6A = command->word[0] >> 16;
    state->field6B = command->word[0] >> 8;
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_set_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetNote { u8 unknown00[0x4A]; u8 channel4A; u8 set4B; u8 unknown4C[4]; u16 note50; u8 unknown52[0x6E]; s8 detuneC0; u8 lastNoteC1; } MacroStateSetNote;
#pragma pack()
extern void sound_midi_set_last_key(u8, u8, u8);
extern int sound_synth_owns_channel(MacroState *);
u8 sound_macro_set_note(MacroStateSetNote *state, MacroCommand *command)
{
    state->note50 = (u8)(command->word[0] >> 8) & 0x7F;
    state->detuneC0 = command->word[0] >> 16;
    if (sound_synth_owns_channel((MacroState *)state)) {
        sound_midi_set_last_key(state->channel4A, state->set4B, state->note50);
    }
    command->word[0] = 4;
    return sound_macro_wait((MacroState *)state, command);
}

/* PROMOTED 2026-10-05 — sound_macro_add_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateAddNote { u8 unknown00[0x4A]; u8 channel4A; u8 set4B; u8 unknown4C[2]; u16 originalNote4E; u16 note50; u8 unknown52[0x6E]; s8 detuneC0; } MacroStateAddNote;
#pragma pack()
u8 sound_macro_add_note(MacroStateAddNote *state, MacroCommand *command)
{
    if ((u8)(command->word[0] >> 24) == 0) {
        state->note50 += (s8)(u8)(command->word[0] >> 8);
    } else {
        state->note50 = state->originalNote4E + (s8)(u8)(command->word[0] >> 8);
    }
    state->note50 = (s16)state->note50 < 0 ? 0 : state->note50 > 127 ? 127 : state->note50;
    state->detuneC0 = command->word[0] >> 16;
    if (sound_synth_owns_channel((MacroState *)state)) {
        sound_midi_set_last_key(state->channel4A, state->set4B, state->note50);
    }
    command->word[0] = 4;
    return sound_macro_wait((MacroState *)state, command);
}

/* PROMOTED 2026-10-11 — sound_macro_last_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateLastNote { u8 unknown00[0x4A]; u8 channel4A; u8 set4B; u8 unknown4C[4]; u16 note50; u8 unknown52[0x6E]; s8 detuneC0; u8 lastNoteC1; } MacroStateLastNote;
#pragma pack()
u8 sound_macro_last_note(MacroStateLastNote *state, MacroCommand *command)
{
    state->note50 = state->lastNoteC1 + (s8)(u8)(command->word[0] >> 8);
    state->note50 = (s16)state->note50 < 0 ? 0 : state->note50 > 127 ? 127 : state->note50;
    state->detuneC0 = command->word[0] >> 16;
    if (state->channel4A != 255) {
        sound_midi_set_last_key(state->channel4A, state->set4B, state->note50);
    }
    command->word[0] = 4;
    return sound_macro_wait((MacroState *)state, command);
}

/* PROMOTED 2026-10-11 — sound_macro_start_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the record sound_find_sample fills and sound_hw_set_voice_sample reads, both with a pointer-sized, packed address field
 * (a stream's host buffer in sound_stream_start's call); here it holds the sample's cartridge address. */
#pragma pack(1)
typedef struct MacroSampleInfo { u32 info; void *address; u32 offset; u32 length; u32 loopStart; u32 loopLength; u8 type; } MacroSampleInfo;
#pragma pack()
#pragma pack(1)
typedef struct MacroStateStartSample { u8 unknown00[0x24]; u32 flags24; u8 unknown28[8]; u32 volume30; u8 unknown34[0x24]; u32 sampleAddress58; u32 sampleInfo5C; u32 identifier60; } MacroStateStartSample;
#pragma pack(0)
extern int sound_find_sample(u16, MacroSampleInfo *);
extern void sound_hw_set_voice_sample(u32, MacroSampleInfo *, u8);
static u8 macro_steps_this_update;
u8 sound_macro_start_sample(MacroStateStartSample *state, MacroCommand *command)
{
    static MacroSampleInfo sample_80056208;
    u32 index;
    if (sound_find_sample(command->word[0] >> 8, &sample_80056208) == 0) {
        switch ((u8)(command->word[0] >> 24)) {
        case 0:
            sample_80056208.offset = command->word[1];
            break;
        case 1:
            sample_80056208.offset = (command->word[1] * (127U - (state->volume30 >> 16))) / 127U;
            break;
        case 2:
            sample_80056208.offset = (command->word[1] * (state->volume30 >> 16)) / 127U;
            break;
        default:
            sample_80056208.offset = 0;
        }
        if (sample_80056208.offset >= sample_80056208.length) {
            sample_80056208.offset = sample_80056208.length - 1;
        }
        index = state->identifier60 & 255;
        if (!(state->flags24 & 0x200)) {
            sound_hw_set_voice_sample(index, &sample_80056208, 1);
        } else {
            sound_hw_set_voice_sample(index, &sample_80056208, 0);
        }
        state->sampleInfo5C = sample_80056208.info;
        state->sampleAddress58 = sample_80056208.address;
        state->flags24 |= 0x20;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_stop_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateStopSample { u8 unknown00[0x30]; u32 field30; u8 unknown34[0x2C]; u32 field60; } MacroStateStopSample;
#pragma pack(0)
extern void sound_hw_stop_sample(u32, u8);
u8 sound_macro_stop_sample(MacroStateStopSample *state, MacroCommand *command)
{
    sound_hw_stop_sample(state->field60 & 0xFF, 1);
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_key_off
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateKeyOff { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateKeyOff;
#pragma pack(0)
typedef struct MacroCommandKeyOff { u32 word[2]; } MacroCommandKeyOff;
u8 sound_macro_key_off(MacroStateKeyOff *state, MacroCommandKeyOff *command)
{
    state->flags24 |= 0x80;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_vibrato
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateVibrato { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x44]; u32 currentTime6C; u32 period70; u8 unknown74[4]; u8 keyRange78; u8 centRange79; } MacroStateVibrato;
#pragma pack()
extern void sound_ms_to_time(u32 *);
extern void sound_ticks_to_time(u32 *, MacroState *);
u8 sound_macro_vibrato(MacroStateVibrato *state, MacroCommand *command)
{
    u32 time;
    s8 keyRange;
    s8 centRange;
    if ((u8)(command->word[0] >> 24) & 3) {
        state->flags24 |= 0x8000;
    } else {
        state->flags24 &= ~0x8000;
    }
    time = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&time);
    } else {
        sound_ticks_to_time(&time, (MacroState *)state);
    }
    if (time) {
        state->flags24 |= 0x4000;
        state->period70 = time;
        keyRange = command->word[0] >> 8;
        centRange = command->word[0] >> 16;
        if (keyRange < 0) {
            if (centRange < 0) state->centRange79 = -centRange;
            else state->centRange79 = centRange;
            state->keyRange78 = -keyRange;
            state->currentTime6C = state->period70 / 2;
        } else {
            if (centRange < 0) {
                if (keyRange == 0) {
                    state->centRange79 = -centRange;
                    state->currentTime6C = state->period70 / 2;
                } else {
                    --keyRange;
                    state->centRange79 = 100 - centRange;
                    state->currentTime6C = 0;
                }
            } else {
                state->centRange79 = centRange;
                state->currentTime6C = 0;
            }
            state->keyRange78 = keyRange;
        }
    } else {
        state->flags24 &= ~0x4000;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_setup_lfo
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct MacroCommandSetupLfo { u32 word[2]; } MacroCommandSetupLfo;
#pragma pack(1)
typedef struct MacroStateSetupLfo { u32 field00; u32 field04; u32 field08; u32 field0C; u8 unknown10[0xC]; enum n64p_MacroCommand field1C; enum n64p_MacroCommand field20; u8 unknown24[0xA]; u8 field2E; } MacroStateSetupLfo;
typedef struct MacroLfoControl { u32 field0; u32 field4; u32 field8; } MacroLfoControl;
#pragma pack(0)
extern void sound_ms_to_time(u32 *);
u8 sound_macro_setup_lfo(MacroStateSetupLfo *state, MacroCommandSetupLfo *command)
{
    u32 duration;
    u8 index;
    MacroLfoControl *control;
    index = command->word[0] >> 8;
    duration = (u16)(command->word[0] >> 16);
    sound_ms_to_time(&duration);
    control = (MacroLfoControl *)((u8 *)state + 0x168) + index;
    if (control->field4 != 0) {
        control->field0 = 0;
    }
    control->field4 = duration;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_set_pitch
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetPitch { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x28]; u16 note50; u8 unknown52[0xA]; u32 sample5C; u8 unknown60[0x60]; s8 detuneC0; } MacroStateSetPitch;
#pragma pack(0)
typedef struct MacroCommandSetPitch { u32 word[2]; } MacroCommandSetPitch;
u8 sound_macro_set_pitch(MacroStateSetPitch *state, MacroCommandSetPitch *command)
{
    u32 f;
    u32 of;
    u32 i;
    u32 frq;
    u32 ofrq;
    u32 no;
    static u16 kf[13] = {
        4096, 4339, 4597, 4871, 5160, 5467, 5792, 6137, 6502, 6888, 7298, 7732, 8192,
    };

    frq = (command->word[0] >> 8) & 0xFFFF;
    ofrq = state->sample5C & 0xFFFFFF;
    if (ofrq == frq) {
        state->note50 = (u8)(state->sample5C >> 24);
        state->detuneC0 = 0;
    } else if (ofrq < frq) {
        f = (frq << 12) / ofrq;
        of = f >> 12;
        for (no = 0; no < 11; no++) {
            if (of < (1 << (no + 1))) {
                break;
            }
        }
        f /= 1 << no;
        for (i = 11; i >= 0; i--) {
            if (f > kf[i]) {
                break;
            }
        }
        state->note50 = (u8)(state->sample5C >> 24) + no * 12 + i;
        state->detuneC0 = ((f - kf[i]) * 100) / (kf[i + 1] - kf[i]);
    } else {
        f = (ofrq << 12) / frq;
        of = f >> 12;
        for (no = 0; no < 11; no++) {
            if (of < (1 << (no + 1))) {
                break;
            }
        }
        f /= 1 << no;
        for (i = 11; i >= 0; i--) {
            if (f > kf[i]) {
                break;
            }
        }
        state->note50 = (u8)(state->sample5C >> 24) - no * 12 - i;
        state->detuneC0 = -(s8)(((f - kf[i]) * 100) / (kf[i + 1] - kf[i]));
    }
    state->flags24 |= 0x100;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_set_adsr
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetAdsr { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x38]; u32 identifier60; } MacroStateSetAdsr;
typedef struct PackedEnvelope { u16 attack, decay, sustain, release; } PackedEnvelope;
#pragma pack()
typedef struct Envelope { u16 attack, decay, sustain, release; } Envelope;
extern void *sound_find_table(u16);
extern void sound_hw_set_envelope(u32, const Envelope *);
u8 sound_macro_set_adsr(MacroStateSetAdsr *state, MacroCommand *command)
{
    Envelope envelope;
    PackedEnvelope *table;
    if ((table = sound_find_table(command->word[0] >> 8)) != 0) {
        envelope.attack = (table->attack >> 8) | (table->attack << 8);
        envelope.decay = (table->decay >> 8) | (table->decay << 8);
        envelope.sustain = (table->sustain >> 8) | (table->sustain << 8);
        envelope.release = (table->release >> 8) | (table->release << 8);
        sound_hw_set_envelope(state->identifier60 & 0xFF, &envelope);
        state->flags24 |= 0x200;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_pitch_sweep
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct MacroStatePitchSweep { u8 unknown00[0x40]; u32 sweepOffset40[2]; u8 sweepNumber48[2]; u8 unknown4A[0x32]; u32 sweepAddition7C[2]; u8 unknown84[0x2C]; u8 sweepCountB0[2]; } MacroStatePitchSweep;
extern u32 sound_rate_to_pitch(u32);
u8 sound_macro_pitch_sweep(MacroStatePitchSweep *state, MacroCommand *command, int index)
{
    s32 delta;
    state->sweepOffset40[index] = 0;
    state->sweepNumber48[index] = (u8)(command->word[0] >> 8);
    state->sweepCountB0[index] = state->sweepNumber48[index];
    delta = (s16)(command->word[0] >> 16);
    if (delta >= 0) {
        delta = (s32)sound_rate_to_pitch(delta);
    } else {
        delta = (s32)(0U - sound_rate_to_pitch(-delta));
    }
    state->sweepAddition7C[index] = (u32)delta << 16;
    command->word[0] = 0;
    return sound_macro_wait((MacroState *)state, command);
}

/* PROMOTED 2026-10-11 — sound_macro_panning
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct SynthVoicePanView { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x10]; u32 pan38; u32 panDelta3C; u8 unknown40[0x68]; u32 panTimeA8; u32 panTargetAC; } SynthVoicePanView;
#pragma pack(0)
u8 sound_macro_panning(SynthVoicePanView *voice, MacroCommand *command)
{
    u32 unused0, unused1;
    u32 time;
    s8 offset;
    voice->flags24 |= 0x10000;
    time = (command->word[0] >> 16) & 65535;
    voice->panTimeA8 = time;
    sound_ms_to_time(&voice->panTimeA8);
    offset = command->word[1] & 0xFF;
    voice->pan38 = ((command->word[0] >> 8) & 255) << 16;
    voice->panTargetAC = ((u32)offset << 16) + voice->pan38;
    if (voice->panTimeA8 != 0) {
        voice->panDelta3C = ((u32)offset << 16) / time;
    } else {
        voice->panDelta3C = (u32)offset << 16;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_piano_pan
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
u8 sound_macro_piano_pan(MacroStateAddAge *state, MacroCommand *command)
{
    s32 value;
    s8 gain;
    value = ((u32)state->field50 << 16) - (((command->word[0] >> 16) & 0xFF) << 16);
    gain = command->word[0] >> 8;
    value = (s32)((u32)value * gain);
    value >>= 7;
    value += (u8)(command->word[0] >> 24) << 16;
    value = value < 0 ? 0 : (value > 0x7F0000 ? 0x7F0000 : value);
    state->field38 = value;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_apply_curve
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
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

/* PROMOTED 2026-10-11 — sound_macro_scale_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateScaleVolume { u8 unknown00[0x30]; u32 volume30; u32 originalVolume34; } MacroStateScaleVolume;
#pragma pack()
extern u32 sound_macro_apply_curve(u32, u16);
u8 sound_macro_scale_volume(MacroStateScaleVolume *state, MacroCommand *command)
{
    u16 curve;
    if ((u8)(command->word[1] >> 8) == 0) {
        state->volume30 = ((u8)(command->word[0] >> 8) * state->volume30) >> 7;
    } else {
        state->volume30 = ((u8)(command->word[0] >> 8) * state->originalVolume34) >> 7;
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

/* PROMOTED 2026-10-11 — sound_macro_start_envelope
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateStartEnvelope { u8 unknown00[0x24]; u32 flags24; u8 unknown28[8]; u32 volume30; u8 unknown34[0x50]; s32 delta84; u32 target88; u8 unknown8C[0x28]; u32 timeB4; } MacroStateStartEnvelope;
#pragma pack()
extern u32 sound_time_to_ms(u32);
#define ENVELOPE_VOICE ((MacroStateStartEnvelope *)state)
u8 sound_macro_start_envelope(MacroState *state, MacroCommand *command, u32 start)
{
    u32 target;
    s32 milliseconds;
    u16 curve;
    ENVELOPE_VOICE->timeB4 = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&ENVELOPE_VOICE->timeB4);
    } else {
        sound_ticks_to_time(&ENVELOPE_VOICE->timeB4, state);
    }
    milliseconds = sound_time_to_ms(ENVELOPE_VOICE->timeB4);
    if (milliseconds == 0) milliseconds = 1;
    target = ((u8)(command->word[0] >> 8) * ENVELOPE_VOICE->volume30) >> 7;
    target += (u8)(command->word[0] >> 16) << 16;
    if (target > 0x7F0000) target = 0x7F0000;
    curve = (u8)command->word[1];
    curve |= (u8)(command->word[0] >> 24) << 8;
    target = sound_macro_apply_curve(target, curve);
    ENVELOPE_VOICE->target88 = target;
    ENVELOPE_VOICE->delta84 = (s32)(target - start) / milliseconds;
    ENVELOPE_VOICE->volume30 = start;
    ENVELOPE_VOICE->flags24 |= 0x20000;
    return 0;
}

extern u8 sound_macro_start_envelope(MacroState *, MacroCommand *, u32);
/* PROMOTED 2026-10-05 — sound_macro_envelope
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateEnvelope { u8 unknown00[0x30]; u32 field30; u8 unknown34[0x2C]; u32 field60; } MacroStateEnvelope;
#pragma pack(0)
u8 sound_macro_envelope(MacroStateEnvelope *state, MacroCommand *command)
{
    return sound_macro_start_envelope((MacroState *)state, command, state->field30);
}

/* PROMOTED 2026-10-04 — sound_macro_fade_in
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_fade_in(MacroState *state, MacroCommand *command)
{
    return sound_macro_start_envelope(state, command, 0);
}

/* PROMOTED 2026-10-11 — sound_macro_random_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
extern u8 sound_macro_set_note(MacroStateSetNote *state, MacroCommand *command);
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
        lower = state->note50 - (u8)(command->word[0] >> 8);
        upper = state->note50 + (u8)(command->word[0] >> 24);
        low = lower < 0 ? 0 : lower > 127 ? 127 : (u8)lower;
        high = upper < 0 ? 0 : upper > 127 ? 127 : (u8)upper;
    }
    if (command->word[1] & 0xFF) {
        detune = (sound_random() % 201) - 100;
    } else {
        detune = command->word[0] >> 16;
    }
    command->word[0] = ((low + (sound_random() % ((high - low) + 1))) * 256) | 0x19 | (detune << 16);
    command->word[1] = 0;
    sound_macro_set_note(state, command);
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_track_pitch_wheel
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateTrackPitchWheel { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateTrackPitchWheel;
#pragma pack(0)
typedef struct MacroCommandTrackPitchWheel { u32 word[2]; } MacroCommandTrackPitchWheel;
u8 sound_macro_track_pitch_wheel(MacroStateTrackPitchWheel *state, MacroCommandTrackPitchWheel *command)
{
    state->flags24 |= 0x40000;
    return 0;
}

/* PROMOTED 2026-10-04 — sound_macro_set_reverb_send
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateSetReverbSend { u8 unknown00[0x1C]; u32 field1C; u32 field20; u32 flags24; u32 field28; u8 unknown2C[0x3E]; u8 field6A; u8 field6B; u8 unknown6C[0x2D]; u8 field99; u8 field9A; } MacroStateSetReverbSend;
#pragma pack(0)
typedef struct MacroCommandSetReverbSend { u32 word[2]; } MacroCommandSetReverbSend;
u8 sound_macro_set_reverb_send(MacroStateSetReverbSend *state, MacroCommandSetReverbSend *command)
{
    state->field99 = command->word[0] >> 8;
    state->field9A = command->word[0] >> 16;
    return 0;
}

typedef struct MacroStateSelectVolume MacroStateSelectVolume;
typedef struct MacroInput MacroInput;
typedef struct MacroCommandSelectVolume { u32 word[2]; } MacroCommandSelectVolume;
extern void sound_macro_add_input_source(MacroStateSelectVolume *state, MacroInput *control, MacroCommandSelectVolume *command, u32 flag);
/* PROMOTED 2026-10-11 — sound_macro_add_input_source
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
struct MacroStateSelectVolume { u8 unknown00[0x24]; u32 flags24; };
typedef struct MacroInputSource { u8 controller; u8 combination; s16 scale; } MacroInputSource;
struct MacroInput { MacroInputSource entries[4]; u8 count; };
#pragma pack()
extern u8 sound_macro_map_controller(u8);
void sound_macro_add_input_source(MacroStateSelectVolume *state, MacroInput *control,
                  MacroCommandSelectVolume *command, u32 flag)
{
    u8 index;
    u8 controller;
    u8 combination;
    combination = command->word[1] & 0xFF;
    if (!(state->flags24 & flag) || combination == 0) {
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

/* PROMOTED 2026-10-04 — sound_macro_select_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_volume(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0xC4),
                  command, 0x00200000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_pan
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_pan(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0xD6),
                  command, 0x00400000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_pitch_bend
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_pitch_bend(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0xFA),
                  command, 0x00800000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_modulation
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_modulation(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0x11E),
                  command, 0x01000000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_pedal
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_pedal(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0x130),
                  command, 0x08000000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_portamento
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_portamento(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0x142),
                  command, 0x04000000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_reverb
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_reverb(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0x154),
                  command, 0x02000000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_span
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_span(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0xE8),
                  command, 0x10000000);
    return 0;
}

/* PROMOTED 2026-10-05 — sound_macro_select_doppler
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 sound_macro_select_doppler(MacroStateSelectVolume *state, MacroCommandSelectVolume *command)
{
    sound_macro_add_input_source(state, (MacroInput *)((u8 *)state + 0x10C),
                  command, 0x20000000);
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_portamento
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStatePortamento { u8 unknown00[0x24]; u32 flags24; u8 unknown28[0x22]; u8 channel4A; u8 set4B; u8 unknown4C[0x44]; u32 time90; u8 unknown94[4]; u8 mode98; } MacroStatePortamento;
#pragma pack()
extern void sound_midi_set_portamento_state(u8, u8, u8);
extern void sound_synth_start_portamento(MacroStatePortamento *);
u8 sound_macro_portamento(MacroStatePortamento *state, MacroCommand *command)
{
    u32 time;
    state->mode98 = command->word[0] >> 16;
    time = (u16)(command->word[1] >> 16);
    if ((u8)(command->word[1] >> 8) & 1) {
        sound_ms_to_time(&time);
    } else {
        sound_ticks_to_time(&time, (MacroState *)state);
    }
    state->time90 = time;
    switch ((u8)(command->word[0] >> 8)) {
    case 0:
        state->flags24 &= ~0x800;
        state->flags24 |= 0x1000;
        if (state->channel4A != 255) sound_midi_set_portamento_state(state->channel4A, state->set4B, 0);
        break;
    case 1:
        if (!(state->flags24 & 0x800)) sound_synth_start_portamento(state);
        state->flags24 |= 0x1800;
        break;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_get_variable
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct SynthVoiceVariablesView { u8 unknown000[0x180]; s16 variables180[16]; } SynthVoiceVariablesView;
#pragma pack(0)
extern s16 sound_seq_current_index[];
extern u16 sound_macro_get_controller(VoiceState *voice, u8 controller);
s16 sound_macro_get_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index)
{
    s32 value;
    if (isController) {
        return sound_macro_get_controller((VoiceState *)voice, index);
    }
    index &= 31;
    value = index < 16 ? voice->variables180[index] : (&sound_seq_current_index[16])[index - 16];
    return value;
}

/* PROMOTED 2026-10-11 — sound_macro_set_variable
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern void sound_macro_set_controller(VoiceState *voice, u8 controller, s16 value);
void sound_macro_set_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index, s16 value)
{
    if (isController) {
        sound_macro_set_controller((VoiceState *)voice, index, value);
    } else {
        index &= 31;
        if (index < 16) {
            voice->variables180[index] = value;
        } else {
            (&sound_seq_current_index[16])[index - 16] = value;
        }
    }
}

/* PROMOTED 2026-10-11 — sound_macro_variable_arithmetic
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern s16 sound_macro_get_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index);
extern void sound_macro_set_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index, s16 value);
u8 sound_macro_variable_arithmetic(SynthVoiceVariablesView *voice, MacroCommand *command, u8 operation)
{
    s16 left;
    s16 right;
    s32 result;

    left = sound_macro_get_variable(voice, (command->word[0] >> 24) & 255, command->word[1] & 255);
    if (operation == 4) {
        right = (s16)(command->word[1] >> 8);
    } else {
        right = sound_macro_get_variable(voice, (command->word[1] >> 8) & 255, (command->word[1] >> 16) & 255);
    }
    switch (operation) {
    case 4:
    case 0: result = left + right; break;
    case 1: result = left - right; break;
    case 2: result = left * right; break;
    case 3: result = right != 0 ? left / right : 0; break;
    }
    sound_macro_set_variable(voice, (command->word[0] >> 8) & 255, (command->word[0] >> 16) & 255,
                 result < -32768 ? -32768 : result > 32767 ? 32767 : result);
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_set_variable_immediate
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern void sound_macro_set_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index, s16 value);
u8 sound_macro_set_variable_immediate(MacroState *state, MacroCommand *command)
{
    sound_macro_set_variable((SynthVoiceVariablesView *)state, command->word[0] >> 8, command->word[0] >> 16,
                  (s16)(command->word[1] & 0xFFFF));
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_compare
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct MacroStateCompare { enum n64p_MacroCommand program; enum n64p_MacroCommand current; } MacroStateCompare;
#pragma pack(0)
extern s16 sound_macro_get_variable(SynthVoiceVariablesView *voice, u8 isController, u8 index);
u8 sound_macro_compare(MacroStateCompare *voice, MacroCommand *command, u8 comparison)
{
    s16 left;
    s16 right;
    u8 condition;
    left = sound_macro_get_variable((SynthVoiceVariablesView *)voice, command->word[0] >> 8, command->word[0] >> 16);
    right = sound_macro_get_variable((SynthVoiceVariablesView *)voice, command->word[0] >> 24, command->word[1] & 0xFF);
    switch (comparison) {
    case 0: condition = left == right; break;
    case 1: condition = left < right; break;
    }
    if ((command->word[1] >> 8) & 255) {
        condition = !condition;
    }
    if (condition) {
        voice->current = N64W(((command->word[1] >> 16) & 65535) + N64P(voice->program));
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_macro_run
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
struct VoiceState;
struct VoiceControls;
#pragma pack(1)
typedef struct SynthVoiceMacroView { enum n64p_MacroCommand addr00; enum n64p_MacroCommand curAddr04; u8 unknown08[0x14]; enum n64p_MacroCommand trapAddr1C; enum n64p_MacroCommand trapCurAddr20; u32 cFlags24; u8 unknown28[7]; u8 section2F; u32 volume30; u32 orgVolume34; u32 panning38; u8 unknown3C[4]; u32 sweepOff40[2]; u8 sweepNum48[2]; u8 midi4A; u8 midiSet4B; u8 unknown4C[2]; u16 orgNote4E; u8 unknown50[2]; u8 setupVol52; u8 setupPan53; u8 setupMidi54; u8 setupMidiSet55; u8 setupSection56; u8 unknown57[5]; u32 field5C; u32 id60; u8 unknown64[4]; u16 field68; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u8 unknown6C[0x20]; u32 portTime8C; u32 portDuration90; u8 unknown94[4]; u8 portType98; u8 revVolScale99; u8 revVolOffset9A; u8 unknown9B; u32 wait9C; u32 waitTimeA0; u32 timeA4; u8 unknownA8[0x10]; u16 pbLastB8; u8 unknownBA[4]; u8 vGroupBE; u8 setupVGroupBF; u8 unknownC0; u8 lastNoteC1; u16 curOutputVolumeC2; u8 unknownC4[0xA8]; u32 lfoPeriod16C; s16 lfoValue170; u8 unknown172[6]; u32 lfoPeriod178; s16 lfoValue17C; u8 unknown17E[2]; s16 localVars180[16]; } SynthVoiceMacroView;
#pragma pack(0)
struct MacroStateReturn;
struct MacroStateSplitKey;
struct MacroStateSplitVelocity;
struct MacroStateLoop;
struct MacroStateGosub;
struct MacroStatePlayMacro;
struct MacroStateSendKeyOff;
struct MacroStateSplitModulation;
struct MacroStateAddAge;
struct MacroStateSetAdsr;
struct MacroStateScaleVolume;
struct SynthVoicePanView;
struct MacroStateEnvelope;
struct MacroStateStartSample;
struct MacroStateStopSample;
struct MacroStateKeyOff;
struct MacroCommandKeyOff;
struct MacroStateSplitRandom;
struct MacroStateSetNote;
struct MacroStateAddNote;
struct MacroStateLastNote;
struct MacroStatePortamento;
struct MacroStateVibrato;
struct MacroStatePitchSweep;
struct MacroStateSetPitch;
struct MacroCommandSetPitch;
struct MacroStateTrapKeyOff;
struct MacroStateUntrapKeyOff;
struct MacroCommandUntrapKeyOff;
struct MacroCommandAddAge;
struct MacroStateSetAge;
struct MacroCommandSetAge;
struct MacroStateSendFlag;
struct MacroCommandSendFlag;
struct MacroStateSetPitchWheelRange;
struct MacroCommandSetPitchWheelRange;
struct MacroStateSetReverbSend;
struct MacroCommandSetReverbSend;
struct MacroStateTrackPitchWheel;
struct MacroCommandTrackPitchWheel;
struct MacroStateAddPriority;
struct MacroStateSetAgeSpeed;
struct MacroCommandSetAgeSpeed;
struct MacroStateSetAgeByVelocity;
struct MacroCommandSetAgeByVelocity;
struct MacroStateSelectVolume;
struct MacroCommandSelectVolume;
struct MacroStateSetupLfo;
struct MacroCommandSetupLfo;
struct SynthVoiceVariablesView;
struct MacroStateCompare;
extern u8 sound_macro_return(struct MacroStateReturn *, MacroCommand *);
extern u8 sound_macro_split_key(struct MacroStateSplitKey *, MacroCommand *);
extern u8 sound_macro_split_velocity(struct MacroStateSplitVelocity *, MacroCommand *);
extern u8 sound_macro_loop(struct MacroStateLoop *, MacroCommand *);
extern u8 sound_macro_gosub(struct MacroStateGosub *, MacroCommand *);
extern u8 sound_macro_wait_ms(MacroState *, MacroCommand *);
extern u8 sound_macro_play_macro(struct MacroStatePlayMacro *, MacroCommand *);
extern u8 sound_macro_send_key_off(struct MacroStateSendKeyOff *, MacroCommand *);
extern u8 sound_macro_split_modulation(struct MacroStateSplitModulation *, MacroCommand *);
extern u8 sound_macro_piano_pan(struct MacroStateAddAge *, MacroCommand *);
extern u8 sound_macro_set_adsr(struct MacroStateSetAdsr *, MacroCommand *);
extern u8 sound_macro_scale_volume(struct MacroStateScaleVolume *, MacroCommand *);
extern u8 sound_macro_panning(struct SynthVoicePanView *, MacroCommand *);
extern u8 sound_macro_envelope(struct MacroStateEnvelope *, MacroCommand *);
extern u8 sound_macro_start_sample(struct MacroStateStartSample *, MacroCommand *);
extern u8 sound_macro_stop_sample(struct MacroStateStopSample *, MacroCommand *);
extern u8 sound_macro_key_off(struct MacroStateKeyOff *, struct MacroCommandKeyOff *);
extern u8 sound_macro_split_random(struct MacroStateSplitRandom *, MacroCommand *);
extern u8 sound_macro_fade_in(MacroState *, MacroCommand *);
extern u8 sound_macro_random_note(struct MacroStateSetNote *, MacroCommand *);
extern u8 sound_macro_add_note(struct MacroStateAddNote *, MacroCommand *);
extern u8 sound_macro_set_note(struct MacroStateSetNote *, MacroCommand *);
extern u8 sound_macro_last_note(struct MacroStateLastNote *, MacroCommand *);
extern u8 sound_macro_portamento(struct MacroStatePortamento *, MacroCommand *);
extern u8 sound_macro_vibrato(struct MacroStateVibrato *, MacroCommand *);
extern u8 sound_macro_pitch_sweep(struct MacroStatePitchSweep *, MacroCommand *, int);
extern u8 sound_macro_set_pitch(struct MacroStateSetPitch *, struct MacroCommandSetPitch *);
extern u8 sound_macro_trap_key_off(struct MacroStateTrapKeyOff *, MacroCommand *);
extern u8 sound_macro_untrap_key_off(struct MacroStateUntrapKeyOff *, struct MacroCommandUntrapKeyOff *);
extern u8 sound_macro_add_age(struct MacroStateAddAge *, struct MacroCommandAddAge *);
extern u8 sound_macro_set_age(struct MacroStateSetAge *, struct MacroCommandSetAge *);
extern u8 sound_macro_send_flag(struct MacroStateSendFlag *, struct MacroCommandSendFlag *);
extern u8 sound_macro_set_pitch_wheel_range(struct MacroStateSetPitchWheelRange *, struct MacroCommandSetPitchWheelRange *);
extern u8 sound_macro_set_reverb_send(struct MacroStateSetReverbSend *, struct MacroCommandSetReverbSend *);
extern u8 sound_macro_track_pitch_wheel(struct MacroStateTrackPitchWheel *, struct MacroCommandTrackPitchWheel *);
extern u8 sound_macro_set_priority(MacroState *, MacroCommand *);
extern u8 sound_macro_add_priority(struct MacroStateAddPriority *, MacroCommand *);
extern u8 sound_macro_set_age_speed(struct MacroStateSetAgeSpeed *, struct MacroCommandSetAgeSpeed *);
extern u8 sound_macro_set_age_by_velocity(struct MacroStateSetAgeByVelocity *, struct MacroCommandSetAgeByVelocity *);
extern u8 sound_macro_select_volume(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_pan(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_pitch_bend(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_modulation(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_pedal(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_portamento(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_reverb(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_span(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_select_doppler(struct MacroStateSelectVolume *, struct MacroCommandSelectVolume *);
extern u8 sound_macro_setup_lfo(struct MacroStateSetupLfo *, struct MacroCommandSetupLfo *);
extern u8 sound_macro_variable_arithmetic(struct SynthVoiceVariablesView *, MacroCommand *, u8);
extern u8 sound_macro_set_variable_immediate(MacroState *, MacroCommand *);
extern u8 sound_macro_compare(struct MacroStateCompare *, MacroCommand *, u8);
extern void sound_hw_break_voice(int);
extern void sound_synth_init_controller_inputs(struct VoiceControls *);
extern unsigned char sound_midi_get_last_key(unsigned char channel, unsigned char set);
extern void sound_synth_claim_channel(struct VoiceState *);
extern u32 sound_tick_step;
void sound_macro_run(SynthVoiceMacroView *voice)
{
    static MacroCommand cstep_80056228;
    u8 i;
    u8 lastNote;
    u8 ex;

    if (voice->cFlags24 & 3) {
        if (voice->cFlags24 & 1) {
            voice->cFlags24 &= ~1;
            sound_hw_break_voice(voice->id60 & 0xFF);
        }
        if (!(voice->cFlags24 & 0x40)) {
            sound_synth_init_controller_inputs((struct VoiceControls *)voice);
            voice->panning38 = voice->setupPan53 << 16;
            voice->volume30 = voice->setupVol52 << 16;
            voice->orgVolume34 = voice->volume30;
            voice->midi4A = voice->setupMidi54;
            voice->midiSet4B = voice->setupMidiSet55;
            voice->section2F = voice->setupSection56;
            if ((lastNote = sound_midi_get_last_key(voice->midi4A, voice->midiSet4B)) != 0xFF) {
                voice->lastNoteC1 = lastNote;
            } else {
                voice->lastNoteC1 = voice->orgNote4E;
            }
            sound_midi_set_last_key(voice->midi4A, voice->midiSet4B, voice->orgNote4E);
            sound_synth_claim_channel((struct VoiceState *)voice);
            voice->vGroupBE = voice->setupVGroupBF;
            voice->portTime8C = 0;
            voice->portDuration90 = 100;
            voice->portType98 = 0;
            voice->pbLowerKeyRange6A = 2;
            voice->pbUpperKeyRange6B = 2;
            voice->revVolScale99 = 128;
            voice->revVolOffset9A = 0;
            voice->field68 = 0;
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
            voice->field5C = 0x4000AC44;
            voice->pbLastB8 = 0x2000;
            voice->curOutputVolumeC2 = 0;
            voice->cFlags24 &= 8;
            for (i = 0; i < 16; i++) {
                voice->localVars180[i] = 0;
            }
            voice->waitTimeA0 = 0;
            voice->timeA4 = 0;
        } else {
            voice->cFlags24 = 0;
            sound_free_voice((MacroState *)voice);
            return;
        }
    }
    if (voice->trapCurAddr20 != 0 && !(voice->cFlags24 & 0x40000000) && (voice->cFlags24 & 8)) {
        voice->addr00 = voice->trapAddr1C;
        voice->curAddr04 = voice->trapCurAddr20;
        voice->wait9C = 0;
        voice->trapAddr1C = voice->trapCurAddr20 = 0;
    }
    if (voice->wait9C == 0) {
        macro_steps_this_update = 0;
        do {
            if (++macro_steps_this_update > 32) {
                break;
            }
            cstep_80056228.word[0] = N64P(voice->curAddr04)->word[0];
            cstep_80056228.word[1] = N64P(voice->curAddr04)->word[1];
            N64_NEXT(voice->curAddr04);
            switch (cstep_80056228.word[0] & 0x7F) {
            case 0x0:
                ex = sound_macro_end((MacroState *)voice, &cstep_80056228);
                break;
            case 0x1:
                ex = sound_macro_return((struct MacroStateReturn *)voice, &cstep_80056228);
                break;
            case 0x2:
                ex = sound_macro_split_key((struct MacroStateSplitKey *)voice, &cstep_80056228);
                break;
            case 0x3:
                ex = sound_macro_split_velocity((struct MacroStateSplitVelocity *)voice, &cstep_80056228);
                break;
            case 0x4:
                ex = sound_macro_wait((MacroState *)voice, &cstep_80056228);
                break;
            case 0x5:
                ex = sound_macro_loop((struct MacroStateLoop *)voice, &cstep_80056228);
                break;
            case 0x6:
                ex = sound_macro_gosub((struct MacroStateGosub *)voice, &cstep_80056228);
                break;
            case 0x7:
                ex = sound_macro_wait_ms((MacroState *)voice, &cstep_80056228);
                break;
            case 0x8:
                ex = sound_macro_play_macro((struct MacroStatePlayMacro *)voice, &cstep_80056228);
                break;
            case 0x9:
                ex = sound_macro_send_key_off((struct MacroStateSendKeyOff *)voice, &cstep_80056228);
                break;
            case 0xA:
                ex = sound_macro_split_modulation((struct MacroStateSplitModulation *)voice, &cstep_80056228);
                break;
            case 0xB:
                ex = sound_macro_piano_pan((struct MacroStateAddAge *)voice, &cstep_80056228);
                break;
            case 0xC:
                ex = sound_macro_set_adsr((struct MacroStateSetAdsr *)voice, &cstep_80056228);
                break;
            case 0xD:
                ex = sound_macro_scale_volume((struct MacroStateScaleVolume *)voice, &cstep_80056228);
                break;
            case 0xE:
                ex = sound_macro_panning((struct SynthVoicePanView *)voice, &cstep_80056228);
                break;
            case 0xF:
                ex = sound_macro_envelope((struct MacroStateEnvelope *)voice, &cstep_80056228);
                break;
            case 0x10:
                ex = sound_macro_start_sample((struct MacroStateStartSample *)voice, &cstep_80056228);
                break;
            case 0x11:
                ex = sound_macro_stop_sample((struct MacroStateStopSample *)voice, &cstep_80056228);
                break;
            case 0x12:
                ex = sound_macro_key_off((struct MacroStateKeyOff *)voice, (struct MacroCommandKeyOff *)&cstep_80056228);
                break;
            case 0x13:
                ex = sound_macro_split_random((struct MacroStateSplitRandom *)voice, &cstep_80056228);
                break;
            case 0x14:
                ex = sound_macro_fade_in((MacroState *)voice, &cstep_80056228);
                break;
            case 0x17:
                ex = sound_macro_random_note((struct MacroStateSetNote *)voice, &cstep_80056228);
                break;
            case 0x18:
                ex = sound_macro_add_note((struct MacroStateAddNote *)voice, &cstep_80056228);
                break;
            case 0x19:
                ex = sound_macro_set_note((struct MacroStateSetNote *)voice, &cstep_80056228);
                break;
            case 0x1A:
                ex = sound_macro_last_note((struct MacroStateLastNote *)voice, &cstep_80056228);
                break;
            case 0x1B:
                ex = sound_macro_portamento((struct MacroStatePortamento *)voice, &cstep_80056228);
                break;
            case 0x1C:
                ex = sound_macro_vibrato((struct MacroStateVibrato *)voice, &cstep_80056228);
                break;
            case 0x1D:
                ex = sound_macro_pitch_sweep((struct MacroStatePitchSweep *)voice, &cstep_80056228, 0);
                break;
            case 0x1E:
                ex = sound_macro_pitch_sweep((struct MacroStatePitchSweep *)voice, &cstep_80056228, 1);
                break;
            case 0x1F:
                ex = sound_macro_set_pitch((struct MacroStateSetPitch *)voice, (struct MacroCommandSetPitch *)&cstep_80056228);
                break;
            case 0x28:
                ex = sound_macro_trap_key_off((struct MacroStateTrapKeyOff *)voice, &cstep_80056228);
                break;
            case 0x29:
                ex = sound_macro_untrap_key_off((struct MacroStateUntrapKeyOff *)voice, (struct MacroCommandUntrapKeyOff *)&cstep_80056228);
                break;
            case 0x30:
                ex = sound_macro_add_age((struct MacroStateAddAge *)voice, (struct MacroCommandAddAge *)&cstep_80056228);
                break;
            case 0x31:
                ex = sound_macro_set_age((struct MacroStateSetAge *)voice, (struct MacroCommandSetAge *)&cstep_80056228);
                break;
            case 0x32:
                ex = sound_macro_send_flag((struct MacroStateSendFlag *)voice, (struct MacroCommandSendFlag *)&cstep_80056228);
                break;
            case 0x33:
                ex = sound_macro_set_pitch_wheel_range((struct MacroStateSetPitchWheelRange *)voice, (struct MacroCommandSetPitchWheelRange *)&cstep_80056228);
                break;
            case 0x34:
                ex = sound_macro_set_reverb_send((struct MacroStateSetReverbSend *)voice, (struct MacroCommandSetReverbSend *)&cstep_80056228);
                break;
            case 0x35:
                ex = sound_macro_track_pitch_wheel((struct MacroStateTrackPitchWheel *)voice, (struct MacroCommandTrackPitchWheel *)&cstep_80056228);
                break;
            case 0x36:
                ex = sound_macro_set_priority((MacroState *)voice, &cstep_80056228);
                break;
            case 0x37:
                ex = sound_macro_add_priority((struct MacroStateAddPriority *)voice, &cstep_80056228);
                break;
            case 0x38:
                ex = sound_macro_set_age_speed((struct MacroStateSetAgeSpeed *)voice, (struct MacroCommandSetAgeSpeed *)&cstep_80056228);
                break;
            case 0x39:
                ex = sound_macro_set_age_by_velocity((struct MacroStateSetAgeByVelocity *)voice, (struct MacroCommandSetAgeByVelocity *)&cstep_80056228);
                break;
            case 0x40:
                ex = sound_macro_select_volume((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x41:
                ex = sound_macro_select_pan((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x42:
                ex = sound_macro_select_pitch_bend((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x43:
                ex = sound_macro_select_modulation((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x44:
                ex = sound_macro_select_pedal((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x45:
                ex = sound_macro_select_portamento((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x46:
                ex = sound_macro_select_reverb((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x47:
                ex = sound_macro_select_span((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x48:
                ex = sound_macro_select_doppler((struct MacroStateSelectVolume *)voice, (struct MacroCommandSelectVolume *)&cstep_80056228);
                break;
            case 0x50:
                ex = sound_macro_setup_lfo((struct MacroStateSetupLfo *)voice, (struct MacroCommandSetupLfo *)&cstep_80056228);
                break;
            case 0x60:
                ex = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &cstep_80056228, 0);
                break;
            case 0x61:
                ex = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &cstep_80056228, 1);
                break;
            case 0x62:
                ex = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &cstep_80056228, 2);
                break;
            case 0x63:
                ex = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &cstep_80056228, 3);
                break;
            case 0x64:
                ex = sound_macro_variable_arithmetic((struct SynthVoiceVariablesView *)voice, &cstep_80056228, 4);
                break;
            case 0x65:
                ex = sound_macro_set_variable_immediate((MacroState *)voice, &cstep_80056228);
                break;
            case 0x70:
                ex = sound_macro_compare((struct MacroStateCompare *)voice, &cstep_80056228, 0);
                break;
            case 0x71:
                ex = sound_macro_compare((struct MacroStateCompare *)voice, &cstep_80056228, 1);
                break;
            default:
                ex = 0;
            }
        } while (!ex);
        voice->timeA4 += sound_tick_step;
    } else {
        voice->timeA4 += sound_tick_step;
        if ((voice->cFlags24 & 4) && !(voice->cFlags24 & 0x40000000) && (voice->cFlags24 & 8)) {
            voice->cFlags24 &= ~4;
            voice->waitTimeA0 = voice->timeA4;
            voice->wait9C = 0;
        }
        if ((voice->cFlags24 & 0x100000) && !sound_hw_is_voice_active(voice->id60 & 0xFF)) {
            voice->cFlags24 &= ~0x100000;
            voice->wait9C = 0;
            voice->waitTimeA0 = voice->timeA4;
        }
        if (voice->wait9C != 0xFFFFFFFF && voice->timeA4 >= voice->wait9C) {
            voice->waitTimeA0 = voice->wait9C;
            voice->wait9C = 0;
        }
    }
}

/* PROMOTED 2026-10-11 — sound_macro_start
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
typedef struct MacroStateStart { enum n64p_MacroCommand start00; enum n64p_MacroCommand current04; enum n64p_MacroCommand saved08; u8 unknown0C[4]; u32 child10; u32 parent14; u8 unknown18[12]; u32 flags24; u32 age28; u16 ageSpeed2C; u8 unknown2E[0x1E]; u8 external4C; u8 unknown4D; u16 originalNote4E; u16 note50; u8 volume52; u8 panning53; u8 channel54; u8 set55; u8 section56; u8 unknown57[9]; u32 identifier60; u16 macro64; u8 unknown66[0x36]; u32 deadline9C; u8 unknownA0[0x1A]; u16 allocationBA; u8 unknownBC[3]; u8 groupBF; u8 detuneC0; u8 unknownC1[0xDF]; } MacroStateStart;
#pragma pack()
extern int sound_alloc_voice(u8, u8, u16, u8);
extern void sound_midi_reset_channel(u8, u8);
extern u32 sound_new_handle(MacroStateStart *);
u32 sound_macro_start(u32 packed, u16 allocation, u8 key, u8 volume,
                 u8 panning, u8 channel, u8 set, u16 offset,
                 u16 section, u8 start, u8 group)
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
        index = sound_alloc_voice(priority, packed & 255, allocation, (key & 128) ? 1 : 0);
        if (index != -1) {
            state = (MacroStateStart *)&sound_synth_voices[index];
            sound_unlink_voice((MacroState *)state);
            state->flags24 = (state->flags24 & 0x10) | 2;
            if (sound_hw_is_voice_active(index)) state->flags24 |= 1;
            state->deadline9C = 0;
            if (key & 128) {
                state->external4C = 1;
                key &= 127;
                sound_midi_reset_channel(index, 255);
                state->channel54 = index;
                state->set55 = 255;
            } else {
                state->external4C = 0;
                state->channel54 = channel;
                state->set55 = set;
            }
            state->macro64 = macro;
            state->allocationBA = allocation;
            state->age28 = 0x75300000U;
            state->ageSpeed2C = 1024;
            state->start00 = N64W(program);
            state->current04 = N64W(program + offset);
            state->originalNote4E = key;
            state->note50 = key;
            state->detuneC0 = 0;
            state->volume52 = volume;
            state->panning53 = panning;
            state->section56 = section;
            state->saved08 = 0;
            state->child10 = 0xFFFFFFFFU;
            state->parent14 = 0xFFFFFFFFU;
            state->groupBF = group;
            state->identifier60 = (macro << 16) | ((u32)key << 8) | (u32)index;
            sound_set_voice_priority((MacroState *)state, priority);
            if (start) return sound_new_handle(state);
            return state->identifier60;
        }
    }
    return 0xFFFFFFFFU;
}

