/* port/audio/musyx_abi.h: the audio task ABI of San Francisco Rush 2049 (N64).
 *
 * The audio task's microcode (0x8002A050, which docs/memory_map.md calls aspMain) runs Factor 5's MusyX v1
 * ABI, not Nintendo's Acmd command lists. OSTask.t.data_ptr points to an array of subframe descriptors (SFDs)
 * and OSTask.t.data_size is how many there are. Each SFD renders one subframe, 192 interleaved stereo frames,
 * into the output buffer named by its last voice. The game's builder is the MusyX driver in
 * src/rom/lib_11640.c:
 *   func_80012D18  one voice record (Command_80012D18; the game's 0x50-byte struct)
 *   func_800139D4  one SFD (Block_80012D18): release mask, state, delay effect, output pointer
 *   func_80013DEC  one task: an SFD per free AI buffer, then func_80011910 builds the OSTask
 *   func_80010E80  the delay effect's parameter block (AudioDelayState_80010E80)
 *   func_80011104  allocations: SFDs at 2576 (0xA10) bytes each, the 664-byte (0x298) state block
 *
 * Two forms of the same records:
 *   N64 form     byte offsets below, 32-bit N64 addresses (K0, K1 or physical) resolved with pc_n64_ptr().
 *                Multi-byte fields are big-endian when the SFD (or delay block) is in a pc_mem_is_be() range,
 *                native otherwise.
 *   native form  the game's structs compiled for the host: the same members in the same order, pointer
 *                members widened, natural alignment. PcMusyx* below are those structs. A pointer member holds a
 *                host pointer or, when below 4 GB, an N64 address resolved with pc_n64_ptr() (the game's heap
 *                and the sample caches are in the RDRAM window at host 0x80000000; the executable is PIE, so
 *                no host data sits below 4 GB). Byte order is native. */
#ifndef MUSYX_ABI_H
#define MUSYX_ABI_H

#include <stddef.h>
#include <stdint.h>

#define MX_SUBFRAME 192       /* stereo frames per SFD */
#define MX_VOICES 32          /* voice records per SFD */
#define MX_STATE_SIZE 0x298   /* the work block carried between tasks; the game allocates 664 bytes */

/* ---------------------------------------------------------------- N64 form: byte offsets */

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

/* ---------------------------------------------------------------- native form */

/* Fetch_80012730 */
typedef struct PcMusyxCat {
    const void *ptr1;
    const void *ptr2;
    int16_t size1;   /* bytes */
    int16_t size2;
} PcMusyxCat;

/* Command_80012D18, with its 0x44 word as a pointer */
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

/* AudioDelayState_80010E80 */
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

/* Block_80012D18 */
typedef struct PcMusyxSfd {
    uint16_t count;
    uint16_t delay_index;
    uint32_t release_mask;
    void *state;
    const PcMusyxDelay *delay;
    PcMusyxVoice voices[MX_VOICES];
} PcMusyxSfd;

#endif
