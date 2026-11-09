/* src/rom/sound/musyx.h: the MusyX sound library's constants its files share: a voice's flags and ids, the MIDI
 * controllers, the volume group faders. MusyX is the N64's sound library (no MAT3 counterpart; docs/code/audio.md). */
#ifndef MUSYX_H
#define MUSYX_H

/* A voice's control flags (its state's +0x24: flags24, cFlags24). Bits 0 and 2 belong to the macros. */
#define VOICE_HW_BUSY_AT_START    0x00000001U /* the hardware voice was still playing when the macro started: break it off */
#define VOICE_STARTING            0x00000002U /* the macro was just started: its first run sets the voice up */
#define VOICE_WAIT_KEY_OFF        0x00000004U /* the current WAIT ends at key off */
#define VOICE_KEY_OFF             0x00000008U /* key off received (sound_synth_key_off_voice): the macro decides what it does */
#define VOICE_PLAYING             0x00000010U /* the synth started the hardware voice: its sample plays */
#define VOICE_START_SAMPLE        0x00000020U /* START_SAMPLE set a sample: the synth's tick starts it */
#define VOICE_RELEASE             0x00000080U /* KEY_OFF: release the hardware voice at the next tick (unless the pedal holds it) */
#define VOICE_PITCH_FROM_RATE     0x00000100U /* SET_PITCH set the note from a sample rate */
#define VOICE_ADSR_SET            0x00000200U /* SET_ADSR gave the hardware voice its envelope: a new sample keeps it */
#define VOICE_PORTAMENTO          0x00000800U /* portamento is on */
#define VOICE_PORTAMENTO_BY_MACRO 0x00001000U /* PORTAMENTO decides, not the channel's portamento controller (65) */
#define VOICE_UPDATED             0x00002000U /* the synth has updated the voice since its macro started (synth.c) */
#define VOICE_VIBRATO             0x00004000U
#define VOICE_VIBRATO_BY_MODULATION 0x00008000U /* the modulation wheel scales the vibrato */
#define VOICE_PAN_SWEEP           0x00010000U /* a PANNING sweep runs */
#define VOICE_VOLUME_SWEEP        0x00020000U /* an ENVELOPE or FADE_IN runs */
#define VOICE_TRACK_PITCH_WHEEL   0x00040000U /* the pitch wheel applies */
#define VOICE_LEGATO              0x00080000U /* retriggered legato (sound_synth_retrigger_legato) */
#define VOICE_WAIT_SAMPLE_END     0x00100000U /* the current WAIT ends when the sample does */
#define VOICE_INPUT_VOLUME        0x00200000U /* VOICE_INPUT_*: a SELECT_* step set this input's mix (else the default) */
#define VOICE_INPUT_PAN           0x00400000U
#define VOICE_INPUT_PITCH_BEND    0x00800000U
#define VOICE_INPUT_MODULATION    0x01000000U
#define VOICE_INPUT_REVERB        0x02000000U
#define VOICE_INPUT_PORTAMENTO    0x04000000U
#define VOICE_INPUT_PEDAL         0x08000000U
#define VOICE_INPUT_SPAN          0x10000000U
#define VOICE_INPUT_DOPPLER       0x20000000U
#define VOICE_PEDAL_DOWN          0x40000000U /* the sustain pedal (controller 64) is down: key offs wait for it */

/* Voice ids and channels. */
#define VOICE_NONE                0xFFFFFFFFU /* a voice id, handle or chain link that names no voice */
#define VOICE_INDEX_MASK          255         /* a voice id's low byte is the voice's index */
#define VOICE_INDEX(id)           ((id) & VOICE_INDEX_MASK)
#define NO_CHANNEL                255         /* a voice's MIDI channel when no MIDI channel drives it (an effect) */
#define VOICE_OWN_SET             255         /* the controller set of an effect voice: its own (pc_mx_fx_ctrl) */
#define NO_LAST_KEY               255         /* a channel's last key before any (controllers.c) */

/* Controller numbers (sound_midi_set_controller): 0..127 are MIDI's, 128 and up the library's own. A controller below
 * 64 is the high half of a 14-bit value whose low half is the controller 32 above; 128/129 and 132/133 are 14-bit
 * pairs too. */
