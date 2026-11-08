/* GENERATED ROM-aligned TU — segment 0x16320 (rom/lib_16320)
 * layout map 231669c219ff8d17283ad97903995c8f4ca7b9a1708f46d32621282f1fe514f5.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"
/* PC: the registries below keep their payloads as 32-bit window addresses (into the pool, wave 7, in the heap):
 * N64A(p) is a window pointer's, N64A0(p) the same where the N64 stores the pointer itself, N64PTR(T, a) and
 * N64PTR0(T, a) the pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
#define N64A0(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))
#define N64PTR0(T, a) ((T)PC_N64P(a))

/* PROMOTED 2026-10-11 — sound_add_keymap
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the keymap and macro registries hold window addresses into the pool (wave 7, in the heap) as u32, so a
 * record stays 8 bytes with its id at +4: sound_find_macro searches with a stride of 8, and the comparators read
 * the id at +4 (docs/port/MUSYX_DATA.md). */
#pragma pack(1)
typedef struct { unsigned int payload; u16 id; u16 references; } KeymapRecord;
#pragma pack()
typedef union { KeymapRecord fields; unsigned int words[2]; } KeymapRecordStorage;
extern int sound_keymap_count;
extern KeymapRecordStorage sound_keymaps[];
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
int sound_add_keymap(u16 id, void *payload)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_keymap_count && ((KeymapRecord *)&sound_keymaps[i])->id < id; i++) {
    }
    if (i < sound_keymap_count) {
        if (((KeymapRecord *)&sound_keymaps[i])->id != id) {
            if (sound_keymap_count < 2048) {
                for (j = sound_keymap_count - 1; j >= i; j--) {
                    sound_keymaps[j + 1] = sound_keymaps[j];
                }
                sound_keymap_count++;
            } else {
                sound_hw_unlock();
                return 0;
            }
        } else {
            ((KeymapRecord *)&sound_keymaps[i])->references++;
            sound_hw_unlock();
            return 0;
        }
    } else {
        if (sound_keymap_count < 2048) {
            sound_keymap_count++;
        } else {
            sound_hw_unlock();
            return 0;
        }
    }
    ((KeymapRecord *)&sound_keymaps[i])->payload = N64A0(payload);
    ((KeymapRecord *)&sound_keymaps[i])->id = id;
    ((KeymapRecord *)&sound_keymaps[i])->references = 1;
    sound_hw_unlock();
    return 1;
}

