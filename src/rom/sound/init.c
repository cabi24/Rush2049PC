/* GENERATED ROM-aligned TU — segment 0x113e0 (rom/lib_113e0)
 * layout map 92c6f1e8103743941a7b4aa34ac1e2aea46d482f3364609a23db9f010ec25ce4.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-04 — sound_lib_init_layers
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern volatile unsigned char sound_voices_locked;
extern void sound_reset_groups(void);
extern void sound_reset_registries(void);
extern void sound_seq_init(void);
extern void sound_synth_init(unsigned int);
extern void sound_stream_reset_all(void);
extern void sound_emitter_init(void);
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

/* PROMOTED 2026-10-11 — sound_lib_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_voice_count;
extern unsigned char sound_music_voice_limit;
extern unsigned char sound_sfx_voice_limit;
#define PC_MX_CAP(n) PC_MX_VOICE_SLOTS   /* the most voices the PC gives (pc_game_types.h, docs/port/AUDIO.md 10) */
extern int sound_hw_init(unsigned int *, unsigned short, unsigned short, unsigned int);
extern int sound_lib_init_layers(unsigned int);
int sound_lib_init(unsigned int frequency, unsigned char voices,
                  unsigned char setting1, unsigned char setting2,
                  unsigned short count, unsigned int flags)
{
    int result;
    sound_initialized = 0;
    if (voices <= PC_MX_CAP(32)) {
        sound_voice_count = voices;
    } else {
        sound_voice_count = PC_MX_CAP(32);
    }
    sound_music_voice_limit = setting1;
    sound_sfx_voice_limit = setting2;
    result = sound_hw_init(&frequency, sound_voice_count, count, flags);
    if (result == 0) {
        result = sound_lib_init_layers(frequency);
    }
    return result;
}

/* PROMOTED 2026-10-11 — sound_lib_init_shared_output
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_hw_init_shared_output(unsigned int *, unsigned short, unsigned short, unsigned int);
int sound_lib_init_shared_output(unsigned int frequency, unsigned char voices,
                  unsigned char setting1, unsigned char setting2,
                  unsigned short count, unsigned int flags)
{
    int result;
    sound_initialized = 0;
    if (voices <= PC_MX_CAP(32)) {
        sound_voice_count = voices;
    } else {
        sound_voice_count = PC_MX_CAP(32);
    }
    sound_music_voice_limit = setting1;
    sound_sfx_voice_limit = setting2;
    result = sound_hw_init_shared_output(&frequency, sound_voice_count, count, flags);
    if (result == 0) {
        result = sound_lib_init_layers(frequency);
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_lib_exit
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_exit(void);
extern void sound_emitter_exit(void);
void sound_lib_exit(void)
{
    if (sound_initialized != 0) {
        sound_hw_exit();
        sound_emitter_exit();
        sound_initialized = 0;
    }
}

/* PROMOTED 2026-10-04 — sound_lib_exit_shared_output
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_hw_exit_shared_output(void);
void sound_lib_exit_shared_output(void)
{
    if (sound_initialized != 0) {
        sound_hw_exit_shared_output();
        sound_emitter_exit();
        sound_initialized = 0;
    }
}

/* PROMOTED 2026-10-04 — sound_is_initialized
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_initialized;
u8 sound_is_initialized(void)
{
    return sound_initialized;
}

/* PROMOTED 2026-10-04 — sound_set_option
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_set_option(unsigned int value)
{
}

/* PROMOTED 2026-10-04 — sound_get_output_info
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_output_info[];
void *sound_get_output_info(void)
{
    if (sound_initialized != 0) {
        return sound_output_info;
    }
    return 0;
}

