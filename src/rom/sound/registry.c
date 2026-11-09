/* src/rom/sound/registry.c: the MusyX sound library's resource registries.
 *
 * Every resource a pushed group brings (groups.c) is registered here by its 16-bit id: macros (the SoundMacro programs,
 * kept in 512 buckets by id >> 6), tables (volume curves and envelopes), keymaps, layers, the effect tables, and the
 * sample directories with their samples. Each registry is an array sorted by id with a reference count per record, so
 * a resource two groups share is registered once and freed when the last group lets go; lookups are binary searches
 * (sound_bsearch). The synth (synth.c), the macro interpreter (macros.c) and the effect start (sound_start_sfx) find
 * their resources here.
 *
 * MusyX is the N64's sound library: no MAT3 counterpart. A record holds its payload (in the pool, wave 7) as a
 * pointer; a sample directory's records hold their samples' cartridge addresses, 32 bits, as the file has room for
 * (docs/port/MUSYX_DATA.md 2, 4). */
#include "../rom_tu.h"
#include "musyx.h"
#include "sound_sample_record.h"

/* The registries' capacities. */
#define MAX_TABLES 2048
#define MAX_KEYMAPS 2048          /* Original game: sound_keymaps has room for 256 records only (0x800 bytes) */
#define MAX_LAYERS 256
#define MAX_MACROS 2048
#define MACRO_BUCKETS 512         /* a macro's bucket is its id >> MACRO_BUCKET_SHIFT */
#define MACRO_BUCKET_SHIFT 6
#define MAX_SFX_TABLES 128
#define MAX_SAMPLE_DIRECTORIES 8

#define SAMPLE_ID_END 0xFFFF            /* ends a sample directory */
#define SAMPLE_LENGTH_MASK 0xFFFFFF     /* a sample's length word: format << 24 | length in samples */

/* A registered macro, table or keymap: its payload in the pool (a macro's steps, a table's bytes, a keymap's 128
 * entries), its id, and how many pushed groups hold it. The registries are sorted by id. */
typedef struct ResourceRecord { void *payload; u16 id; u16 references; } ResourceRecord;

/* A registered layer list: its layers in the pool, count of them (synth.c's LayerEntry), its id, and how many pushed
 * groups hold it. */
typedef struct LayerRecord { void *payload; u16 id; u16 count; u16 references; } LayerRecord;

/* An effect of an effect table, 12 bytes (synth.c's Preset reads it whole): the effect's id, the macro, keymap or
 * layer it plays, its voice limit, priority, default volume and pan, key, and its volume group, the one byte the
 * library writes (SFX_DEFAULT_VOLUME_GROUP, or sound_set_sfx_volume_group's). */
#pragma pack(1)
typedef struct {
    u16 id;
    u16 macro_id;
    u8 max_voices;
    u8 priority;
    u8 volume;
    u8 pan;
    u8 key;
    u8 volume_group;
    u8 unknown0A[2];
} Descriptor;
#pragma pack()

/* An effect table's record: its group id, its count of effects, the effects (in the project, wave 6). */
#pragma pack(1)
typedef struct { u16 id; u16 count; Descriptor *descriptors; } RecordFields;
#pragma pack()
typedef union { RecordFields fields; unsigned int words[2]; } RecordStorage;

/* A macro bucket: count macros of the bucket, from index first of sound_macros. */
#pragma pack(1)
typedef struct ResourceRange { u16 count; u16 first; } ResourceRange;
#pragma pack(0)

/* The sample information in a sample directory record (SampleRecord.descriptor): key << 24 | rate, format << 24 |
 * length in samples, the loop's start and length. */
#pragma pack(1)
typedef struct SampleDirectoryInfo { u32 info; u32 length; u32 loopOffset; u32 loopLength; } SampleDirectoryInfo;
#pragma pack()

/* The comparator's view of an effect: its id first. */
#pragma pack(1)
typedef struct PackedLeadingKey { unsigned short key; } PackedLeadingKey;
#pragma pack()
typedef struct SfxSearchKey { u16 id; unsigned char unknown02[10]; } SfxSearchKey;

