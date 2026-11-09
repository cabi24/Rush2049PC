/* src/rom/sound/groups.c: the MusyX sound library's sound groups.
 *
 * The sound project (wave 6, musyx_project) is a list of groups: each names, by id, the macros, samples, tables,
 * keymaps and layers one set of sounds needs; an effect group adds its effect table, a song group its two program
 * pages and its MIDI setups. sound_push_group registers a group's resources, found in the pool (wave 7) and the
 * sample directory (wave 8), in registry.c's tables; sound_pop_group releases the group pushed last; and
 * sound_start_song_in_group starts a song with a group's pages and MIDI setup (sequencer.c). The game pushes the
 * effect groups at boot (AudioInitSys) and a song's group when the song starts (AudioSongPlay).
 *
 * MusyX is the N64's sound library (the GameCube build, MAT3, has its own sound code): no MAT3 counterpart. The files
 * keep their N64 layout, swapped to host byte order in place when loaded (port/assets/musyx.c), so every offset in
 * them is a 32-bit count of bytes: docs/port/MUSYX_DATA.md 3 has the formats. */
#include "../rom_tu.h"

#define SOUND_ID_LIST_END 0xFFFF      /* ends a list of u16 resource ids */
#define SOUND_LIST_END 0xFFFFFFFFU    /* a size of all ones ends the project's groups and each pool list */
#define SOUND_MAX_GROUPS 128          /* groups pushed at once (sound_groups) */
#define GROUP_OFFSETS_BASE 8          /* a group's offsets count from its offsets array, 8 bytes in */
#define POOL_ENTRY_HEADER 8           /* a pool entry's {u32 size; u16 id; u16} before its payload */
#define LAYER_ENTRY_HEADER 12         /* a layer list's pool entry: the header, a u16, the u16 count, then the layers */

/* A group's type (ResourceGroup.type). */
#define GROUP_TYPE_SONGS 0
#define GROUP_TYPE_SFX 1

/* The lists a group's offsets lead to: the id lists of its resources, then its effect table (an effect group) or its
 * normal and drum program pages and MIDI setups (a song group). */
enum {
    GROUP_MACRO_IDS, GROUP_SAMPLE_IDS, GROUP_TABLE_IDS, GROUP_KEYMAP_IDS, GROUP_LAYER_IDS,
    GROUP_SFX_TABLE,
    GROUP_NORMAL_PAGE = 5, GROUP_DRUM_PAGE, GROUP_MIDI_SETUPS
};

/* An entry of a pool list: the next entry is next_offset bytes on, a next_offset of SOUND_LIST_END ends the list. */
typedef struct RelativeEntry { unsigned int next_offset; unsigned short id; } RelativeEntry;

/* The pool's header: the offsets, from the pool's start, of its macro, table, keymap and layer lists. */
typedef struct ResourceOffsets { unsigned int offsets[4]; } ResourceOffsets;
enum { POOL_MACROS, POOL_TABLES, POOL_KEYMAPS, POOL_LAYERS };

/* A pool entry of the layer list (LAYER_ENTRY_HEADER bytes, then layer_count 12-byte layers: synth.c's LayerEntry). */
typedef struct ResourceEntry { unsigned int next_offset; u16 id; u16 unknown06; u16 unknown08; u16 layer_count; } ResourceEntry;

/* The sample directory's 0x1C-byte record, as include/sound_sample_record.h declares it there. */
typedef struct SampleRecord { u16 identifier; u16 references; u32 offset; u32 cart_address; u8 descriptor[16]; } SampleRecord;

/* A group of the project: the next group is size bytes on. offsets[] index the enum above. */
typedef struct ResourceGroup { int size; u16 id; u16 type; u32 offsets[6]; } ResourceGroup;