/* PROMOTED 2026-10-11 — sound_remove_keymap
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
int sound_remove_keymap(u16 id)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_keymap_count && ((KeymapRecord *)&sound_keymaps[i])->id != id; i++) {
    }
    if (i != sound_keymap_count) {
        if (--((KeymapRecord *)&sound_keymaps[i])->references == 0) {
            for (j = i + 1; j < sound_keymap_count; j++) {
                sound_keymaps[j - 1] = sound_keymaps[j];
            }
            sound_keymap_count--;
            sound_hw_unlock();
            return 1;
        }
    }
    sound_hw_unlock();
    return 0;
}

/* Table record, hoisted before its first user. */
#pragma pack(1)
typedef struct { unsigned int payload; u16 id; u16 type; u16 references; u16 metadata; } LayerRecord;
#pragma pack()
extern int sound_layer_count;
typedef union { LayerRecord fields; unsigned int words[3]; } LayerRecordStorage;
extern LayerRecordStorage sound_layers[];
/* PROMOTED 2026-10-11 — sound_add_layer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
int sound_add_layer(u16 id, void *payload, u16 type)
{
    long i;
    long j;
    sound_hw_lock();
    for (i = 0; i < sound_layer_count && ((LayerRecord *)&sound_layers[i])->id < id; ++i) {
    }
    if (i < sound_layer_count) {
        if (((LayerRecord *)&sound_layers[i])->id != id) {
            if (sound_layer_count < 256) {
                for (j = sound_layer_count - 1; j >= i; --j) {
                    sound_layers[j + 1] = sound_layers[j];
                }
                ++sound_layer_count;
            } else {
                sound_hw_unlock();
                return 0;
            }
        } else {
            ((LayerRecord *)&sound_layers[i])->references++;
            sound_hw_unlock();
            return 0;
        }
    } else if (sound_layer_count < 256) {
        ++sound_layer_count;
    } else {
        sound_hw_unlock();
        return 0;
    }
    ((LayerRecord *)&sound_layers[i])->id = id;
    ((LayerRecord *)&sound_layers[i])->payload = N64A(payload);
    ((LayerRecord *)&sound_layers[i])->type = type;
    ((LayerRecord *)&sound_layers[i])->references = 1;
    sound_hw_unlock();
    return 1;
}

/* PROMOTED 2026-10-11 — sound_remove_layer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_remove_layer(u16 id)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_layer_count && ((LayerRecord *)&sound_layers[i])->id != id; i++) {
    }
    if (i != sound_layer_count) {
        if (--((LayerRecord *)&sound_layers[i])->references == 0) {
            for (j = i + 1; j < sound_layer_count; j++) {
                sound_layers[j - 1] = sound_layers[j];
            }
            sound_layer_count--;
            sound_hw_unlock();
            return 1;
        }
    }
    sound_hw_unlock();
    return 0;
}

/* Table record, hoisted before its first user. */
#pragma pack(1)
typedef struct { unsigned int payload; u16 id; u16 references; } TableRecord;
#pragma pack()
extern int sound_table_count;
typedef union { TableRecord fields; unsigned int words[2]; } TableRecordStorage;
extern TableRecordStorage sound_tables[];
/* PROMOTED 2026-10-11 — sound_add_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
int sound_add_table(u16 id, void *payload)
{
    long i;
    long j;
    sound_hw_lock();
    for (i = 0; i < sound_table_count && ((TableRecord *)&sound_tables[i])->id < id; ++i) {
    }
    if (i < sound_table_count) {
        if (((TableRecord *)&sound_tables[i])->id != id) {
            if (sound_table_count < 2048) {
                for (j = sound_table_count - 1; j >= i; --j) {
                    sound_tables[j + 1] = sound_tables[j];
                }
                ++sound_table_count;
            } else {
                sound_hw_unlock();
                return 0;
            }
        } else {
            sound_hw_unlock();
            ((TableRecord *)&sound_tables[i])->references++;
            return 0;
        }
    } else if (sound_table_count < 2048) {
        ++sound_table_count;
    } else {
        sound_hw_unlock();
        return 0;
    }
    ((TableRecord *)&sound_tables[i])->id = id;
    ((TableRecord *)&sound_tables[i])->payload = N64A(payload);
    ((TableRecord *)&sound_tables[i])->references = 1;
    sound_hw_unlock();
    return 1;
}

/* PROMOTED 2026-10-11 — sound_remove_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_remove_table(u16 id)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_table_count && ((TableRecord *)&sound_tables[i])->id != id; i++) {
    }
    if (i != sound_table_count) {
        if (--((TableRecord *)&sound_tables[i])->references == 0) {
            for (j = i + 1; j < sound_table_count; j++) {
                sound_tables[j - 1] = sound_tables[j];
            }
            sound_table_count--;
            sound_hw_unlock();
            return 1;
        }
    }
    sound_hw_unlock();
    return 0;
}

/* PROMOTED 2026-10-11 — sound_add_sample_directory
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern int sound_sample_directory_count;
extern RegisteredSamples sound_sample_directories[];
int sound_add_sample_directory(SampleRecord *records, void *base)
{
    int index;
    SampleRecord *entry;
    u16 count;
    u16 i;
    for (index = 0; index < sound_sample_directory_count &&
         sound_sample_directories[index].records != records; ++index) {
    }
    if (index == sound_sample_directory_count) {
        if (sound_sample_directory_count < 8) {
            count = 0;
            entry = records;
            while (entry->identifier != 0xFFFF) {
                ++count;
                ++entry;
            }
            entry = records;
            sound_hw_lock();
            sound_sample_directories[sound_sample_directory_count].records = records;
            sound_sample_directories[sound_sample_directory_count].base = base;
            sound_sample_directories[sound_sample_directory_count].count = count;
            for (i = 0; i < count; ++i, ++entry) {
                entry->references = 0;
            }
            ++sound_sample_directory_count;
            sound_hw_unlock();
            return 1;
        }
        return 0;
    }
    return 1;
}

/* PROMOTED 2026-10-11 — sound_remove_sample_directory
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern int sound_sample_directory_count;
extern RegisteredSamples sound_sample_directories[];
int sound_remove_sample_directory(SampleRecord *records)
{
    int index;
    int following;
    for (index = 0; index < sound_sample_directory_count &&
         sound_sample_directories[index].records != records; ++index) {
    }
    if (index != sound_sample_directory_count) {
        sound_hw_lock();
        for (following = index + 1; following < sound_sample_directory_count; ++following) {
            sound_sample_directories[following - 1] = sound_sample_directories[following];
        }
        --sound_sample_directory_count;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_add_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern int sound_sample_directory_count;
extern RegisteredSamples sound_sample_directories[];
extern void sound_hw_save_sample(void **, void **);
int sound_add_sample(u16 id, SampleRecord *records)
{
    u32 index;
    void *descriptor;
    for (index = 0; index < (u32)sound_sample_directory_count &&
         sound_sample_directories[index].records != records; ++index) {
    }
    while (records->identifier != 0xFFFF) {
        if (records->identifier == id) {
            if (records->references == 0) {
                records->data = N64A0(sound_sample_directories[index].base + records->offset);
                descriptor = records->descriptor;
                sound_hw_save_sample(&descriptor, &records->data);
            }
            ++records->references;
            break;
        }
        ++records;
    }
    return 1;
}

/* PROMOTED 2026-10-05 — sound_remove_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern void sound_hw_remove_sample(void *, unsigned int);
extern int sound_remove_sample_directory(SampleRecord *);
int sound_remove_sample(unsigned short id)
{
    SampleRecord *resource;
    int used;
    int found;
    unsigned int i;
    found = 0;
    for (i = 0; i < (unsigned int)sound_sample_directory_count; i++) {
        used = 0;
        for (resource = sound_sample_directories[i].records; resource->identifier != 0xffff; resource++) {
            if (resource->identifier == id) {
                found = 1;
                if (--resource->references == 0) {
                    sound_hw_remove_sample(resource->descriptor, resource->offset);
                }
            }
            if (resource->references != 0) {
                used = 1;
            }
        }
        if (found) {
            if (!used) {
                sound_remove_sample_directory(sound_sample_directories[i].records);
            }
            return 1;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_add_sfx_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct { unsigned char unknown_00[9]; unsigned char state; unsigned char unknown_0A[2]; } Descriptor;
#pragma pack(1)
typedef struct { u16 id; u16 count; Descriptor *descriptors; } RecordFields;
#pragma pack()
typedef union { RecordFields fields; unsigned int words[2]; } RecordStorage;
extern int sound_sfx_table_count;
extern RecordStorage sound_sfx_tables[];
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
int sound_add_sfx_table(u16 id, Descriptor *descriptors, u16 count)
{
    int i;
    for (i = 0; i < sound_sfx_table_count && ((RecordFields *)&sound_sfx_tables[i])->id != id; i++) {
    }
    if (i == sound_sfx_table_count && sound_sfx_table_count < 128) {
        sound_hw_lock();
        ((RecordFields *)&sound_sfx_tables[sound_sfx_table_count])->id = id;
        ((RecordFields *)&sound_sfx_tables[sound_sfx_table_count])->count = count;
        ((RecordFields *)&sound_sfx_tables[sound_sfx_table_count])->descriptors = descriptors;
        for (i = 0; i < count; i++) {
            descriptors->state = 31;
            descriptors++;
        }
        sound_sfx_table_count++;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_remove_sfx_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
int sound_remove_sfx_table(u16 id)
{
    int i;
    int j;
    for (i = 0; i < sound_sfx_table_count && ((RecordFields *)&sound_sfx_tables[i])->id != id; i++) {
    }
    if (i != sound_sfx_table_count) {
        sound_hw_lock();
        for (j = i + 1; j < sound_sfx_table_count; j++) {
            sound_sfx_tables[j - 1] = sound_sfx_tables[j];
        }
        sound_sfx_table_count--;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Macro main table record, hoisted before its first user (was in the sound_reset_registries block). */