/* The registries */
extern int sound_keymap_count;
extern ResourceRecord sound_keymaps[];
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern int sound_layer_count;
extern LayerRecord sound_layers[];
extern int sound_table_count;
extern ResourceRecord sound_tables[];
extern int sound_sample_directory_count;
extern RegisteredSamples sound_sample_directories[];
extern void sound_hw_save_sample(void **, unsigned int *);
extern void sound_hw_remove_sample(void *, unsigned int);
extern int sound_sfx_table_count;
extern RecordStorage sound_sfx_tables[];
extern int sound_macro_count;
extern ResourceRange sound_macro_buckets[512];
extern ResourceRecord sound_macros[];
void *sound_bsearch(const void *key, const void *base, int count, int size, int (*compare)(const void *, const void *));
extern void sound_hw_init_sample_memory(void);

/* Registers the keymap with this id (payload: its 128 entries in the pool), or takes one more reference to it.
 * Returns 1 when it is new, 0 when it was there already or the registry is full. */
int sound_add_keymap(u16 id, void *payload)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_keymap_count && sound_keymaps[i].id < id; i++) {
    }
    if (i < sound_keymap_count) {
        if (sound_keymaps[i].id != id) {
            if (sound_keymap_count < MAX_KEYMAPS) {
                for (j = sound_keymap_count - 1; j >= i; j--) {
                    sound_keymaps[j + 1] = sound_keymaps[j];
                }
                sound_keymap_count++;
            } else {
                sound_hw_unlock();
                return 0;
            }
        } else {
            sound_keymaps[i].references++;
            sound_hw_unlock();
            return 0;
        }
    } else {
        if (sound_keymap_count < MAX_KEYMAPS) {
            sound_keymap_count++;
        } else {
            sound_hw_unlock();
            return 0;
        }
    }
    sound_keymaps[i].payload = payload;
    sound_keymaps[i].id = id;
    sound_keymaps[i].references = 1;
    sound_hw_unlock();
    return 1;
}