/* A song group seen whole: the five id lists, then the program pages and the MIDI setups. */
typedef struct {
    u32 size;
    u16 id;
    u16 type;
    u32 id_lists[5];
    u32 normal_page;   /* GROUP_NORMAL_PAGE */
    u32 drum_page;     /* GROUP_DRUM_PAGE */
    u32 midi_setups;   /* GROUP_MIDI_SETUPS */
} ResourceHeader;

/* A song group's MIDI setup: 132 bytes, the song id it is for, then the 16 channels' program, volume, pan, reverb and
 * chorus (sequencer.c reads them). A list of them ends with id SOUND_ID_LIST_END. */
#pragma pack(1)
typedef struct { u16 id; u8 data[130]; } Program;
#pragma pack()

typedef struct SequencePageEntry SequencePageEntry;
typedef struct MidiSetup MidiSetup;
typedef struct SongHeader SongHeader;
typedef struct SequencePlayParams SequencePlayParams;

/* The library's state */
extern short sound_group_count;
extern u8 sound_initialized;
extern ResourceGroup *sound_groups[];

/* registry.c */
extern int sound_add_macro(u16, void *);
extern int sound_add_table(u16, void *);
extern int sound_add_keymap(u16, void *);
extern int sound_add_layer(u16, void *, u16);
extern int sound_remove_macro(u16);
extern int sound_remove_table(u16);
extern int sound_remove_keymap(u16);
extern int sound_remove_layer(u16);
extern unsigned int sound_hw_cart_address(unsigned int);
extern int sound_add_sample_directory(SampleRecord *, unsigned int);
extern int sound_add_sample(u16, SampleRecord *);
extern int sound_remove_sample(u16);
extern int sound_add_sfx_table(unsigned short, void *, unsigned short);
extern int sound_remove_sfx_table(unsigned short);

/* hardware.c, sequencer.c */
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern u32 sound_seq_start(SequencePageEntry *, SequencePageEntry *, MidiSetup *, SongHeader *, SequencePlayParams *);

/* Forgets every pushed group (the library's start-up; the registries are reset separately). */
void sound_reset_groups(void)
{
    sound_group_count = 0;
}

/* Finds the entry with this id in a pool list, which starts at entry; NULL if the list has none. */
void *sound_find_pool_entry(unsigned short id, RelativeEntry *entry)
{
    while (entry->next_offset != SOUND_LIST_END) {
        if (entry->id == id) {
            return entry;
        }
        entry = (RelativeEntry *)((unsigned char *)entry + entry->next_offset);
    }
    return 0;
}

/* The pool entry of the macro, table, keymap or layer with this id (resource: the pool), or NULL. */
void *sound_find_pool_macro(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[POOL_MACROS];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

void *sound_find_pool_table(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[POOL_TABLES];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

void *sound_find_pool_keymap(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[POOL_KEYMAPS];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

void *sound_find_pool_layer(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[POOL_LAYERS];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

/* Registers each macro of an id list (ended by SOUND_ID_LIST_END) that the pool has, by its payload (the steps);
 * the ids the pool lacks are skipped. */
void sound_register_macros(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    unsigned char *entry;
    while ((id = *ids) != SOUND_ID_LIST_END) {
        entry = sound_find_pool_macro(id, resources);
        if (entry != 0) {
            sound_add_macro(*ids, entry + POOL_ENTRY_HEADER);
        }
        ++ids;
    }
}

/* The same for tables (volume curves and envelopes). */
void sound_register_tables(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    unsigned char *entry;
    while ((id = *ids) != SOUND_ID_LIST_END) {
        entry = sound_find_pool_table(id, resources);
        if (entry != 0) {
            sound_add_table(*ids, entry + POOL_ENTRY_HEADER);
        }
        ++ids;
    }
}

/* The same for keymaps. */
void sound_register_keymaps(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    unsigned char *entry;
    while ((id = *ids) != SOUND_ID_LIST_END) {
        entry = sound_find_pool_keymap(id, resources);
        if (entry != 0) {
            sound_add_keymap(*ids, entry + POOL_ENTRY_HEADER);
        }
        ++ids;
    }
}

/* The same for layers: a layer list's payload follows a 12-byte header that holds its count. */
void sound_register_layers(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    ResourceEntry *entry;
    while ((id = *ids) != SOUND_ID_LIST_END) {
        entry = sound_find_pool_layer(id, resources);
        if (entry != 0) {
            sound_add_layer(*ids, (unsigned char *)entry + LAYER_ENTRY_HEADER, entry->layer_count);
        }
        ++ids;
    }
}

/* Releases each macro of an id list (one reference each: the registry frees one when nobody holds it). */
void sound_unregister_macros(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != SOUND_ID_LIST_END) {
        sound_remove_macro(id);
        id = *++ids;
    }
}

/* The same for tables. */
void sound_unregister_tables(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != SOUND_ID_LIST_END) {
        sound_remove_table(id);
        id = *++ids;
    }
}

/* The same for keymaps. */
void sound_unregister_keymaps(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != SOUND_ID_LIST_END) {
        sound_remove_keymap(id);
        id = *++ids;
    }
}

/* The same for layers. */
void sound_unregister_layers(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != SOUND_ID_LIST_END) {
        sound_remove_layer(id);
        id = *++ids;
    }
}

