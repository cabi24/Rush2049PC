/* src/rom/sound/api.c: the MusyX sound library's public entry points for effects, controllers and volumes.
 *
 * What the game's sound server (audiosfx_play/group.c, AudioInitSys, AudioSetMasterVolume ...) and the emitters
 * (streams_3d.c) call: start an effect, set a playing sound's volume, pan, surround span, doppler, pitch bend,
 * modulation, pedal or reverb (MIDI controllers on every voice of its chain), key it off, fade the volume groups, stop
 * everything, ask whether the library is idle, install the platform's service hooks. Each takes the library lock
 * (sound_hw_lock) around the synth's unlocked form (synth.c) and does nothing (or returns -1) before the library is
 * started. A sound is named by its handle (voices.c); -1 or VOICE_NONE is "no sound".
 *
 * MusyX is the N64's sound library: no MAT3 counterpart. The voices are seen through this file's view at the N64
 * offsets (each field's name ends in its offset); on the PC their storage is PC data, PC_MX_VOICE_SLOTS long. */
#include "../rom_tu.h"

#define VOICE_NONE 0xFFFFFFFFU     /* no voice, the end of a voice chain, an invalid handle */
#define VOICE_INDEX_MASK 255       /* a voice id's low byte is the voice's index */
#define VOICE_STARTING 2           /* flags24: a macro was just started on the voice and has not run yet */

/* MIDI controllers (sound_midi_set_controller). */
#define CTRL_MODULATION 1          /* 14 bits: 1 the high 7, 33 the low 7 */
#define CTRL_MODULATION_LOW 33
#define CTRL_PEDAL 64
#define CTRL_REVERB 91

#define FADER_MUSIC 21             /* the music's master volume fader */
#define FADER_SFX 22               /* the effects' master volume fader */
#define SFX_GROUP_RESET 254        /* sound_set_sfx_volume_group: back to the default group */
#define SFX_GROUP_DEFAULT 31
#define FADER_TYPE_SFX 3           /* a volume group an effect was given (synth.c) */
#define SFX_VOLUME_GROUP_BYTE 9    /* an effect's volume group: byte 9 of its 12 (registry.c's Descriptor) */

/* A synth voice, as this file reads it (macros.c has the whole voice). */
#pragma pack(1)
typedef struct VoiceState {
    u32 addr00;            /* the macro's first step; 0: no macro runs */
    u8 unknown04[12];
    u32 child10;           /* the next voice of the chain, or VOICE_NONE */
    u8 unknown14[16];
    u32 flags24;
    u8 unknown28[4];
    u8 unknown2C[2];
    u8 priority2E;
    u8 unknown2F[28];
    u8 midiSet4B;          /* the controllers' set: a song player, or 255 for an effect's own */
    u8 fxFlag4C;
    u8 unknown4D[8];
    u8 setupMidiSet55;     /* the controllers' set until the macro's first step copies it to midiSet4B */
    u8 unknown56[10];
    u32 identifier60;      /* the voice's id; VOICE_NONE when free */
    u8 unknown64[316];
} VoiceState;
#pragma pack(0)


/* the 8 service callbacks are function pointers (docs/port/MUSYX_DATA.md 8.1) */
typedef struct CallbackWords { void (*words[8])(void); } CallbackWords;

