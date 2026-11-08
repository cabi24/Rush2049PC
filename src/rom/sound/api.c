/* GENERATED ROM-aligned TU — segment 0x207b0 (rom/lib_207b0)
 * layout map a4f8c8e3046756fa727553750689364dc71ce2d0413805cd6440e6752a219abe.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

#pragma pack(1)
typedef struct VoiceState { u32 command00; u8 unknown04[12]; u32 next10; u8 unknown14[16]; u32 flags24; u8 unknown28[3]; u8 channel2B; u8 unknown2C[2]; u8 channel2E; u8 unknown2F[28]; u8 channel4B; u8 external4C; u8 unknown4D[8]; u8 channel55; u8 unknown56[10]; u32 identifier60; u8 unknown64[316]; } VoiceState;
#pragma pack(0)
extern u8 sound_initialized;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
/* On the PC the voices are PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9), seen here in this file's view. */
#define sound_synth_voices (*(VoiceState (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)
extern int sound_resolve_handle(u32);
extern void sound_midi_set_controller(u8, u8, u8, u8);
/* PROMOTED 2026-10-11 — sound_set_modulation
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
int sound_set_modulation(u32 identifier, u16 value)
{
    int result;
    u32 index;
    u8 low;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        identifier = sound_resolve_handle(identifier);
        while (identifier != 0xFFFFFFFFU) {
            index = identifier & 255;
            if (sound_synth_voices[index].identifier60 == identifier) {
                low = value & 127;
                if (sound_synth_voices[index].flags24 & 2) {
                    sound_midi_set_controller(1, index, sound_synth_voices[index].channel55, value >> 7);
                    sound_midi_set_controller(33, index, sound_synth_voices[index].channel55, low);
                } else {
                    sound_midi_set_controller(1, index, sound_synth_voices[index].channel4B, value >> 7);
                    sound_midi_set_controller(33, index, sound_synth_voices[index].channel4B, low);
                }
                result = 0;
                identifier = sound_synth_voices[index].next10;
            } else {
                sound_hw_unlock();
                return result;
            }
        }
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_reverb
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_set_reverb(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        identifier = sound_resolve_handle(identifier);
        while (identifier != 0xFFFFFFFFU) {
            index = identifier & 255;
            if (sound_synth_voices[index].identifier60 == identifier) {
                result = 0;
                if (sound_synth_voices[index].flags24 & 2) {
                    sound_midi_set_controller(91, index, sound_synth_voices[index].channel55, value);
                } else {
                    sound_midi_set_controller(91, index, sound_synth_voices[index].channel4B, value);
                }
                identifier = sound_synth_voices[index].next10;
            } else {
                sound_hw_unlock();
                return result;
            }
        }
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_key_off
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern int sound_key_off_unlocked(unsigned int);
int sound_key_off(unsigned int identifier)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_key_off_unlocked(identifier);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_set_sfx_volume_unlocked(unsigned int, unsigned char);
int sound_set_volume(unsigned int identifier, unsigned char value)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_set_sfx_volume_unlocked(identifier, value);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_pitch_bend
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_set_sfx_pitch_bend_unlocked(unsigned int, unsigned short);
int sound_set_pitch_bend(unsigned int identifier, unsigned short value)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_set_sfx_pitch_bend_unlocked(identifier, value);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_span
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_set_sfx_span_unlocked(unsigned int, unsigned char);
int sound_set_span(unsigned int identifier, unsigned char value)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_set_sfx_span_unlocked(identifier, value);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_doppler
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_set_sfx_doppler_unlocked(unsigned int, unsigned short);
int sound_set_doppler(unsigned int identifier, unsigned short value)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_set_sfx_doppler_unlocked(identifier, value);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_pan
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_set_sfx_pan_unlocked(unsigned int, unsigned char);
int sound_set_pan(unsigned int identifier, unsigned char value)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_set_sfx_pan_unlocked(identifier, value);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_set_pedal
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_set_pedal(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        identifier = sound_resolve_handle(identifier);
        while (identifier != 0xFFFFFFFFU) {
            index = identifier & 255;
            if (sound_synth_voices[index].identifier60 == identifier) {
                result = 0;
                if (sound_synth_voices[index].flags24 & 2) {
                    sound_midi_set_controller(64, index, sound_synth_voices[index].channel55, value);
                } else {
                    sound_midi_set_controller(64, index, sound_synth_voices[index].channel4B, value);
                }
                identifier = sound_synth_voices[index].next10;
            } else {
                sound_hw_unlock();
                return result;
            }
        }
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_start_sfx
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_start_sfx_unlocked(unsigned short, unsigned char, unsigned char);
int sound_start_sfx(unsigned short item, unsigned char channel, unsigned char value)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_start_sfx_unlocked(item, channel, value);
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-05 — sound_check
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_resolve_handle(unsigned int);
unsigned int sound_check(unsigned int value)
{
    if (sound_resolve_handle(value) != -1) {
        return value;
    }
    return 0xFFFFFFFF;
}

/* PROMOTED 2026-10-11 — sound_get_global_variable
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern short sound_global_variables[];
short sound_get_global_variable(unsigned char value)
{
    return sound_global_variables[value & 15];
}

/* PROMOTED 2026-10-11 — sound_set_global_variable
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern s16 sound_global_variables[16];
s16 sound_set_global_variable(u8 channel, s16 value)
{
    s16 previous;
    channel &= 15;
    sound_hw_lock();
    previous = sound_global_variables[channel];
    sound_global_variables[channel] = value;
    sound_hw_unlock();
    return previous;
}

/* PROMOTED 2026-10-04 — sound_stop_everything
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_stop_all(void);
extern void sound_emitter_remove_all(void);
extern void sound_stop_all_voices(unsigned char);
void sound_stop_everything(void)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_stop_all();
        sound_emitter_remove_all();
        sound_stop_all_voices(0);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-11 — sound_is_idle
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile u8 sound_voices_locked;
extern u8 sound_voice_count;
extern int sound_hw_is_busy(void);
extern u8 sound_hw_is_voice_active(int);
u8 sound_is_idle(void)
{
    u8 active;
    int i;
    active = 0;
    if (sound_initialized) {
        sound_voices_locked = 1;
        if (sound_hw_is_busy() == 0) {
            for (i = 0; i < sound_voice_count; i++) {
                active |= sound_hw_is_voice_active(i);
            }
        } else {
            active = 1;
        }
        sound_voices_locked = 0;
    }
    return active == 0;
}

/* PROMOTED 2026-10-04 — sound_set_sfx_volume_group
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char *sound_find_sfx(unsigned short);
extern void sound_set_group_type(unsigned char, unsigned char);
void sound_set_sfx_volume_group(unsigned short identifier, unsigned char channel)
{
    unsigned char *state;
    if (sound_initialized) {
        sound_hw_lock();
        state = sound_find_sfx(identifier);
        if (state != 0) {
            if (channel != 254) {
                state[9] = channel;
                sound_set_group_type(channel, 3);
            } else state[9] = 31;
        }
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_fade_pause_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_fade_pause_volume_unlocked(unsigned char, unsigned short, unsigned char);
void sound_fade_pause_volume(unsigned char channel, unsigned short duration, unsigned char mode)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_fade_pause_volume_unlocked(channel, duration, mode);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_fade_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_fade_volume_group_unlocked(unsigned char, unsigned short, unsigned char, unsigned char, unsigned int);
void sound_fade_volume(unsigned char channel, unsigned short duration, unsigned char mode)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_fade_volume_group_unlocked(channel, duration, mode, 0, 0);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_fade_master_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_fade_volume_group_unlocked(u8, u16, u8, u8, unsigned int);
void sound_fade_master_volume(u8 channel, u16 duration, u8 first, u8 second)
{
    if (sound_initialized) {
        sound_hw_lock();
        if (first) sound_fade_volume_group_unlocked(channel, duration, 21, 0, 0);
        if (second) sound_fade_volume_group_unlocked(channel, duration, 22, 0, 0);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_set_mono
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_mono;
void sound_set_mono(unsigned char value)
{
    sound_mono = value;
}

/* PROMOTED 2026-10-04 — sound_set_delay_effect
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_set_delay_effect(void *);
void sound_set_delay_effect(void *state)
{
    if (sound_initialized) sound_hw_set_delay_effect(state);
}

/* PROMOTED 2026-10-04 — sound_remove_delay_effect
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_remove_delay_effect(void);
void sound_remove_delay_effect(void)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_hw_remove_delay_effect();
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-11 — sound_set_services
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* the 8 service callbacks are function pointers (docs/port/MUSYX_DATA.md 8.1) */
typedef struct CallbackWords { void (*words[8])(void); } CallbackWords;
extern CallbackWords sound_services;
void sound_set_services(const CallbackWords *callbacks)
{
    sound_services = *callbacks;
}

/* PROMOTED 2026-10-11 — sound_freeze_output
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_freeze_output(void);
void sound_freeze_output(void)
{
    sound_hw_freeze_output();
}

