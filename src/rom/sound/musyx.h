/* src/rom/sound/musyx.h: what the MusyX sound library's files share: a voice's flags and ids, the MIDI controllers,
 * the volume group faders, and the synth voice (SynthVoice), the one record its files read a voice as. MusyX is the
 * N64's sound library (no MAT3 counterpart; docs/code/audio.md). */
#ifndef MUSYX_H
#define MUSYX_H

/* A voice's control flags (SynthVoice.flags). Bits 0 and 2 belong to the macros. */
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
#define CTRL_LFO1            160 /* not stored: the voice's first LFO (SynthVoice.lfo[0]) */
#define CTRL_LFO2            161 /* not stored: the voice's second LFO (lfo[1]) */

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

/* ---- The synth voice (sound_synth_voices[i], pc_mx_voice on the PC: pc_game_types.h)
 * A sound plays on one or more synth voices, each running a SoundMacro (macros.c): the synth's tick (synth.c) runs
 * the macro, then turns what its steps set into the hardware voice's pitch and volume (hardware.c). The library's
 * files all read a voice as this one record. */
typedef struct MacroCommand { u32 word[2]; } MacroCommand;   /* a macro's step: opcode and parameters (macros.c) */
struct HandleNode;                                              /* a handle's node (voices.c) */

/* The voice's head: where its macro is (the steps in the pool, wave 7), where a GOSUB returns to and a key off jumps
 * to, the voices chained behind one handle, and the handle's node. */
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

/* A voice's controller inputs (SynthVoice.inputs), in this order: the synth reads each as a mix of up to four
 * sources (controllers.c), and the macros' SELECT_* steps set them (macros.c). */
enum SynthInput {
    SYNTH_INPUT_VOLUME, SYNTH_INPUT_PAN, SYNTH_INPUT_SPAN, SYNTH_INPUT_PITCH_BEND, SYNTH_INPUT_DOPPLER,
    SYNTH_INPUT_MODULATION, SYNTH_INPUT_PEDAL, SYNTH_INPUT_PORTAMENTO, SYNTH_INPUT_REVERB, SYNTH_INPUT_COUNT
};
/* The voice's records below are packed (alignment 1): the library's code was written against packed views of the
 * voice, and GCC makes other instructions for the same fields of a naturally aligned record. Every field is at its
 * natural alignment all the same, so packing moves none. */
#pragma pack(1)

/* One source of an input: a controller (sound_macro_map_controller's numbering, or an LFO), how it combines with the
 * sources before it (controllers.c's ControlCombine) and its scale (8.8 fixed point, read signed). */
typedef struct SynthInputSource {
    u8 controller;
    u8 combine;
    u16 scale;
} SynthInputSource;
typedef struct SynthInputMix {
    SynthInputSource source[4];
    u8 count;                    /* sources in use; 0: the input's default */
    u8 unused;
} SynthInputMix;

/* One of a voice's two LFOs (SETUP_LFO): its clock and period in ticks, and its value now (a sine,
 * -0x7FFF..0x7FFF). */
typedef struct SynthLfo {
    u32 time;
    u32 period;                  /* 0: off */
    s16 value;
    s16 lastValue;               /* not read */
} SynthLfo;

/* The voice. Volumes and pans are 16.16, 0x7F0000 the loudest or rightmost; notes are MIDI keys. On the N64 the head
 * was nine words and the voice 0x1A0 bytes; on the PC the head's pointers are host pointers, so every field after it
 * lies 0x1C bytes later than on the N64, and the voice is SYNTH_VOICE_SIZE bytes (0x1C0: the tail keeps the next
 * voice's head 8-byte aligned). */
