/* include/sound_hw_voice.h: the MusyX hardware layer's voice record (src/rom/sound/hardware.c).
 *
 * One record per hardware voice, in sound_hw_voices (sound_hw_voice_count of them, allocated by
 * sound_hw_alloc_voices). The synth above sets a voice's sample, pitch, gains and envelope through the sound_hw_*
 * setters; each subframe sound_hw_build_voice_command turns the record into the voice's SFD record and advances its
 * envelope and play position. Only hardware.c reads the fields. The offsets in the comments are the N64's; on the PC
 * the record is in host layout (data is a host-sized pointer, so the fields after it move). */
#ifndef BOOT_TAIL_AUDIO_RECORD_H
#define BOOT_TAIL_AUDIO_RECORD_H

typedef struct SoundHwVoice {
    unsigned char active;                 /* 0x00: HW_VOICE_OFF, HW_VOICE_STARTING (its first subframe), HW_VOICE_PLAYING */
    unsigned char key_held;               /* 0x01: 1 until key-off; then the envelope releases and a looping
                                           * sample plays its tail */
    unsigned short flags;                 /* 0x02: VOICE_FLAG_CARTRIDGE: the sample is fetched by DMA */
    unsigned short rate;                  /* 0x04: input samples per output frame, 4.12 fixed point, at most 2.0 */
    unsigned char pad06[2];
    double sample_position;               /* 0x08: the play position, in samples */
    unsigned int latched_position;        /* 0x10: sample_position at the last sound_hw_latch_voice_positions */
    unsigned int position;                /* 0x14: the latch before that: what sound_hw_get_voice_position
                                           * reports (the output plays about a frame behind the render) */
    short level[4];                       /* 0x18: the gains the last subframe ended on: left, right, surround,
                                           * aux (Q15) */
    unsigned short attack_ms;             /* 0x20: the envelope: attack and decay times, sustain level (1.0 =
                                           * the target gains), release time */
    unsigned short decay_ms;              /* 0x22 */
    float sustain_level;                  /* 0x24 */
    unsigned short release_ms;            /* 0x28 */
    unsigned char pad2A[2];
    unsigned char *data;                  /* 0x2C: the sample: a cartridge or RDRAM-window address (ADPCM: its
                                           * 0x100-byte codebook, then the blocks) */
    unsigned int length;                  /* 0x30: in samples */
    unsigned int loop_start;              /* 0x34 */
    unsigned int loop_length;             /* 0x38: 0 for a sample that does not loop */
    unsigned int tail;                    /* 0x3C: samples after the loop (0 when fewer than 10) */
    short target[4];                      /* 0x40: the gains the synth asked for (sound_hw_set_volume), Q15 */
    unsigned short stage_ms;              /* 0x48: the envelope stage's length */
    unsigned short pad4A;
    float envelope_level;                 /* 0x4C: the envelope's level now: the gains are target * level */
    unsigned int stage_time;              /* 0x50: time into the stage, 16.16 milliseconds */
    float stage_target;                   /* 0x54: the level the stage ends on */
    float stage_start;                    /* 0x58: the level the stage started from */
    unsigned char envelope_stage;         /* 0x5C: ENVELOPE_ATTACK, _DECAY, _SUSTAIN or _RELEASE */
    unsigned char format;                 /* 0x5D: SAMPLE_FORMAT_PCM16 or SAMPLE_FORMAT_ADPCM */
    unsigned char pad5E[2];
    unsigned char sfd_slot;               /* 0x60: its record's index in the last SFD (255: none yet) */
    unsigned char release_pending;        /* 0x61: stopped short (sound_hw_break_voice): the next SFD's release
                                           * mask frees its slot */
    unsigned char pad62[6];
} SoundHwVoice;
extern SoundHwVoice *sound_hw_voices;
extern void sound_hw_start_envelope(SoundHwVoice *);
extern void sound_hw_release_envelope(SoundHwVoice *);

#endif
