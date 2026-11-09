/* include/sound_sample_record.h: records the MusyX sound library's units share: the sample directory's records and the
 * registry of directories (registry.c, groups.c), a stream's buffer record, the emitter and listener list nodes
 * (streams_3d.c). Each is laid out as the library reads it on the PC (host pointers; the sample record keeps the
 * file's layout). */
#ifndef BOOT_TAIL_SAMPLE_CONTRACT_H
#define BOOT_TAIL_SAMPLE_CONTRACT_H

/* A record of the sample directory file (wave 8), 0x1C bytes: the sample's id, its reference count (written:
 * registry.c), its offset from the samples' base, its address (written: the 32-bit cartridge address, base + offset),
 * then its information (key and rate, format and length, loop start and length: docs/port/MUSYX_DATA.md 3.3). The
 * copy in groups.c matches. */
typedef struct SampleRecord {
    unsigned short identifier;
    unsigned short references;
    unsigned int offset;
    unsigned int data;
    unsigned char descriptor[16];
} SampleRecord;
/* A registered sample directory (sound_sample_directories): its records, their base address, their count. */
typedef struct RegisteredSamples {
    SampleRecord *records;
    unsigned char *base;
    unsigned short count;
    unsigned short unknown0A;
} RegisteredSamples;

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