#pragma pack(1)
typedef struct ResourceRange { u16 count; u16 first; } ResourceRange;
#pragma pack(0)
/* PROMOTED 2026-10-11 — sound_add_macro
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
/* PC: a u32 window address, as KeymapRecord. */
#pragma pack(1)
typedef struct { unsigned int payload; u16 id; u16 references; } MacroRecord;
#pragma pack()
typedef union { MacroRecord fields; unsigned int words[2]; } MacroRecordStorage;
extern int sound_macro_count;
extern ResourceRange sound_macro_buckets[512];
extern MacroRecordStorage sound_macros[];
int sound_add_macro(u16 id, void *payload)
{
    long main;
    long pos;
    long base;
    long i;
    sound_hw_lock();
    main = id >> 6;
    if (sound_macro_buckets[main].count == 0) {
        pos = base = sound_macro_buckets[main].first = sound_macro_count;
    } else {
        base = sound_macro_buckets[main].first;
        for (i = 0; i < sound_macro_buckets[main].count && ((MacroRecord *)&sound_macros[base + i])->id < id; ++i) {
        }
        if (i < sound_macro_buckets[main].count) {
            pos = base + i;
            if (((MacroRecord *)&sound_macros[pos])->id == id) {
                ((MacroRecord *)&sound_macros[pos])->references++;
                sound_hw_unlock();
                return 0;
            }
        } else {
            pos = base + i;
        }
    }
    if (sound_macro_count < 2048) {
        for (i = 0; i < 512; ++i) {
            if (sound_macro_buckets[i].first > base) {
                sound_macro_buckets[i].first++;
            }
        }
        i = sound_macro_count - 1;
        for (; i >= pos; --i) {
            sound_macros[i + 1] = sound_macros[i];
        }
        ((MacroRecord *)&sound_macros[pos])->id = id;
        ((MacroRecord *)&sound_macros[pos])->payload = N64A0(payload);
        ((MacroRecord *)&sound_macros[pos])->references = 1;
        sound_macro_buckets[main].count++;
        sound_macro_count++;
        sound_hw_unlock();
        return 1;
    }
    sound_hw_unlock();
    return 0;
}