/* Drops one reference to the keymap with this id, removing it at the last. Returns 1 when removed. */
int sound_remove_keymap(u16 id)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_keymap_count && sound_keymaps[i].id != id; i++) {
    }
    if (i != sound_keymap_count) {
        if (--sound_keymaps[i].references == 0) {
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

/* Registers the layer list with this id (payload: its layers in the pool; count: how many), or takes one more
 * reference to it. Returns 1 when it is new. */
int sound_add_layer(u16 id, void *payload, u16 count)
{
    long i;
    long j;
    sound_hw_lock();
    for (i = 0; i < sound_layer_count && sound_layers[i].id < id; ++i) {
    }
    if (i < sound_layer_count) {
        if (sound_layers[i].id != id) {
            if (sound_layer_count < MAX_LAYERS) {
                for (j = sound_layer_count - 1; j >= i; --j) {
                    sound_layers[j + 1] = sound_layers[j];
                }
                ++sound_layer_count;
            } else {
                sound_hw_unlock();
                return 0;
            }
        } else {
            sound_layers[i].references++;
            sound_hw_unlock();
            return 0;
        }
    } else if (sound_layer_count < MAX_LAYERS) {
        ++sound_layer_count;
    } else {
        sound_hw_unlock();
        return 0;
    }
    sound_layers[i].id = id;
    sound_layers[i].payload = payload;
    sound_layers[i].count = count;
    sound_layers[i].references = 1;
    sound_hw_unlock();
    return 1;
}

/* Drops one reference to the layer list with this id, removing it at the last. Returns 1 when removed. */
int sound_remove_layer(u16 id)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_layer_count && sound_layers[i].id != id; i++) {
    }
    if (i != sound_layer_count) {
        if (--sound_layers[i].references == 0) {
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

/* Registers the table with this id (payload: its bytes in the pool), or takes one more reference to it. Returns 1
 * when it is new. */
int sound_add_table(u16 id, void *payload)
{
    long i;
    long j;
    sound_hw_lock();
    for (i = 0; i < sound_table_count && sound_tables[i].id < id; ++i) {
    }
    if (i < sound_table_count) {
        if (sound_tables[i].id != id) {
            if (sound_table_count < MAX_TABLES) {
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
            sound_tables[i].references++;
            return 0;
        }
    } else if (sound_table_count < MAX_TABLES) {
        ++sound_table_count;
    } else {
        sound_hw_unlock();
        return 0;
    }
    sound_tables[i].id = id;
    sound_tables[i].payload = payload;
    sound_tables[i].references = 1;
    sound_hw_unlock();
    return 1;
}

/* Drops one reference to the table with this id, removing it at the last. Returns 1 when removed. */
int sound_remove_table(u16 id)
{
    int i;
    int j;
    sound_hw_lock();
    for (i = 0; i < sound_table_count && sound_tables[i].id != id; i++) {
    }
    if (i != sound_table_count) {
        if (--sound_tables[i].references == 0) {
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

/* Registers a sample directory (records, ended by id SAMPLE_ID_END; cart_base: the samples' cartridge address, which
 * each record's offset counts from), clearing its records' reference counts. Returns 1 when it is registered (now or
 * before), 0 when MAX_SAMPLE_DIRECTORIES are. */
int sound_add_sample_directory(SampleRecord *records, unsigned int cart_base)
{
    int index;
    SampleRecord *entry;
    u16 count;
    u16 i;
    for (index = 0; index < sound_sample_directory_count &&
         sound_sample_directories[index].records != records; ++index) {
    }
    if (index == sound_sample_directory_count) {
        if (sound_sample_directory_count < MAX_SAMPLE_DIRECTORIES) {
            count = 0;
            entry = records;
            while (entry->identifier != SAMPLE_ID_END) {
                ++count;
                ++entry;
            }
            entry = records;
            sound_hw_lock();
            sound_sample_directories[sound_sample_directory_count].records = records;
            sound_sample_directories[sound_sample_directory_count].cart_base = cart_base;
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

/* Forgets the sample directory records. Returns 1, or 0 if it is not registered. */
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

/* Takes a reference to the sample with this id in the directory records: the first one sets the record's cartridge
 * address (the directory's plus the sample's offset). Always returns 1. */
int sound_add_sample(u16 id, SampleRecord *records)
{
    u32 index;
    void *descriptor;
    for (index = 0; index < (u32)sound_sample_directory_count &&
         sound_sample_directories[index].records != records; ++index) {
    }
    while (records->identifier != SAMPLE_ID_END) {
        if (records->identifier == id) {
            if (records->references == 0) {
                records->cart_address = sound_sample_directories[index].cart_base + records->offset;
                descriptor = records->descriptor;
                sound_hw_save_sample(&descriptor, &records->cart_address);
            }
            ++records->references;
            break;
        }
        ++records;
    }
    return 1;
}

/* Drops one reference to the sample with this id, in whichever directory has it; the directory is forgotten once none
 * of its samples is referenced. Returns 1 when a directory has the id. */
int sound_remove_sample(unsigned short id)
{
    SampleRecord *record;
    int used;
    int found;
    unsigned int i;
    found = 0;
    for (i = 0; i < (unsigned int)sound_sample_directory_count; i++) {
        used = 0;
        for (record = sound_sample_directories[i].records; record->identifier != SAMPLE_ID_END; record++) {
            if (record->identifier == id) {
                found = 1;
                if (--record->references == 0) {
                    sound_hw_remove_sample(record->descriptor, record->offset);
                }
            }
            if (record->references != 0) {
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

/* Registers the effect table of group id (count effects), setting each effect's volume group to the default.
 * Returns 1, or 0 if the group has one already or MAX_SFX_TABLES are registered. */
int sound_add_sfx_table(u16 id, Descriptor *descriptors, u16 count)
{
    int i;
    for (i = 0; i < sound_sfx_table_count && ((RecordFields *)&sound_sfx_tables[i])->id != id; i++) {
    }
    if (i == sound_sfx_table_count && sound_sfx_table_count < MAX_SFX_TABLES) {
        sound_hw_lock();
        ((RecordFields *)&sound_sfx_tables[sound_sfx_table_count])->id = id;
        ((RecordFields *)&sound_sfx_tables[sound_sfx_table_count])->count = count;
        ((RecordFields *)&sound_sfx_tables[sound_sfx_table_count])->descriptors = descriptors;
        for (i = 0; i < count; i++) {
            descriptors->volume_group = SFX_DEFAULT_VOLUME_GROUP;
            descriptors++;
        }
        sound_sfx_table_count++;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Forgets the effect table of group id. Returns 1, or 0 if there is none. */
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

/* Registers the macro with this id (payload: its steps in the pool), or takes one more reference to it: kept sorted
 * within its bucket, the buckets one after the other in sound_macros. Returns 1 when it is new. */
int sound_add_macro(u16 id, void *payload)
{
    long bucket;
    long pos;
    long base;
    long i;
    sound_hw_lock();
    bucket = id >> MACRO_BUCKET_SHIFT;
    if (sound_macro_buckets[bucket].count == 0) {
        pos = base = sound_macro_buckets[bucket].first = sound_macro_count;
    } else {
        base = sound_macro_buckets[bucket].first;
        for (i = 0; i < sound_macro_buckets[bucket].count && sound_macros[base + i].id < id; ++i) {
        }
        if (i < sound_macro_buckets[bucket].count) {
            pos = base + i;
            if (sound_macros[pos].id == id) {
                sound_macros[pos].references++;
                sound_hw_unlock();
                return 0;
            }
        } else {
            pos = base + i;
        }
    }
    if (sound_macro_count < MAX_MACROS) {
        for (i = 0; i < MACRO_BUCKETS; ++i) {
            if (sound_macro_buckets[i].first > base) {
                sound_macro_buckets[i].first++;
            }
        }
        i = sound_macro_count - 1;
        for (; i >= pos; --i) {
            sound_macros[i + 1] = sound_macros[i];
        }
        sound_macros[pos].id = id;
        sound_macros[pos].payload = payload;
        sound_macros[pos].references = 1;
        sound_macro_buckets[bucket].count++;
        sound_macro_count++;
        sound_hw_unlock();
        return 1;
    }
    sound_hw_unlock();
    return 0;
}

/* Drops one reference to the macro with this id, removing it at the last. Always returns 0. */
int sound_remove_macro(u16 id)
{
    long bucket;
    long base;
    long i;
    sound_hw_lock();
    bucket = id >> MACRO_BUCKET_SHIFT;
    if (sound_macro_buckets[bucket].count != 0) {
        base = sound_macro_buckets[bucket].first;
        for (i = 0; i < sound_macro_buckets[bucket].count && sound_macros[base + i].id != id; ++i) {
        }
        if (i < sound_macro_buckets[bucket].count) {
            if (--sound_macros[base + i].references == 0) {
                for (i = base + i + 1; i < sound_macro_count; ++i) {
                    sound_macros[i - 1] = sound_macros[i];
                }
                for (i = 0; i < MACRO_BUCKETS; ++i) {
                    if (sound_macro_buckets[i].first > base) {
                        --sound_macro_buckets[i].first;
                    }
                }
                --sound_macro_buckets[bucket].count;
                --sound_macro_count;
            }
        }
    }
    sound_hw_unlock();
    return 0;
}

/* sound_bsearch's comparator for macro records: by id. */
int sound_compare_macro_ids(const ResourceRecord *left, const ResourceRecord *right)
{
    return left->id - right->id;
}

/* The steps of the macro with this id, or NULL. The search key and result are static, as in the original. */
void *sound_find_macro(u16 id)
{
    static long base;
    static long bucket;
    static ResourceRecord key;
    static ResourceRecord *result;

    bucket = id >> MACRO_BUCKET_SHIFT;
    if (sound_macro_buckets[bucket].count != 0) {
        base = sound_macro_buckets[bucket].first;
        key.id = id;
        if ((result = sound_bsearch(&key, &sound_macros[base], sound_macro_buckets[bucket].count, sizeof(ResourceRecord),
                                    (int (*)(const void *, const void *))sound_compare_macro_ids)) != 0) {
            return result->payload;
        }
    }
    return 0;
}

/* sound_bsearch's comparator for sample records: by the id first. */
int sound_compare_sample_ids(const unsigned short *left, const unsigned short *right)
{
    return *left - *right;
}

/* Fills *sample with the sample with this id from the first directory that has it: its cartridge address, key and
 * rate, length, loop and format. Returns 0, or -1 if no directory has it. */
int sound_find_sample(u16 id, SoundSampleInfo *sample)
{
    static SampleRecord key;
    static SampleRecord *result;
    static SampleDirectoryInfo *info;
    long i;

    key.identifier = id;
    for (i = 0; i < sound_sample_directory_count; ++i) {
        if ((result = sound_bsearch(&key, sound_sample_directories[i].records, sound_sample_directories[i].count, sizeof(SampleRecord),
                                    (int (*)(const void *, const void *))sound_compare_sample_ids)) != 0) {
            info = (SampleDirectoryInfo *)result->descriptor;
            sample->info = info->info;
            sample->cart_address = result->cart_address;
            sample->data = 0;
            sample->offset = 0;
            sample->loopStart = info->loopOffset;
            sample->length = info->length & SAMPLE_LENGTH_MASK;
            sample->loopLength = info->loopLength;
            sample->format = info->length >> 24;
            return 0;
        }
    }
    return -1;
}

/* sound_bsearch's comparator for table and keymap records: by id. */
int sound_compare_record_ids(const ResourceRecord *left, const ResourceRecord *right)
{
    return left->id - right->id;
}

/* The table with this id, or NULL. */
void *sound_find_table(u16 id)
{
    static ResourceRecord key;
    static ResourceRecord *result;

    key.id = id;
    if ((result = sound_bsearch(&key, sound_tables, sound_table_count, sizeof(ResourceRecord),
                                (int (*)(const void *, const void *))sound_compare_record_ids)) != 0) {
        return result->payload;
    }
    return 0;
}

/* The keymap with this id (128 entries, one per key), or NULL. */
void *sound_find_keymap(u16 id)
{
    static ResourceRecord key;
    static ResourceRecord *result;

    key.id = id;
    if ((result = sound_bsearch(&key, sound_keymaps, sound_keymap_count, sizeof(ResourceRecord),
                                (int (*)(const void *, const void *))sound_compare_record_ids)) != 0) {
        return result->payload;
    }
    return 0;
}

/* sound_bsearch's comparator for layer records: by id. */
int sound_compare_layer_ids(const LayerRecord *left, const LayerRecord *right)
{
    return left->id - right->id;
}

/* The layer list with this id, its count of layers in *count; NULL if there is none. */
void *sound_find_layer(u16 id, u16 *count)
{
    static LayerRecord key;
    static LayerRecord *result;

    key.id = id;
    if ((result = sound_bsearch(&key, sound_layers, sound_layer_count, sizeof(LayerRecord),
                                (int (*)(const void *, const void *))sound_compare_layer_ids)) != 0) {
        *count = result->count;
        return result->payload;
    }
    return 0;
}

/* sound_bsearch's comparator for effects: by the id first. */
int sound_compare_sfx_ids(const PackedLeadingKey *left, const PackedLeadingKey *right)
{
    return left->key - right->key;
}

/* The effect with this id in the first effect table that has it, or NULL. */
Descriptor *sound_find_sfx(u16 id)
{
    static SfxSearchKey key;
    Descriptor *result;
    long i;

    key.id = id;
    for (i = 0; i < sound_sfx_table_count; ++i) {
        if ((result = sound_bsearch(&key, ((RecordFields *)&sound_sfx_tables[i])->descriptors,
                                    ((RecordFields *)&sound_sfx_tables[i])->count, sizeof(Descriptor),
                                    (int (*)(const void *, const void *))sound_compare_sfx_ids)) != 0) {
            return result;
        }
    }
    return 0;
}

/* Empties every registry (the library's start-up). */
void sound_reset_registries(void)
{
    int i;
    sound_sample_directory_count = 0;
    sound_table_count = 0;
    sound_keymap_count = 0;
    sound_layer_count = 0;
    sound_sfx_table_count = 0;
    sound_macro_count = 0;
    for (i = 0; i < MACRO_BUCKETS; ++i) {
        sound_macro_buckets[i].count = 0;
        sound_macro_buckets[i].first = 0;
    }
    sound_hw_init_sample_memory();
}
