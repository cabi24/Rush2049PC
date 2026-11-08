/* GENERATED ROM-aligned TU — segment 0x15a10 (rom/lib_15a10)
 * layout map 6642e6a192ea8a14e7ca410824561e61c14c8896ea423af8459b04cbb4b1a59d.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-04 — sound_reset_groups
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern short sound_group_count;
void sound_reset_groups(void)
{
    sound_group_count = 0;
}

/* PROMOTED 2026-10-11 — sound_find_pool_entry
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct RelativeEntry { unsigned int next_offset; unsigned short id; } RelativeEntry;
void *sound_find_pool_entry(unsigned short id, RelativeEntry *entry)
{
    while (entry->next_offset != 0xFFFFFFFFU) {
        if (entry->id == id) {
            return entry;
        }
        entry = (RelativeEntry *)((unsigned char *)entry + entry->next_offset);
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_find_pool_macro
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
struct ResourceOffsets;
struct RelativeEntry;
extern void *sound_find_pool_entry(unsigned short, struct RelativeEntry *);
void *sound_find_pool_macro(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[0];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

/* PROMOTED 2026-10-11 — sound_find_pool_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void *sound_find_pool_table(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[1];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

/* PROMOTED 2026-10-11 — sound_find_pool_keymap
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void *sound_find_pool_keymap(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[2];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

/* PROMOTED 2026-10-11 — sound_find_pool_layer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void *sound_find_pool_layer(unsigned short id, struct ResourceOffsets *resource)
{
    unsigned char *p = (unsigned char *)resource;
    p += ((unsigned int *)resource)[3];
    return sound_find_pool_entry(id, (struct RelativeEntry *)p);
}

/* PROMOTED 2026-10-04 — sound_register_macros
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct ResourceOffsets { unsigned int offsets[4]; } ResourceOffsets;
extern void *sound_find_pool_macro(u16, ResourceOffsets *);
extern int sound_add_macro(u16, void *);
void sound_register_macros(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    unsigned char *entry;
    while ((id = *ids) != 0xFFFF) {
        entry = sound_find_pool_macro(id, resources);
        if (entry != 0) {
            sound_add_macro(*ids, entry + 8);
        }
        ++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_register_tables
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *sound_find_pool_table(u16, ResourceOffsets *);
extern int sound_add_table(u16, void *);
void sound_register_tables(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    unsigned char *entry;
    while ((id = *ids) != 0xFFFF) {
        entry = sound_find_pool_table(id, resources);
        if (entry != 0) {
            sound_add_table(*ids, entry + 8);
        }
        ++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_register_keymaps
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void *sound_find_pool_keymap(u16, ResourceOffsets *);
extern int sound_add_keymap(u16, void *);
void sound_register_keymaps(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    unsigned char *entry;
    while ((id = *ids) != 0xFFFF) {
        entry = sound_find_pool_keymap(id, resources);
        if (entry != 0) {
            sound_add_keymap(*ids, entry + 8);
        }
        ++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_register_layers
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct ResourceEntry { unsigned int next_offset; u16 id; u16 unknown06; u16 unknown08; u16 parameter; } ResourceEntry;
extern void *sound_find_pool_layer(u16, ResourceOffsets *);
extern int sound_add_layer(u16, void *, u16);
void sound_register_layers(u16 *ids, ResourceOffsets *resources)
{
    u16 id;
    ResourceEntry *entry;
    while ((id = *ids) != 0xFFFF) {
        entry = sound_find_pool_layer(id, resources);
        if (entry != 0) {
            sound_add_layer(*ids, (unsigned char *)entry + 12, entry->parameter);
        }
        ++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_unregister_macros
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_remove_macro(u16);
void sound_unregister_macros(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != 0xFFFF) {
        sound_remove_macro(id);
        id = *++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_unregister_tables
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_remove_table(u16);
void sound_unregister_tables(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != 0xFFFF) {
        sound_remove_table(id);
        id = *++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_unregister_keymaps
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_remove_keymap(u16);
void sound_unregister_keymaps(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != 0xFFFF) {
        sound_remove_keymap(id);
        id = *++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_unregister_layers
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_remove_layer(u16);
void sound_unregister_layers(u16 *ids)
{
    u16 id;
    id = *ids;
    while (id != 0xFFFF) {
        sound_remove_layer(id);
        id = *++ids;
    }
}

/* PROMOTED 2026-10-04 — sound_register_samples
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the sample directory's 0x1C-byte record, as include/sound_sample_record.h declares it there. */
typedef struct SampleRecord { u16 identifier; u16 references; u32 offset; u32 data; u8 descriptor[16]; } SampleRecord;
extern void *sound_hw_translate_address(void *);
extern int sound_add_sample_directory(SampleRecord *, void *);
extern int sound_add_sample(u16, SampleRecord *);
void sound_register_samples(u16 *ids, void *address, SampleRecord *records)
{
    u16 id;
    if (sound_add_sample_directory(records, sound_hw_translate_address(address))) {
        while ((id = *ids) != 0xFFFF) {
            sound_add_sample(*ids++, records);
        }
    }
}