#define CTRL_MODULATION      1
#define CTRL_VOLUME          7
#define CTRL_PAN             10
#define CTRL_MODULATION_LOW  33
#define CTRL_PEDAL           64  /* sustain; 64..69 are switches, read as off (0) or on (16383) */
#define CTRL_PORTAMENTO      65
#define CTRL_REVERB          91
#define CTRL_PITCH_BEND      128
#define CTRL_PITCH_BEND_LOW  129
#define CTRL_PRESSURE        130 /* channel pressure (the sequencer's) */
#define CTRL_SPAN            131 /* front-back pan (surround) */
#define CTRL_DOPPLER         132
#define CTRL_DOPPLER_LOW     133
#define CTRL_LFO1            160 /* not stored: the voice's first LFO (+0x170) */
#define CTRL_LFO2            161 /* not stored: the voice's second LFO (+0x17C) */

/* The volume group faders (sound_volume_faders, 32 of them). A fader's type: 0 a song track's group (sound_seq_start),
 * 1 the effects' default group (31), 2 a song's studio group (sound_seq_set_studio_group), 3 an effects' group
 * (sound_set_sfx_volume_group), 4 not in use. */
#define FADER_COUNT          32
#define FADER_MUSIC          21  /* the music's master fader */
#define FADER_SFX            22  /* the effects' master fader */
#define FADER_SONG_FIRST     23  /* 23-30: the song players' default groups; 31: the effects' default group */
#define FADER_TYPE_SFX       3
#define FADER_UNUSED         4
#define SFX_DEFAULT_VOLUME_GROUP 31 /* the volume group an effect has until sound_set_sfx_volume_group */

/* A DMA list entry on the N64, which the cache-line bookkeeping counts (dma_queue.c, hardware.c). */
#define DMA_ENTRY_N64_BYTES  12

/* ---- The synth voice (sound_synth_voices[i], pc_mx_voice on the PC: pc_game_types.h)
 * Each file reads a voice through views of its own (macros.c has the whole map). Every view starts with the voice's
 * head, the same in all of them: where its macro is (the steps in the pool, wave 7), where a GOSUB returns to and a
 * key off jumps to, the voices chained behind one handle, and the handle's node. On the N64 the head was nine words,
 * +0x00 to +0x23; on the PC its seven pointers are host pointers, so it is 0x40 bytes and every later field lies
 * SYNTH_VOICE_GROWTH bytes past the N64 offset its name ends in (flags24 is the N64's +0x24, the PC's +0x40). A
 * voice is SYNTH_VOICE_SIZE bytes: the N64's 0x1A0, the head's growth, and a tail that keeps the next voice's
 * pointers aligned. */
typedef struct MacroCommand { u32 word[2]; } MacroCommand;   /* a macro's step: opcode and parameters (macros.c) */
struct HandleNode;                                              /* a handle's node (voices.c) */
typedef struct SynthVoiceHead {
    MacroCommand *addr;          /* the macro's first step; NULL: no macro runs */
    MacroCommand *curAddr;       /* the next step to run */
    MacroCommand *retAddr;       /* GOSUB's return: the macro (NULL: none) */
    MacroCommand *retCurAddr;    /* and the step after the GOSUB */
    u32 next;                    /* the next voice of the handle's chain (a voice id), or VOICE_NONE */
    u32 previous;                /* the previous one; VOICE_NONE for the first */
    struct HandleNode *node;     /* the chain's handle node (voices.c), kept on its first voice */
    MacroCommand *trapAddr;      /* TRAP_KEY_OFF: the macro a key off jumps to (NULL: none) */
    MacroCommand *trapCurAddr;   /* and its step */
} SynthVoiceHead;
#define SYNTH_VOICE_N64_HEAD  0x24                                      /* the head's bytes on the N64 */
#define SYNTH_VOICE_N64_SIZE  0x1A0
#define SYNTH_VOICE_GROWTH    (sizeof(SynthVoiceHead) - SYNTH_VOICE_N64_HEAD)
#define SYNTH_VOICE_SIZE      PC_MX_VOICE_SIZE                          /* 0x1C0 */
#define SYNTH_VOICE_TAIL      (SYNTH_VOICE_SIZE - SYNTH_VOICE_N64_SIZE - SYNTH_VOICE_GROWTH)
_Static_assert(SYNTH_VOICE_TAIL > 0 && SYNTH_VOICE_SIZE % 8 == 0, "the voice holds the head and stays aligned");

#endif /* MUSYX_H */