extern u8 sound_initialized;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
/* On the PC the voices are PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9), seen here in this file's view. */
#define sound_synth_voices (*(VoiceState (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)
extern int sound_resolve_handle(u32);
extern void sound_midi_set_controller(u8, u8, u8, u8);
extern int sound_key_off_unlocked(unsigned int);
extern int sound_set_sfx_volume_unlocked(unsigned int, unsigned char);
extern int sound_set_sfx_pitch_bend_unlocked(unsigned int, unsigned short);
extern int sound_set_sfx_span_unlocked(unsigned int, unsigned char);
extern int sound_set_sfx_doppler_unlocked(unsigned int, unsigned short);
extern int sound_set_sfx_pan_unlocked(unsigned int, unsigned char);
extern int sound_start_sfx_unlocked(unsigned short, unsigned char, unsigned char);
extern short sound_global_variables[16];
extern void sound_seq_stop_all(void);
extern void sound_emitter_remove_all(void);
extern void sound_stop_all_voices(unsigned char);
extern volatile u8 sound_voices_locked;
extern u8 sound_voice_count;
extern int sound_hw_is_busy(void);
extern u8 sound_hw_is_voice_active(int);
extern unsigned char *sound_find_sfx(unsigned short);
extern void sound_set_group_type(unsigned char, unsigned char);
extern void sound_fade_pause_volume_unlocked(unsigned char, unsigned short, unsigned char);
extern void sound_fade_volume_group_unlocked(unsigned char, unsigned short, unsigned char, unsigned char, unsigned int);
extern unsigned char sound_mono;
extern void sound_hw_set_delay_effect(void *);
extern void sound_hw_remove_delay_effect(void);
extern CallbackWords sound_services;
extern void sound_hw_freeze_output(void);

/* Sets the modulation wheel (14 bits) on every voice of a sound's chain. Returns 0, or -1 if the library is not
 * started or the handle is not valid. sound_set_reverb and sound_set_pedal below are the same for their
 * controllers. */
int sound_set_modulation(u32 identifier, u16 value)
{
    int result;
    u32 index;
    u8 low;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        identifier = sound_resolve_handle(identifier);
        while (identifier != VOICE_NONE) {
            index = identifier & VOICE_INDEX_MASK;
            if (sound_synth_voices[index].identifier60 == identifier) {
                low = value & 127;
                if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                    sound_midi_set_controller(CTRL_MODULATION, index, sound_synth_voices[index].setupMidiSet55, value >> 7);
                    sound_midi_set_controller(CTRL_MODULATION_LOW, index, sound_synth_voices[index].setupMidiSet55, low);
                } else {
                    sound_midi_set_controller(CTRL_MODULATION, index, sound_synth_voices[index].midiSet4B, value >> 7);
                    sound_midi_set_controller(CTRL_MODULATION_LOW, index, sound_synth_voices[index].midiSet4B, low);
                }
                result = 0;
                identifier = sound_synth_voices[index].child10;
            } else {
                sound_hw_unlock();
                return result;
            }
        }
        sound_hw_unlock();
    }
    return result;
}

int sound_set_reverb(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        identifier = sound_resolve_handle(identifier);
        while (identifier != VOICE_NONE) {
            index = identifier & VOICE_INDEX_MASK;
            if (sound_synth_voices[index].identifier60 == identifier) {
                result = 0;
                if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                    sound_midi_set_controller(CTRL_REVERB, index, sound_synth_voices[index].setupMidiSet55, value);
                } else {
                    sound_midi_set_controller(CTRL_REVERB, index, sound_synth_voices[index].midiSet4B, value);
                }
                identifier = sound_synth_voices[index].child10;
            } else {
                sound_hw_unlock();
                return result;
            }
        }
        sound_hw_unlock();
    }
    return result;
}

/* Keys off a sound (sound_key_off_unlocked). Returns 0, or -1. */
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

/* Sets a sound's volume, 0-127 (sound_set_sfx_volume_unlocked). Returns 0, or -1. The pitch bend, span, doppler and
 * pan setters below are the same. */
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

int sound_set_pedal(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        identifier = sound_resolve_handle(identifier);
        while (identifier != VOICE_NONE) {
            index = identifier & VOICE_INDEX_MASK;
            if (sound_synth_voices[index].identifier60 == identifier) {
                result = 0;
                if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                    sound_midi_set_controller(CTRL_PEDAL, index, sound_synth_voices[index].setupMidiSet55, value);
                } else {
                    sound_midi_set_controller(CTRL_PEDAL, index, sound_synth_voices[index].midiSet4B, value);
                }
                identifier = sound_synth_voices[index].child10;
            } else {
                sound_hw_unlock();
                return result;
            }
        }
        sound_hw_unlock();
    }
    return result;
}

/* MusyX's sndFXStart: starts the effect with this id at volume and pan (255: the effect's own). Returns the handle, or
 * -1. */
