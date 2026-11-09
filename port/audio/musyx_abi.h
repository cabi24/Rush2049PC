/* port/audio/musyx_abi.h: the audio task ABI of San Francisco Rush 2049 (N64).
 *
 * The audio task's microcode (audio_ucode_text, which docs/memory_map.md calls aspMain) runs Factor 5's MusyX v1
 * ABI, not Nintendo's Acmd command lists. OSTask.t.data_ptr points to an array of subframe descriptors (SFDs)
 * and OSTask.t.data_size is how many there are. Each SFD renders one subframe, 192 interleaved stereo frames,
 * into the output buffer named by its last voice. The game's builder is the MusyX driver in
 * src/rom/sound/hardware.c:
 *   sound_hw_build_voice_command  one voice record (SfdVoice; the game's 0x50-byte struct)
 *   sound_hw_build_subframe  one SFD (SfdBlock): release mask, state, delay effect, output pointer
 *   sound_hw_service_frame  one task: an SFD per free AI buffer, then sound_hw_submit_task builds the OSTask
 *   sound_hw_set_delay  the delay effect's parameter block (SoundDelayState)
 *   sound_hw_alloc_voices  allocations: SFDs at 2576 (0xA10) bytes each, the 664-byte (0x298) state block (both larger on
 *                  the PC with more than 32 voices: MX_VOICES_MAX below)
 *
 * The records the interpreter reads are the game's structs compiled for the host (PcMusyx* below): the N64's members
 * in the same order, the pointer members host pointers, natural alignment, host byte order. The N64 form below (byte
 * offsets, 32-bit N64 addresses, big-endian) is the microcode's: the task capture (port/audio/musyx.c,
 * RUSH2049_AUDIO_CAPTURE) writes its images in it, for a replay on the microcode (docs/port/AUDIO.md 8). */
#ifndef MUSYX_ABI_H
#define MUSYX_ABI_H

#include <stddef.h>
#include <stdint.h>

#define MX_SUBFRAME 192       /* stereo frames per SFD */
#define MX_VOICES 32          /* voice records per SFD on the N64 */
#define MX_STATE_SIZE 0x298   /* the work block carried between tasks; the game allocates 664 bytes */

/* More voices (the PC's [audio] voices, docs/port/AUDIO.md 10). The game's SFDs on the PC hold one voice record per
 * voice slot, n of them (pc_audio_set_sfd_voices; 32 unless the setting is above original), and after the n records
 * the release mask's words for slots 32 and up: u32 word k, bit b is slot 32 + 32k + b. The state block holds the
 * last output of slots 32 and up after its 0x298 bytes, 8 bytes a slot. With n = 32 every record is the N64's.
 * A voice's slot is a byte in the game's builder (sound_hw_build_voice_command), and 255 is its "none". */
#define MX_VOICES_MAX 255
#define MX_MASK_WORDS(n) ((n) > MX_VOICES ? ((n) - MX_VOICES + 31) / 32 : 0)
#define MX_STATE_SIZE_N(n) (MX_STATE_SIZE + ((n) > MX_VOICES ? ((n) - MX_VOICES) * 8 : 0))
#define MX_ALIGN8(x) (((x) + 7) & ~(size_t)7)

/* ---------------------------------------------------------------- N64 form (the capture's images): byte offsets */

enum {
    /* SFD, 0xA10 bytes */
    MX_SFD_COUNT = 0x00,         /* u16 voices written (the microcode ignores it; the output pointer ends the list) */
    MX_SFD_DELAY_INDEX = 0x02,   /* u16 subframe index into the delay line: position = index * 192 */
    MX_SFD_RELEASE_MASK = 0x04,  /* u32 bit n: the voice in slot n stopped; fold its last samples into the base */
    MX_SFD_STATE = 0x08,         /* u32 address of the 0x298-byte state block */
    MX_SFD_DELAY = 0x0C,         /* u32 address of the delay effect parameters, 0 for none */
    MX_SFD_VOICES = 0x10,        /* voice records */
    MX_SFD_SIZE = 0xA10,