/* Registers the sample directory records (once: their base is the samples' cartridge address, sound_hw_cart_address
 * of samples), then each sample of the id list. */
void sound_register_samples(u16 *ids, unsigned int samples, SampleRecord *records)
{
    u16 id;
    if (sound_add_sample_directory(records, sound_hw_cart_address(samples))) {
        while ((id = *ids) != SOUND_ID_LIST_END) {
            sound_add_sample(*ids++, records);
        }
    }
}

/* Releases each sample of an id list, last to first (registry.c drops the sample directory with its last sample). */
void sound_unregister_samples(u16 *ids)
{
    u16 *end;
    end = ids;
    while (*end != SOUND_ID_LIST_END) {
        ++end;
    }
    --end;
    while (end >= ids) {
        sound_remove_sample(*end);
        --end;
    }
}

/* Registers an effect group's effect table: a u16 count, a u16, then count 12-byte entries (registry.c's
 * Descriptor). */
void sound_register_sfx_table(unsigned short id, unsigned short *table)
{
    sound_add_sfx_table(id, table + 2, table[0]);
}

/* Releases the effect table of group id. */
void sound_unregister_sfx_table(unsigned short id)
{
    sound_remove_sfx_table(id);
}

/* MusyX's sndPushGroup: finds group id in the project (groups) and registers its macros, samples (samples: where the
 * samples are in the cartridge, a ROM offset or a cartridge address; records: the sample directory), tables, keymaps
 * and layers from the pool (resources), and an effect group's effect table. Returns 1, or 0 if the library is not
 * started, SOUND_MAX_GROUPS groups are pushed, or the project has no such group. */
int sound_push_group(u8 *groups, u16 id, unsigned int samples, SampleRecord *records, ResourceOffsets *resources)
{
    u8 *group;
    if (sound_initialized != 0 && sound_group_count < SOUND_MAX_GROUPS) {
        group = groups;
        while (*(u32 *)group != SOUND_LIST_END) {
            if (((ResourceGroup *)group)->id == id) {
                sound_groups[sound_group_count] = (ResourceGroup *)group;
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[GROUP_MACRO_IDS]; p += GROUP_OFFSETS_BASE; sound_register_macros((u16 *)p, resources); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[GROUP_SAMPLE_IDS]; p += GROUP_OFFSETS_BASE; sound_register_samples((u16 *)p, samples, records); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[GROUP_TABLE_IDS]; p += GROUP_OFFSETS_BASE; sound_register_tables((u16 *)p, resources); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[GROUP_KEYMAP_IDS]; p += GROUP_OFFSETS_BASE; sound_register_keymaps((u16 *)p, resources); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[GROUP_LAYER_IDS]; p += GROUP_OFFSETS_BASE; sound_register_layers((u16 *)p, resources); }
                if (((ResourceGroup *)group)->type == GROUP_TYPE_SFX) {
                    u8 *p = group; p += ((ResourceGroup *)group)->offsets[GROUP_SFX_TABLE]; p += GROUP_OFFSETS_BASE; sound_register_sfx_table(id, (u16 *)p);
                }
                sound_group_count++;
                return 1;
            }
            group += ((ResourceGroup *)group)->size;
        }
    }
    return 0;
}