typedef struct SynthVoice {
    SynthVoiceHead head;
    u32 flags;                   /* VOICE_* above */
    u32 age;                     /* the voice-stealing age (15 bits of fraction): falls by ageSpeed each tick */
    u16 ageSpeed;
    u8 priority;                 /* the allocation priority (voices.c's lists) */
    u8 track;                    /* the song track whose volume applies (sound_track_volumes), 255: none */
    u32 volume;                  /* 0..0x7F0000 */
    u32 orgVolume;               /* the volume it started with (SCALE_VOLUME's base) */
    u32 panning;
    s32 panDelta;                /* a PANNING sweep's step */
    s32 sweepOff[2];             /* the two pitch sweeps' offsets, 16.16 */
    u8 sweepNum[2];              /* and their steps per cycle */
    u8 channel;                  /* the MIDI channel, NO_CHANNEL for an effect */
    u8 set;                      /* whose controllers it reads: a song player, or VOICE_OWN_SET (an effect's own) */
    u8 fxFlag;                   /* an effect voice (the key's bit 7 at start) */
    u8 unused1;
    u16 orgNote;                 /* the key it started on */
    u16 curNote;                 /* the note now */
    u8 setupVol;                 /* what sound_macro_start stores and the macro's first run applies: */
    u8 setupPan;
    u8 setupChannel;
    u8 setupSet;
    u8 setupTrack;
    u8 unused2;
    u32 sampleAddress;           /* the sample playing: its ROM offset (written, never read) */
    u32 sampleInfo;              /* its root key (bits 31..24) and rate in Hz (23..0) */
    u32 id;                      /* (macro << 16) | (key << 8) | the voice's index; VOICE_NONE when free */
    u16 macroId;                 /* the macro it runs */
    u8 unused3[2];
    u16 loop;                    /* the LOOP step's counter */
    u8 pbLowerKeyRange;          /* the pitch wheel's range down and up, in semitones */
    u8 pbUpperKeyRange;
    u32 vibCurTime;              /* vibrato: its clock, period and offset now, */
    u32 vibPeriod;
    s32 vibCurOffset;
    u8 vibKeyRange;              /* and its depth in keys and cents */
    u8 vibCentRange;
    u8 unused4[2];
    s32 sweepAdd[2];             /* the pitch sweeps' steps */
    s32 envDelta;                /* the volume envelope's step (ENVELOPE, FADE_IN) and target */
    u32 envTarget;
    u32 portTime;                /* portamento: time gone, of portDuration */
    u32 portDuration;
    u32 portCurrent;             /* the pitch it has reached, 16.16 */
    u8 portType;                 /* 1: glide only on the voice's first update */
    u8 revVolScale;              /* the reverb send's scale (128: 1) and offset */
    u8 revVolOffset;
    u8 unused5;
    u32 wait;                    /* when the current WAIT ends (0: not waiting; MACRO_WAIT_FOREVER, macros.c) */
    u32 waitTime;                /* when the last wait ended: tick waits count from it */
    u32 time;                    /* the voice's clock */
    u32 panTime;                 /* the PANNING sweep's time left and end */
    u32 panTarget;
    u8 sweepCnt[2];              /* the pitch sweeps' step counters */
    u8 unused6[2];
    u32 envTime;                 /* the envelope's time left */
    u16 pbLast;                  /* the last pitch-wheel value (0x2000: centre) */
    u16 allocId;                 /* the allocation id (sound_alloc_voice) */
    u8 unused7;
    u8 blocked;                  /* set while the voice starts a child: not to be stolen or updated */
    u8 vGroup;                   /* the volume group (sound_volume_faders) now and as started */
    u8 setupVGroup;
    s8 curDetune;                /* cents */
    u8 lastNote;                 /* the channel's previous key (LAST_NOTE, the portamento's start) */
    u16 curOutputVolume;
    SynthInputMix inputs[SYNTH_INPUT_COUNT];   /* the controller mixes of SYNTH_INPUT_* */
    u8 unused8[2];
    SynthLfo lfo[2];
    s16 localVars[16];           /* the macro's variables (the globals are sound_global_variables) */
    u8 unused9[4];              /* the PC's tail */
} SynthVoice;
#pragma pack()
#define SYNTH_VOICE_SIZE PC_MX_VOICE_SIZE   /* 0x1C0 */
_Static_assert(sizeof(SynthVoice) == SYNTH_VOICE_SIZE && SYNTH_VOICE_SIZE % 8 == 0,
               "a voice is pc_mx_voice's element, and the next voice's head stays aligned");
/* The voices: the PC's per-voice storage pc_mx_voice (pc_game_types.h, PC_MX_VOICE_SLOTS long, no N64 address:
 * docs/port/DECOUPLE.md 9) as voices. */
#define sound_synth_voices (*(SynthVoice (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)

#endif /* MUSYX_H */
