/* include/sound_sample_record.h: records the MusyX sound library's units share: the sample directory's records and the
 * registry of directories (registry.c, groups.c), a sample as the hardware layer starts it (registry.c, macros.c,
 * streams_3d.c, hardware.c), a stream's buffer record, the emitter and listener list nodes (streams_3d.c). Each is
 * laid out as the library reads it on the PC (host pointers; the sample record keeps the file's layout).
 *
 * The samples stay in the cartridge (the sample bank, ROM 0x39B40 on): a sample there is named by its cartridge
 * address, 0xB0000000 + its place in the ROM, the address the PI reads it from (docs/port/MUSYX_DATA.md 2). That is
 * a 32-bit number of the cartridge's bus, never memory: the records keep it as u32. */
#ifndef BOOT_TAIL_SAMPLE_CONTRACT_H
#define BOOT_TAIL_SAMPLE_CONTRACT_H

/* A record of the sample directory file (wave 8), 0x1C bytes: the sample's id, its reference count (written:
 * registry.c), its offset from the samples' base, its cartridge address (written: the base's plus the offset), then
 * its information (key and rate, format and length, loop start and length: docs/port/MUSYX_DATA.md 3.3). The copy in
 * groups.c matches. */
typedef struct SampleRecord {
    unsigned short identifier;
    unsigned short references;
    unsigned int offset;
    unsigned int cart_address;
    unsigned char descriptor[16];
} SampleRecord;
/* A registered sample directory (sound_sample_directories): its records, the samples' cartridge address (what their
 * offsets count from), their count. */
typedef struct RegisteredSamples {
    SampleRecord *records;
    unsigned int cart_base;
    unsigned short count;
    unsigned short unknown0A;
} RegisteredSamples;

/* A sample as the hardware layer starts it on a voice (sound_hw_set_voice_sample): a sample directory's (from
 * sound_find_sample: in the cartridge, at cart_address) or a stream's buffer (sound_stream_start: in RAM, at data).
 * Its key and rate (key << 24 | rate in Hz), the sample to start at, its length and loop in samples, its format (0
 * PCM16, 3 ADPCM). */
typedef struct SoundSampleInfo {
    unsigned int info;
    unsigned int cart_address;   /* a sample in the cartridge: its cartridge address; 0 for a sample in RAM */
    void *data;                  /* a sample in RAM: its first byte; NULL for a sample in the cartridge */
    unsigned int offset;
    unsigned int length;
    unsigned int loopStart;
    unsigned int loopLength;
    unsigned char format;
} SoundSampleInfo;

/* A stream's refill callback and buffer record (streams_3d.c's sound_streams, one a voice). */
typedef int (*SampleCallback)(short *, unsigned int, short *, unsigned int,
                              unsigned int);
typedef struct SampleBuffer {
    unsigned char mode;
    unsigned char unknown01[3];
    SampleCallback callback;
    short *buffer;
    unsigned int samples;
    unsigned int position;
    unsigned int context;
} SampleBuffer;

/* An emitter (streams_3d.c's sound_emitters list): its links, flags, then its state; the emitter functions read the
 * rest through their own views of the same record. */
typedef struct StateNode {
    struct StateNode *next;
    struct StateNode *previous;
    unsigned int flags08;
    unsigned char unknown0C[40];
    unsigned int identifier34;
    unsigned int group;
    unsigned short sound_id;
    unsigned short counter;
    float fade;
} StateNode;
typedef StateNode StatePrefix;

/* A listener's links (sound_listeners list); the listener views in streams_3d.c begin with them. */
typedef struct LinkNode {
    struct LinkNode *next;
    struct LinkNode *previous;
} LinkNode;

#endif