/* PROMOTED 2026-10-04 — sound_unregister_samples
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_remove_sample(u16);
void sound_unregister_samples(u16 *ids)
{
    u16 *end;
    end = ids;
    while (*end != 0xFFFF) {
        ++end;
    }
    --end;
    while (end >= ids) {
        sound_remove_sample(*end);
        --end;
    }
}

/* PROMOTED 2026-10-11 — sound_register_sfx_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_add_sfx_table(unsigned short, void *, unsigned short);
void sound_register_sfx_table(unsigned short id, unsigned short *data)
{
    sound_add_sfx_table(id, data + 2, data[0]);
}

/* PROMOTED 2026-10-11 — sound_unregister_sfx_table
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_remove_sfx_table(unsigned short);
void sound_unregister_sfx_table(unsigned short id)
{
    sound_remove_sfx_table(id);
}

/* PROMOTED 2026-10-11 — sound_push_group
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct ResourceGroup { int size; u16 id; u16 type; u32 offsets[6]; } ResourceGroup;
extern u8 sound_initialized;
extern ResourceGroup *sound_groups[];
extern void sound_register_macros(u16 *ids, ResourceOffsets *resources);
extern void sound_register_samples(u16 *ids, void *address, SampleRecord *records);
extern void sound_register_tables(u16 *ids, ResourceOffsets *resources);
extern void sound_register_keymaps(u16 *ids, ResourceOffsets *resources);
extern void sound_register_layers(u16 *ids, ResourceOffsets *resources);
extern void sound_register_sfx_table(unsigned short id, unsigned short *data);
int sound_push_group(u8 *groups, u16 id, void *address, SampleRecord *records, ResourceOffsets *resources)
{
    u8 *group;
    if (sound_initialized != 0 && sound_group_count < 128) {
        group = groups;
        while (*(u32 *)group != 0xFFFFFFFF) {
            if (((ResourceGroup *)group)->id == id) {
                sound_groups[sound_group_count] = (ResourceGroup *)group;
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[0]; p += 8; sound_register_macros((u16 *)p, resources); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[1]; p += 8; sound_register_samples((u16 *)p, address, records); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[2]; p += 8; sound_register_tables((u16 *)p, resources); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[3]; p += 8; sound_register_keymaps((u16 *)p, resources); }
                { u8 *p = group; p += ((ResourceGroup *)group)->offsets[4]; p += 8; sound_register_layers((u16 *)p, resources); }
                if (((ResourceGroup *)group)->type == 1) {
                    u8 *p = group; p += ((ResourceGroup *)group)->offsets[5]; p += 8; sound_register_sfx_table(id, (u16 *)p);
                }
                sound_group_count++;
                return 1;
            }
            group += ((ResourceGroup *)group)->size;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_pop_group
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_unregister_macros(u16 *ids);
extern void sound_unregister_samples(u16 *ids);
extern void sound_unregister_tables(u16 *ids);
extern void sound_unregister_keymaps(u16 *ids);
extern void sound_unregister_layers(u16 *ids);
extern void sound_unregister_sfx_table(unsigned short id);
int sound_pop_group(void)
{
    ResourceGroup *group;
    if (sound_initialized != 0 && sound_group_count != 0) {
        group = sound_groups[--sound_group_count];
        { u8 *p = (u8 *)group; p += group->offsets[0]; p += 8; sound_unregister_macros((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[1]; p += 8; sound_unregister_samples((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[2]; p += 8; sound_unregister_tables((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[3]; p += 8; sound_unregister_keymaps((u16 *)p); }
        { u8 *p = (u8 *)group; p += group->offsets[4]; p += 8; sound_unregister_layers((u16 *)p); }
        if (group->type == 1) {
            sound_unregister_sfx_table(group->id);
        }
        return 1;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_set_studio
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_set_studio(unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
    return 0;
}

/* PROMOTED 2026-10-12 — sound_start_song_in_group
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct { u32 metadata; u16 id; u16 type; u32 header[5]; u32 bank_a; u32 bank_b; u32 programs; } ResourceHeader;
#pragma pack(1)
typedef struct { u16 id; u8 data[130]; } Program;
#pragma pack()
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
typedef struct SequencePageEntry SequencePageEntry;
typedef struct MidiSetup MidiSetup;
typedef struct SongHeader SongHeader;
typedef struct SequencePlayParams SequencePlayParams;
extern u32 sound_seq_start(SequencePageEntry *, SequencePageEntry *, MidiSetup *, SongHeader *, SequencePlayParams *);
/* PC: stream and options are pointers passed as integers (options is on the caller's 64-bit stack). */
int sound_start_song_in_group(u16 id, u16 program_id, uintptr_t stream, uintptr_t options, u8 unlocked)
{
    int i;
    ResourceHeader *resource;
    void *bank_a;
    void *bank_b;
    Program *program;
    int result;
    if (sound_initialized) {
        i = 0;
        result = id;
        for (; i < sound_group_count; i++) {
            resource = (ResourceHeader *)sound_groups[i];
            if (resource->id == id) {
                if (resource->type == 0) {
                    bank_a = (u8 *)resource + resource->bank_a + 8;
                    bank_b = (u8 *)resource + resource->bank_b + 8;
                    program = (Program *)((u8 *)resource + resource->programs + 8);
                    for (; program->id != 0xffff; program++) {
                        if (program->id == program_id) {
                            if (unlocked) {
                                result = sound_seq_start(bank_a, bank_b, (MidiSetup *)program, (SongHeader *)stream, (SequencePlayParams *)options);
                            }
                            else {
                                sound_hw_lock();
                                result = sound_seq_start(bank_a, bank_b, (MidiSetup *)program, (SongHeader *)stream, (SequencePlayParams *)options);
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

/* PROMOTED 2026-10-11 — sound_start_song
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_start_song_in_group(unsigned short, unsigned short, uintptr_t, uintptr_t, unsigned char);
int sound_start_song(unsigned short group, unsigned short item, uintptr_t context, uintptr_t option)
{
    return sound_start_song_in_group(group, item, context, option, 0);
}