/* PROMOTED 2026-10-11 — sound_remove_macro
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
#pragma pack()
int sound_remove_macro(u16 id)
{
    long main;
    long base;
    long i;
    sound_hw_lock();
    main = id >> 6;
    if (sound_macro_buckets[main].count != 0) {
        base = sound_macro_buckets[main].first;
        for (i = 0; i < sound_macro_buckets[main].count && ((MacroRecord *)&sound_macros[base + i])->id != id; ++i) {
        }
        if (i < sound_macro_buckets[main].count) {
            if (--((MacroRecord *)&sound_macros[base + i])->references == 0) {
                for (i = base + i + 1; i < sound_macro_count; ++i) {
                    sound_macros[i - 1] = sound_macros[i];
                }
                for (i = 0; i < 512; ++i) {
                    if (sound_macro_buckets[i].first > base) {
                        --sound_macro_buckets[i].first;
                    }
                }
                --sound_macro_buckets[main].count;
                --sound_macro_count;
            }
        }
    }
    sound_hw_unlock();
    return 0;
}

/* PROMOTED 2026-10-04 — sound_compare_macro_ids
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct PackedKey { unsigned char unknown00[4]; unsigned short key; } PackedKey;
#pragma pack()
int sound_compare_macro_ids(const PackedKey *left, const PackedKey *right)
{
    return left->key - right->key;
}

/* PROMOTED 2026-10-11 — sound_find_macro
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
#pragma pack()
#pragma pack(1)
#pragma pack()
int sound_compare_macro_ids(const PackedKey *left, const PackedKey *right);
void *sound_bsearch(const void *key, const void *base, int count, int size, int (*compare)(const void *, const void *));
void *sound_find_macro(u16 id)
{
    static long base_80016C20;
    static long main_80016C20;
    static MacroRecord key_80016C20;
    static MacroRecord *result_80016C20;

    main_80016C20 = id >> 6;
    if (sound_macro_buckets[main_80016C20].count != 0) {
        base_80016C20 = sound_macro_buckets[main_80016C20].first;
        key_80016C20.id = id;
        if ((result_80016C20 = sound_bsearch(&key_80016C20, &sound_macros[base_80016C20], sound_macro_buckets[main_80016C20].count, 8,
                                    (int (*)(const void *, const void *))sound_compare_macro_ids)) != 0) {
            return N64PTR0(void *, result_80016C20->payload);
        }
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_compare_sample_ids
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_compare_sample_ids(const unsigned short *left, const unsigned short *right)
{
    return *left - *right;
}

/* PROMOTED 2026-10-11 — sound_find_sample
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the layout sound_hw_set_voice_sample (hardware.c) reads, whose data pointer is 8 bytes: offset is a u32 there. */
#pragma pack(1)
typedef struct SampleDirectoryInfo { u32 info; u32 length; u32 loopOffset; u32 loopLength; } SampleDirectoryInfo;
typedef struct SampleLookup { u32 info; void *addr; u32 offset; u32 length; u32 loop; u32 loopLength; u8 compType; } SampleLookup;
#pragma pack()
int sound_compare_sample_ids(const unsigned short *left, const unsigned short *right);
int sound_find_sample(u16 id, SampleLookup *sample)
{
    static SampleRecord key_80016CF0;
    static SampleRecord *result_80016CF0;
    static SampleDirectoryInfo *header_80016CF0;
    long i;

    key_80016CF0.identifier = id;
    for (i = 0; i < sound_sample_directory_count; ++i) {
        if ((result_80016CF0 = sound_bsearch(&key_80016CF0, sound_sample_directories[i].records, sound_sample_directories[i].count, sizeof(SampleRecord),
                                    (int (*)(const void *, const void *))sound_compare_sample_ids)) != 0) {
            header_80016CF0 = (SampleDirectoryInfo *)result_80016CF0->descriptor;
            sample->info = header_80016CF0->info;
            sample->addr = result_80016CF0->data;
            sample->offset = 0;
            sample->loop = header_80016CF0->loopOffset;
            sample->length = header_80016CF0->length & 0xFFFFFF;
            sample->loopLength = header_80016CF0->loopLength;
            sample->compType = header_80016CF0->length >> 24;
            return 0;
        }
    }
    return -1;
}