int sound_start_sfx(unsigned short identifier, unsigned char volume, unsigned char pan)
{
    int result;
    result = -1;
    if (sound_initialized) {
        sound_hw_lock();
        result = sound_start_sfx_unlocked(identifier, volume, pan);
        sound_hw_unlock();
    }
    return result;
}

/* The handle if it still names a playing sound, else VOICE_NONE (the game's AudioEffectPlaying asks this). */
unsigned int sound_check(unsigned int handle)
{
    if (sound_resolve_handle(handle) != -1) {
        return handle;
    }
    return VOICE_NONE;
}

/* One of the 16 global variables the macros read and write. */
short sound_get_global_variable(unsigned char index)
{
    return sound_global_variables[index & 15];
}

/* Sets one of the 16 global variables; returns its old value. */
s16 sound_set_global_variable(u8 index, s16 value)
{
    s16 previous;
    index &= 15;
    sound_hw_lock();
    previous = sound_global_variables[index];
    sound_global_variables[index] = value;
    sound_hw_unlock();
    return previous;
}

/* Stops every song, emitter and voice. */
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

/* Whether nothing plays: no audio task under way and no hardware voice active. The voices are held still
 * (sound_voices_locked: sound_alloc_voice refuses meanwhile) while it looks. AudioShutdownSys waits on it. */
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

/* Puts the effect with this id in volume group (setting the group's type), or back in the default group with
 * SFX_GROUP_RESET. */
void sound_set_sfx_volume_group(unsigned short identifier, unsigned char group)
{
    unsigned char *effect;
    if (sound_initialized) {
        sound_hw_lock();
        effect = sound_find_sfx(identifier);
        if (effect != 0) {
            if (group != SFX_GROUP_RESET) {
                effect[SFX_VOLUME_GROUP_BYTE] = group;
                sound_set_group_type(group, FADER_TYPE_SFX);
            } else effect[SFX_VOLUME_GROUP_BYTE] = SFX_GROUP_DEFAULT;
        }
        sound_hw_unlock();
    }
}

/* Fades the pause volume of volume groups (selected as for sound_fade_volume_group_unlocked) to volume over duration
 * ms. */
void sound_fade_pause_volume(unsigned char volume, unsigned short duration, unsigned char group)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_fade_pause_volume_unlocked(volume, duration, group);
        sound_hw_unlock();
    }
}

/* Fades the volume of volume groups (selected as for sound_fade_volume_group_unlocked) to volume over duration ms. */
void sound_fade_volume(unsigned char volume, unsigned short duration, unsigned char group)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_fade_volume_group_unlocked(volume, duration, group, 0, 0);
        sound_hw_unlock();
    }
}

/* Fades the master volume of the music, the effects, or both, to volume over duration ms. */
void sound_fade_master_volume(u8 volume, u16 duration, u8 music, u8 sfx)
{
    if (sound_initialized) {
        sound_hw_lock();
        if (music) sound_fade_volume_group_unlocked(volume, duration, FADER_MUSIC, 0, 0);
        if (sfx) sound_fade_volume_group_unlocked(volume, duration, FADER_SFX, 0, 0);
        sound_hw_unlock();
    }
}

/* Mono (nonzero: every voice centred, no surround) or stereo. */
void sound_set_mono(unsigned char mono)
{
    sound_mono = mono;
}

/* Installs the delay (echo) effect's settings in the hardware layer. */
void sound_set_delay_effect(void *state)
{
    if (sound_initialized) sound_hw_set_delay_effect(state);
}

/* Removes the delay effect. */
void sound_remove_delay_effect(void)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_hw_remove_delay_effect();
        sound_hw_unlock();
    }
}

/* Installs the platform's eight service hooks (sound_services: submit and wait for the audio task, set and clear the
 * retrace callback, DMA, the cartridge address, allocate, free): the library calls nothing of the OS itself. */
void sound_set_services(const CallbackWords *callbacks)
{
    sound_services = *callbacks;
}

/* Stops the output from advancing (sound_hw_freeze_output): the game's pre-NMI (reset) handler and main loop. */
void sound_freeze_output(void)
{
    sound_hw_freeze_output();
}