    /* voice record, 0x50 bytes */
    MX_V_ENV = 0x00,             /* s32[4] volume per channel (left, right, surround, aux), Q15 in the high half */
    MX_V_ENV_STEP = 0x10,        /* s32[4] added to the volume after each output frame */
    MX_V_PITCH_FRAC = 0x20,      /* u16 fraction of the first input position, Q16 */
    MX_V_PITCH = 0x22,           /* u16 input samples per output frame, Q4.12 (the game caps it at 2.0) */
    MX_V_SRC = 0x24,             /* two source ranges, 12 bytes each: main segment, loop segment */
    MX_CAT_PTR1 = 0x00,          /*   u32 first range */
    MX_CAT_PTR2 = 0x04,          /*   u32 second range (a cache split) */
    MX_CAT_SIZE1 = 0x08,         /*   s16 bytes in the first range; 0 in voice 0 means "no voices" */
    MX_CAT_SIZE2 = 0x0A,         /*   s16 bytes in the second range */
    MX_CAT_SIZE = 0x0C,
    MX_V_FRAMES = 0x3C,          /* u8[2] ADPCM frames to decode (main, loop); main 0 means PCM16 */
    MX_V_SKIP = 0x3E,            /* u8[2] first sample: ADPCM 0..63 (bit 5: the block's second frame), PCM 0..3 */
    MX_V_BOOK = 0x40,            /* u32 ADPCM: codebook, 8 books of 2x8 s16 */
    MX_V_PCM_COUNT = 0x40,       /* u16[2] PCM16: samples in the main and loop segments (same word as BOOK) */
    MX_V_OUTPUT = 0x44,          /* u32 nonzero ends the voice list: 192 interleaved stereo s16 frames go here */
    MX_V_END = 0x48,             /* u16 end point, from the main segment's base */
    MX_V_RESTART = 0x4A,         /* u16 restart point: bit 15 set, in the loop segment; else from the main base */
    MX_V_4C = 0x4C,              /* u16 written by the game, not read */
    MX_V_START = 0x4E,           /* u16 added to the first sample's offset */
    MX_V_SIZE = 0x50,

    /* delay effect parameters, 0x48 bytes */
    MX_D_BUFFER = 0x00,          /* u32 circular line of s16, written by the microcode only */
    MX_D_LENGTH = 0x04,          /* u32 samples, a multiple of 192 */
    MX_D_TAPS = 0x08,            /* u16 taps used, up to 8 */
    MX_D_FEEDBACK = 0x0A,        /* s16 feedback gain, Q15 */
    MX_D_TAP_DELAY = 0x0C,       /* u32[8] samples */
    MX_D_TAP_GAIN = 0x2C,        /* s16[8] Q15 */
    MX_D_3C = 0x3C,              /* u32 not read by v1 */
    MX_D_FILTER = 0x40,          /* s16[4] feedback FIR, oldest tap first; {0,0,0,32767} passes through */
    MX_D_SIZE = 0x48,

    /* state block, 0x298 bytes, native byte order (only the interpreter reads it; the game zeroes it) */
    MX_ST_LAST = 0x000,          /* s16[32][4] each voice slot's last output, per channel */
    MX_ST_BASE = 0x100,          /* s32[4] carry of stopped voices, decaying each subframe */
    MX_ST_SURROUND = 0x110,      /* s16[192] the previous subframe's surround mix */
    MX_ST_HIST = 0x290,          /* s16[4] last wet samples, the feedback FIR's history */
};

/* ---------------------------------------------------------------- the records: the game's structs on the host */

/* SfdSampleRange */
typedef struct PcMusyxCat {
    const void *ptr1;
    const void *ptr2;
    int16_t size1;   /* bytes */
    int16_t size2;
} PcMusyxCat;

/* SfdVoice, with its 0x44 word as a pointer */
typedef struct PcMusyxVoice {
    int32_t env[4];
    int32_t env_step[4];
    uint16_t pitch_frac;
    uint16_t pitch;
    PcMusyxCat src[2];
    uint8_t frames[2];
    uint8_t skip[2];
    union {
        const void *book;
        struct {
            int16_t count[2];
        } pcm;
    } u;
    void *output;
    uint16_t end;
    uint16_t restart;
    uint16_t unused4c;
    uint16_t start;
} PcMusyxVoice;

/* SoundDelayState */
typedef struct PcMusyxDelay {
    int16_t *buffer;
    uint32_t length;
    uint16_t taps;
    int16_t feedback;
    uint32_t tap_delay[8];
    int16_t tap_gain[8];
    uint32_t unused3c;
    int16_t filter[4];
} PcMusyxDelay;

/* SfdBlock (with n voice records, MX_NATIVE_SFD_SIZE(n) bytes: MX_VOICES of them is this struct) */
typedef struct PcMusyxSfd {
    uint16_t count;
    uint16_t delay_index;
    uint32_t release_mask;
    void *state;
    const PcMusyxDelay *delay;
    PcMusyxVoice voices[MX_VOICES];
} PcMusyxSfd;

/* An SFD of n voice records in each form, rounded to 8 bytes so the next one's pointers stay aligned (for n = 32,
 * 0xA10 and sizeof(PcMusyxSfd)); and where its mask words start. */
#define MX_SFD_SIZE_N(n) MX_ALIGN8(MX_SFD_VOICES + (size_t)(n) * MX_V_SIZE + 4 * (size_t)MX_MASK_WORDS(n))
#define MX_SFD_MASK_AT(n) (MX_SFD_VOICES + (size_t)(n) * MX_V_SIZE)
#define MX_NATIVE_SFD_SIZE(n) \
    MX_ALIGN8(offsetof(PcMusyxSfd, voices) + (size_t)(n) * sizeof(PcMusyxVoice) + 4 * (size_t)MX_MASK_WORDS(n))
#define MX_NATIVE_MASK_AT(n) (offsetof(PcMusyxSfd, voices) + (size_t)(n) * sizeof(PcMusyxVoice))

#endif