/* PROMOTED 2026-10-04 — sound_compare_record_ids
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
int sound_compare_record_ids(const PackedKey *left, const PackedKey *right)
{
    return left->key - right->key;
}

/* PROMOTED 2026-10-11 — sound_find_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
#pragma pack(1)
#pragma pack()
int sound_compare_record_ids(const PackedKey *left, const PackedKey *right);
void *sound_bsearch(const void *key, const void *base, int count, int size, int (*compare)(const void *, const void *));
void *sound_find_table(u16 id)
{
    static TableRecord key_80038610;
    static TableRecord *result_80038610;

    key_80038610.id = id;
    if ((result_80038610 = sound_bsearch(&key_80038610, sound_tables, sound_table_count, sizeof(TableRecord),
                                (int (*)(const void *, const void *))sound_compare_record_ids)) != 0) {
        return N64PTR(void *, result_80038610->payload);
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_find_keymap
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
#pragma pack(1)
#pragma pack()
void *sound_find_keymap(u16 id)
{
    static KeymapRecord key_8003C618;
    static KeymapRecord *result_8003C618;

    key_8003C618.id = id;
    if ((result_8003C618 = sound_bsearch(&key_8003C618, sound_keymaps, sound_keymap_count, sizeof(KeymapRecord),
                                (int (*)(const void *, const void *))sound_compare_record_ids)) != 0) {
        return N64PTR0(void *, result_8003C618->payload);
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_compare_layer_ids
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
int sound_compare_layer_ids(const PackedKey *left, const PackedKey *right)
{
    return left->key - right->key;
}

/* PROMOTED 2026-10-11 — sound_find_layer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
#pragma pack(1)
#pragma pack()
int sound_compare_layer_ids(const PackedKey *left, const PackedKey *right);
void *sound_find_layer(u16 id, u16 *type)
{
    static LayerRecord key_8003CE20;
    static LayerRecord *result_8003CE20;

    key_8003CE20.id = id;
    if ((result_8003CE20 = sound_bsearch(&key_8003CE20, sound_layers, sound_layer_count, sizeof(LayerRecord),
                                (int (*)(const void *, const void *))sound_compare_layer_ids)) != 0) {
        *type = result_8003CE20->type;
        return N64PTR(void *, result_8003CE20->payload);
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_compare_sfx_ids
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct PackedLeadingKey { unsigned short key; } PackedLeadingKey;
#pragma pack()
int sound_compare_sfx_ids(const PackedLeadingKey *left, const PackedLeadingKey *right)
{
    return left->key - right->key;
}

/* PROMOTED 2026-10-11 — sound_find_sfx
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack()
typedef struct SfxSearchKey { u16 id; unsigned char unknown02[10]; } SfxSearchKey;
#pragma pack(1)
#pragma pack()
int sound_compare_sfx_ids(const PackedLeadingKey *left, const PackedLeadingKey *right);
Descriptor *sound_find_sfx(u16 id)
{
    static SfxSearchKey key_80017040;
    Descriptor *result;
    long i;

    key_80017040.id = id;
    for (i = 0; i < sound_sfx_table_count; ++i) {
        if ((result = sound_bsearch(&key_80017040, ((RecordFields *)&sound_sfx_tables[i])->descriptors,
                                    ((RecordFields *)&sound_sfx_tables[i])->count, sizeof(Descriptor),
                                    (int (*)(const void *, const void *))sound_compare_sfx_ids)) != 0) {
            return result;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-05 — sound_reset_registries
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern int sound_table_count;
extern int sound_keymap_count;
extern int sound_layer_count;
extern int sound_macro_count;
extern ResourceRange sound_macro_buckets[512];
extern void sound_hw_init_sample_memory(void);
void sound_reset_registries(void)
{
    int i;
    sound_sample_directory_count = 0;
    sound_table_count = 0;
    sound_keymap_count = 0;
    sound_layer_count = 0;
    sound_sfx_table_count = 0;
    sound_macro_count = 0;
    for (i = 0; i < 512; ++i) {
        sound_macro_buckets[i].count = 0;
        sound_macro_buckets[i].first = 0;
    }
    sound_hw_init_sample_memory();
}