/* Releases the group pushed last: its resources' references and an effect group's effect table. Returns 1, or 0 when
 * nothing is pushed. */
int sound_pop_group(void)
{
    ResourceGroup *group;
    if (sound_initialized != 0 && sound_group_count != 0) {
        group = sound_groups[--sound_group_count];
        { u8 *p = (u8 *)group; p += group->offsets[GROUP_MACRO_IDS]; p += GROUP_OFFSETS_BASE; sound_unregister_macros((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[GROUP_SAMPLE_IDS]; p += GROUP_OFFSETS_BASE; sound_unregister_samples((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[GROUP_TABLE_IDS]; p += GROUP_OFFSETS_BASE; sound_unregister_tables((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[GROUP_KEYMAP_IDS]; p += GROUP_OFFSETS_BASE; sound_unregister_keymaps((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[GROUP_LAYER_IDS]; p += GROUP_OFFSETS_BASE; sound_unregister_layers((u16 *)p); }
        if (group->type == GROUP_TYPE_SFX) {
            sound_unregister_sfx_table(group->id);
        }
        return 1;
    }
    return 0;
}

/* An empty entry of the library's API: does nothing and returns 0. */
int sound_set_studio(unsigned int unused0, unsigned int unused1, unsigned int unused2, unsigned int unused3)
{
    return 0;
}

/* Starts a song with a pushed song group's program pages and its MIDI setup for song_id (sound_seq_start), under the
 * library lock unless the caller holds it (unlocked). song is the song's data (wave 10-21), options the play
 * parameters or NULL. Returns the song's handle, or -1 if the library is not started or the group or the setup is not
 * there. */
int sound_start_song_in_group(u16 id, u16 song_id, SongHeader *song, SequencePlayParams *options, u8 unlocked)
{
    int i;
    ResourceHeader *resource;
    void *normal_page;
    void *drum_page;
    Program *setup;
    int result;
    if (sound_initialized) {
        i = 0;
        result = id;
        for (; i < sound_group_count; i++) {
            resource = (ResourceHeader *)sound_groups[i];
            if (resource->id == id) {
                if (resource->type == GROUP_TYPE_SONGS) {
                    normal_page = (u8 *)resource + resource->normal_page + GROUP_OFFSETS_BASE;
                    drum_page = (u8 *)resource + resource->drum_page + GROUP_OFFSETS_BASE;
                    setup = (Program *)((u8 *)resource + resource->midi_setups + GROUP_OFFSETS_BASE);
                    for (; setup->id != SOUND_ID_LIST_END; setup++) {
                        if (setup->id == song_id) {
                            if (unlocked) {
                                result = sound_seq_start(normal_page, drum_page, (MidiSetup *)setup, song, options);
                            }
                            else {
                                sound_hw_lock();
                                result = sound_seq_start(normal_page, drum_page, (MidiSetup *)setup, song, options);
                                sound_hw_unlock();
                            }
                            return result;
                        }
                    }
                }
                return -1;
            }
        }
    }
    return -1;
}

/* MusyX's sndSeqPlay: sound_start_song_in_group, taking the library lock. */
int sound_start_song(unsigned short group, unsigned short song_id, SongHeader *song, SequencePlayParams *options)
{
    return sound_start_song_in_group(group, song_id, song, options, 0);
}
