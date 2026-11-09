/* src/rom/sound/init.c: starting and stopping the MusyX sound library.
 *
 * The game's AudioInitSys starts the library once with sound_lib_init (which starts the hardware layer's own output
 * thread) and again, when it restarts the sound, with sound_lib_init_shared_output over the thread already running.
 * Either starts the hardware layer (hardware.c), then the layers above it: the group and resource registries, the
 * sequencer, the synth, the streams and the 3D emitters. MusyX is the N64's sound library: no MAT3 counterpart. */
#include "../rom_tu.h"

#define PC_MX_CAP(n) PC_MX_VOICE_SLOTS   /* the most voices the PC gives (pc_game_types.h, docs/port/AUDIO.md 10) */

extern unsigned char sound_initialized;
extern volatile unsigned char sound_voices_locked;
extern void sound_reset_groups(void);
extern void sound_reset_registries(void);
extern void sound_seq_init(void);
extern void sound_synth_init(unsigned int);
extern void sound_stream_reset_all(void);
extern void sound_emitter_init(void);
extern unsigned char sound_voice_count;
extern unsigned char sound_music_voice_limit;
extern unsigned char sound_sfx_voice_limit;
extern int sound_hw_init(unsigned int *, unsigned short, unsigned short, unsigned int);
extern int sound_lib_init_layers(unsigned int);
extern int sound_hw_init_shared_output(unsigned int *, unsigned short, unsigned short, unsigned int);
extern void sound_hw_exit(void);
extern void sound_emitter_exit(void);
extern void sound_hw_exit_shared_output(void);
extern unsigned char sound_output_info[];

/* Starts the layers above the hardware: empties the group and resource registries, starts the sequencer, the synth
 * at the output rate frequency, the streams and the emitters, and marks the library started. Returns 0. */
int sound_lib_init_layers(unsigned int frequency)
{
    sound_reset_groups();
    sound_reset_registries();
    sound_seq_init();
    sound_voices_locked = 0;
    sound_synth_init(frequency);
    sound_stream_reset_all();
    sound_emitter_init();
    sound_initialized = 1;
    return 0;
}

/* Starts the library with its own output thread: frequency the output rate asked for, voices the voices to use (at
 * most PC_MX_VOICE_SLOTS on the PC, 32 on the N64), of which music_voices at most for songs and sfx_voices for
 * effects, a delay line of delay_ms (0 in Rush 2049) and the hardware's flags (sound_hw_init). Returns 0, or the
 * hardware layer's error. */
int sound_lib_init(unsigned int frequency, unsigned char voices,
                  unsigned char music_voices, unsigned char sfx_voices,
                  unsigned short delay_ms, unsigned int flags)
{
    int result;
    sound_initialized = 0;
    if (voices <= PC_MX_CAP(32)) {
        sound_voice_count = voices;
    } else {
        sound_voice_count = PC_MX_CAP(32);
    }
    sound_music_voice_limit = music_voices;
    sound_sfx_voice_limit = sfx_voices;
    result = sound_hw_init(&frequency, sound_voice_count, delay_ms, flags);
    if (result == 0) {
        result = sound_lib_init_layers(frequency);
    }
    return result;
}

/* As sound_lib_init, but over the output thread already running (after sound_lib_exit_shared_output). */
int sound_lib_init_shared_output(unsigned int frequency, unsigned char voices,
                  unsigned char music_voices, unsigned char sfx_voices,
                  unsigned short delay_ms, unsigned int flags)
{
    int result;
    sound_initialized = 0;
    if (voices <= PC_MX_CAP(32)) {
        sound_voice_count = voices;
    } else {
        sound_voice_count = PC_MX_CAP(32);
    }
    sound_music_voice_limit = music_voices;
    sound_sfx_voice_limit = sfx_voices;
    result = sound_hw_init_shared_output(&frequency, sound_voice_count, delay_ms, flags);
    if (result == 0) {
        result = sound_lib_init_layers(frequency);
    }
    return result;
}

/* Stops the library, the output thread too. */
void sound_lib_exit(void)
{
    if (sound_initialized != 0) {
        sound_hw_exit();
        sound_emitter_exit();
        sound_initialized = 0;
    }
}

/* Stops the library but leaves the output thread running, for sound_lib_init_shared_output. */
void sound_lib_exit_shared_output(void)
{
    if (sound_initialized != 0) {
        sound_hw_exit_shared_output();
        sound_emitter_exit();
        sound_initialized = 0;
    }
}

/* Has the library been started? */
u8 sound_is_initialized(void)
{
    return sound_initialized;
}

/* An option setter that does nothing in this version of the library. */
void sound_set_option(unsigned int value)
{
}

/* The output's description (an AudioConfiguration: rate, mode, sample bits, the driver's name), or 0 before the
 * library is started. */
void *sound_get_output_info(void)
{
    if (sound_initialized != 0) {
        return sound_output_info;
    }
    return 0;
}
