/* GENERATED ROM-aligned TU — segment 0x17dc0 (rom/lib_17dc0)
 * layout map 8baa7d85ab5236a98d6d20c261b8e0ad26e5b810082eeb29bf486ec2aa49ae83.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"
#include "sequence_context.h"

/* PROMOTED 2026-10-05 — sound_seq_init_notes
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SequenceNode sound_seq_note_pool[256];
void sound_seq_init_notes(void)
{
    SequenceNode *previous;
    int i;
    previous = 0;
    sound_seq_free_notes = sound_seq_note_pool;
    for (i = 0; i < 256; i++) {
        sound_seq_note_pool[i].previous = previous;
        if (previous != 0) {
            previous->next = &sound_seq_note_pool[i];
        }
        previous = &sound_seq_note_pool[i];
    }
    previous->next = 0;
}

/* PROMOTED 2026-10-04 — sound_seq_free_player_notes
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SequenceNode *sound_seq_free_notes;
extern SequenceContext *sound_seq_current;
void sound_seq_free_player_notes(SequenceContext *state)
{
    SequenceNode *node;
    node = state->active;
    if (node != 0) {
        while (node->next != 0) {
            node = node->next;
        }
        if (sound_seq_free_notes != 0) {
            node->next = sound_seq_free_notes;
            sound_seq_free_notes->previous = node;
        }
        sound_seq_free_notes = state->active;
        state->active = 0;
    }
    node = state->pending;
    if (node != 0) {
        while (node->next != 0) {
            node = node->next;
        }
        if (sound_seq_free_notes != 0) {
            node->next = sound_seq_free_notes;
            sound_seq_free_notes->previous = node;
        }
        sound_seq_free_notes = state->pending;
        state->pending = 0;
    }
}

/* PROMOTED 2026-10-04 — sound_seq_stop_player_notes
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_stop_unlocked(u32);
void sound_seq_stop_player_notes(SequenceContext *context)
{
    SequenceNode *entry;
    entry = context->active;
    while (entry != 0) {
        sound_stop_unlocked(entry->identifier);
        entry = entry->next;
    }
    entry = context->pending;
    while (entry != 0) {
        sound_stop_unlocked(entry->identifier);
        entry = entry->next;
    }
}

/* PROMOTED 2026-10-04 — sound_seq_alloc_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
SequenceNode *sound_seq_alloc_note(void)
{
    SequenceNode *node;
    SequenceNode *head;
    node = sound_seq_free_notes;
    if (node != 0) {
        sound_seq_free_notes = node->next;
        if (sound_seq_free_notes != 0) {
            sound_seq_free_notes->previous = 0;
        }
        node->previous = 0;
        head = sound_seq_current->active;
        node->next = head;
        if (head != 0) {
            sound_seq_current->active->previous = node;
        }
        sound_seq_current->active = node;
    }
    return node;
}

/* PROMOTED 2026-10-04 — sound_seq_free_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_free_note(SequenceNode *node)
{
    if (node->next != 0) {
        node->next->previous = node->previous;
    }
    if (node->previous != 0) {
        node->previous->next = node->next;
    } else {
        sound_seq_current->active = node->next;
    }
    node->next = sound_seq_free_notes;
    if (node->next != 0) {
        sound_seq_free_notes->previous = node;
    }
    node->previous = 0;
    sound_seq_free_notes = node;
}

/* PROMOTED 2026-10-04 — sound_seq_free_released_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_free_released_note(SequenceNode *node)
{
    if (node->next != 0) {
        node->next->previous = node->previous;
    }
    if (node->previous != 0) {
        node->previous->next = node->next;
    } else {
        sound_seq_current->pending = node->next;
    }
    node->next = sound_seq_free_notes;
    if (node->next != 0) {
        sound_seq_free_notes->previous = node;
    }
    node->previous = 0;
    sound_seq_free_notes = node;
}

/* PROMOTED 2026-10-04 — sound_seq_release_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_release_note(SequenceNode *node)
{
    SequenceNode *head;
    if (node->next != 0) {
        node->next->previous = node->previous;
    }
    if (node->previous != 0) {
        node->previous->next = node->next;
    } else {
        sound_seq_current->active = node->next;
    }
    head = sound_seq_current->pending;
    node->next = head;
    if (head != 0) {
        sound_seq_current->pending->previous = node;
    }
    node->previous = 0;
    sound_seq_current->pending = node;
}

/* PROMOTED 2026-10-05 — sound_seq_reap_released_notes
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_reap_released_notes(void)
{
    SequenceNode *entry;
    SequenceNode *next;
    entry = sound_seq_current->pending;
    while (entry != 0) {
        next = entry->next;
        if (sound_check(entry->identifier) == 0xFFFFFFFFU) sound_seq_free_released_note(entry);
        entry = next;
    }
}

/* PROMOTED 2026-10-04 — sound_seq_reset_ids
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_seq_next_id;
void sound_seq_reset_ids(void)
{
    sound_seq_next_id = 0;
}

/* PROMOTED 2026-10-05 — sound_seq_assign_id
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned int sound_seq_assign_id(unsigned int slot)
{
    unsigned int identifier;
    int i;
    do {
        identifier = sound_seq_next_id++;
        sound_seq_next_id &= 0x7FFFFFFFU;
        for (i = 0; i < 8; i++) {
            if (sound_seq_players[i].inactiveFC1 == 0 &&
                sound_seq_players[i].identifier == identifier) {
                identifier = 0xFFFFFFFFU;
                break;
            }
        }
    } while (identifier == 0xFFFFFFFFU);
    sound_seq_players[slot].identifier = identifier;
    return identifier;
}

/* PROMOTED 2026-10-04 — sound_seq_find_slot
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SequenceContext sound_seq_players[8];
u32 sound_seq_find_slot(u32 identifier)
{
    int i;
    for (i = 0; i < 8; ++i) {
        if (sound_seq_players[i].inactiveFC1 == 0 &&
            sound_seq_players[i].identifier == (identifier & 0x7FFFFFFFU)) {
            return (identifier & 0x80000000U) | i;
        }
    }
    return 0xFFFFFFFFU;
}

/* PROMOTED 2026-10-11 — sound_seq_set_program
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct SequenceProgramEntry { u16 macro; u8 priority; u8 maxVoices; u32 unknown04; } SequenceProgramEntry;
#pragma pack(0)
/* PC: the slot's native layout (sequence_context.h) */
typedef struct SequenceProgramView { u32 identifier; SequenceProgramEntry *normtab; u8 normTrans[128]; SequenceProgramEntry *drumtab; u8 drumTrans[128]; u8 unknown10C[SEQ_PC_SPAN(song10C, unknownF80)]; u32 prgState[16]; } SequenceProgramView;
SEQ_PC_SAME(SequenceProgramView, normtab, normtab004); SEQ_PC_SAME(SequenceProgramView, drumtab, drumtab088);
SEQ_PC_SAME(SequenceProgramView, drumTrans, drumTrans08C); SEQ_PC_SAME(SequenceProgramView, prgState, unknownF80);
#define SEQ_PLAYER ((SequenceProgramView *)context)
void sound_seq_set_program(SequenceContext *context, unsigned char program, unsigned char channel)
{
    if (channel != 9) {
        program = SEQ_PLAYER->normTrans[program];
        if (program != 255) {
            SEQ_PLAYER->prgState[channel] = ((u32)SEQ_PLAYER->normtab[program].macro << 16) |
                ((u32)SEQ_PLAYER->normtab[program].priority << 8) | SEQ_PLAYER->normtab[program].maxVoices;
        }
    } else {
        program = SEQ_PLAYER->drumTrans[program];
        if (program != 255) {
            SEQ_PLAYER->prgState[channel] = ((u32)SEQ_PLAYER->drumtab[program].macro << 16) |
                ((u32)SEQ_PLAYER->drumtab[program].priority << 8) | SEQ_PLAYER->drumtab[program].maxVoices;
        }
    }
}

/* PROMOTED 2026-10-11 — sound_seq_set_channel_pressure
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_seq_current_index_byte;
/* PC: the low byte of the word sound_seq_current_index (the current sequence slot, sound_seq_tick), which the N64 reads as the
 * word's byte 3. One name, as an optimizing GCC takes two external variables for two objects and sound_seq_tick
 * stores the word, then calls the sequencer, which reads this byte and may be inlined into it
 * (tools/port/alias_check.py); and the low byte, as the PC holds the word little-endian, where byte 3 is its high
 * byte, 0, which sent these controller messages of every sequence to slot 0 (docs/port/OPTIMIZED.md section 8). */
#define sound_seq_current_index_byte ((u8)sound_seq_current_index)
extern void sound_midi_set_controller(u8 controller, u8 channel, u8 set, u8 value);
void sound_seq_set_channel_pressure(u8 value, u8 channel)
{
    sound_midi_set_controller(130, channel, sound_seq_current_index_byte, value);
}

/* PROMOTED 2026-10-11 — sound_seq_set_channel_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_set_channel_volume(u8 value, u8 channel)
{
    sound_midi_set_controller(7, channel, sound_seq_current_index_byte, value);
}

/* PROMOTED 2026-10-11 — sound_seq_build_program_map
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct MapRecord { u8 unknown00[5]; u8 key; u8 unknown06[2]; } MapRecord;
void sound_seq_build_program_map(u8 *map, MapRecord *record)
{
    u8 index;
    for (index = 0; index < 128; index++) map[index] = 255;
    index = 0;
    while (record->key != 255) {
        map[record->key] = index;
        index++;
        record++;
    }
}

/* PROMOTED 2026-10-11 — sound_seq_start
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
#pragma pack(1)
typedef struct SequencePageEntry { u16 macro; u8 prio, maxVoices, slot, index, reserved[2]; } SequencePageEntry;
#pragma pack()
typedef struct MidiSetupChannel { u8 program, volume, panning, reverb, chorus, reserved[3]; } MidiSetupChannel;
typedef struct MidiSetup { u16 songId, reserved; MidiSetupChannel channel[16]; } MidiSetup;
typedef struct SequenceTrackGroup { u8 track, volGroup; } SequenceTrackGroup;
typedef struct SequencePlayParams { u32 flags; u32 trackMute[2]; u16 speed; u16 time; u8 target; u8 pad11; u8 numSeqVolDef; SequenceTrackGroup *seqVolDef; u8 numFaded; u8 *faded; } SequencePlayParams;
typedef struct SongHeader { u32 tTab, unknown04, unknown08, mTrack, info; } SongHeader;
typedef struct SequenceTrackCursor { u8 *addr, *base; u32 pattern08, pattern0C; } SequenceTrackCursor;
/* PC: the three streams are pointers (SequenceTrack, sequence_context.h) */
typedef struct SequencePatternState { u32 unknown00[3]; u8 *active0C, *active10, *active14; u8 unknown18[16]; } SequencePatternState;
/* PC: the request and its result pointer from FC8 to FF4 are 0x38 bytes (sequence_context.h) */
typedef struct SequencePlayer { u32 identifier; SequencePageEntry *normtab; u8 normTrans[128]; SequencePageEntry *drumtab; u8 drumTrans[128]; SongHeader *arrbase; u32 trackMute[2]; u32 fraction118, whole11C, half120; u32 bpm; SequenceTrackCursor track[64]; u8 trackVolGroup[64]; SequencePatternState trackState[64]; u32 *mTrackBase, *mTrackAddr; u32 lowF70, highF74; SequenceNode *noteUsed, *noteKeyOff; u32 prgState[16]; u8 activeFC0, freeFC1; u16 speed; u8 defVGroup, disabledFC5; u16 valueFC6; u8 unknownFC8[SEQ_PC_SPAN(unknownFC8, unknownFF4)]; u8 syncFF4; u8 unknownFF5[3]; } SequencePlayer;
SEQ_PC_SAME(SequencePlayer, normtab, normtab004); SEQ_PC_SAME(SequencePlayer, drumTrans, drumTrans08C);
SEQ_PC_SAME(SequencePlayer, arrbase, song10C); SEQ_PC_SAME(SequencePlayer, trackMute, first110);
SEQ_PC_SAME(SequencePlayer, bpm, rate124); SEQ_PC_SAME(SequencePlayer, track, unknown128);
SEQ_PC_SAME(SequencePlayer, trackVolGroup, channels528); SEQ_PC_SAME(SequencePlayer, trackState, tracks568);
SEQ_PC_SAME(SequencePlayer, mTrackBase, timingStartF68); SEQ_PC_SAME(SequencePlayer, noteUsed, active);
SEQ_PC_SAME(SequencePlayer, noteKeyOff, pending); SEQ_PC_SAME(SequencePlayer, prgState, unknownF80);
SEQ_PC_SAME(SequencePlayer, freeFC1, inactiveFC1); SEQ_PC_SAME(SequencePlayer, speed, valueFC2);
SEQ_PC_SAME(SequencePlayer, defVGroup, channelFC4); SEQ_PC_SAME(SequencePlayer, valueFC6, valueFC6);
SEQ_PC_SAME(SequencePlayer, syncFF4, unknownFF4);
typedef char pc_seq_800178B0_size[sizeof(SequencePlayer) == sizeof(SequenceContext) ? 1 : -1];
typedef char pc_seq_800178B0_track[sizeof(SequenceTrackCursor) == sizeof(SequenceSlotTrack) ? 1 : -1];
typedef char pc_seq_800178B0_state[sizeof(SequencePatternState) == sizeof(SequenceTrack) ? 1 : -1];
extern u8 sound_track_volumes[64];
extern void sound_seq_build_program_map(u8 *map, MapRecord *record);
extern void sound_seq_set_program(SequenceContext *, unsigned char, unsigned char);
extern void sound_set_group_type(unsigned char, unsigned char);
extern void sound_midi_reset_channel(u8, u8);
extern unsigned int sound_seq_assign_id(unsigned int slot);
unsigned int sound_seq_start(SequencePageEntry *norm, SequencePageEntry *drum, MidiSetup *midiSetup, SongHeader *song,
                           SequencePlayParams *para)
{
    SongHeader *arr;
    u32 *tracktab;
    u8 *trans;
    SequencePageEntry *page;
    SequencePageEntry *entry;
    int i, j, t;
    SequencePlayer *nseq;
    u8 seqId;

    for (i = 0; i < 8; i++) {
        if (sound_seq_players[i].inactiveFC1 != 0) break;
    }
    if (i != 8) {
    nseq = (SequencePlayer *)&sound_seq_players[i];
    nseq->syncFF4 = 0;
    nseq->normtab = norm;
    nseq->drumtab = drum;
    nseq->arrbase = song;
    sound_seq_build_program_map(nseq->normTrans, (MapRecord *)nseq->normtab);
    sound_seq_build_program_map(nseq->drumTrans, (MapRecord *)nseq->drumtab);
    nseq->defVGroup = i + 23;
    for (j = 0; j < 64; j++) {
        nseq->trackVolGroup[j] = nseq->defVGroup;
    }
    if (para == NULL) {
        nseq->trackMute[0] = 0xFFFFFFFF;
        nseq->trackMute[1] = 0xFFFFFFFF;
        nseq->speed = 256;
    } else {
        if (para->flags & 1) {
            nseq->trackMute[0] = para->trackMute[0];
            nseq->trackMute[1] = para->trackMute[1];
        } else {
            nseq->trackMute[0] = 0xFFFFFFFF;
            nseq->trackMute[1] = 0xFFFFFFFF;
        }
        if (para->flags & 2) {
            nseq->speed = para->speed;
        } else {
            nseq->speed = 256;
        }
        if (para->flags & 8) {
            for (j = 0; j < para->numSeqVolDef; j++) {
                nseq->trackVolGroup[para->seqVolDef[j].track] = para->seqVolDef[j].volGroup;
                sound_set_group_type(para->seqVolDef[j].volGroup, 0);
            }
        }
        if (para->flags & 4) {
            sound_fade_volume_group_unlocked(para->target, para->time, nseq->defVGroup, 0, 0);
            for (j = 0; j < para->numFaded; j++) {
                sound_fade_volume_group_unlocked(para->target, para->time, para->faded[j], 0, 0);
            }
        }
    }
    arr = song;
    seqId = i;
    nseq->bpm = arr->info;
    sound_synth_set_tempo(nseq->bpm, seqId);
    if (arr->mTrack != 0) {
        nseq->mTrackBase = (u32 *)(arr->mTrack + (u32)arr);
        nseq->mTrackAddr = nseq->mTrackBase;
        nseq->lowF70 = 0;
        nseq->highF74 = 0;
    } else {
        nseq->mTrackBase = NULL;
    }
    nseq->disabledFC5 = 0;
    tracktab = (u32 *)(arr->tTab + (u32)arr);
    for (j = 0; j < 64; j++) {
        sound_track_volumes[j] = 127;
        nseq->track[j].pattern08 = 0;
        nseq->track[j].pattern0C = 0;
        nseq->trackState[j].active0C = 0;
        nseq->trackState[j].active10 = 0;
        nseq->trackState[j].active14 = 0;
        if (tracktab[j] != 0) {
            nseq->track[j].base = nseq->track[j].addr = (u8 *)(tracktab[j] + (u32)arr);
        } else {
            nseq->track[j].base = nseq->track[j].addr = NULL;
        }
    }
    nseq->noteUsed = NULL;
    nseq->noteKeyOff = NULL;
    for (j = 0; j < 16; j++) {
        sound_midi_reset_channel(j, seqId);
    }
    for (j = 0; j < 16; j++) {
        nseq->prgState[j] = 0xFFFFFFFF;
    }
    page = nseq->normtab;
    trans = nseq->normTrans;
    for (t = 0; t < 2; t++) {
        for (j = 0; j < 128; j++) {
            if (trans[j] != 0xFF) {
                entry = page;
                entry += trans[j];
                if (entry->slot != 0xFF) {
                    nseq->prgState[entry->slot] = entry->maxVoices | (entry->macro << 16) | (entry->prio << 8);
                }
            }
        }
        page = nseq->drumtab;
        trans = nseq->drumTrans;
    }
    if (midiSetup != NULL) {
        for (j = 0; j < 16; j++) {
            sound_seq_set_program((SequenceContext *)nseq, midiSetup->channel[j].program, j);
            sound_midi_set_controller(7, j, seqId, midiSetup->channel[j].volume);
            sound_midi_set_controller(10, j, seqId, midiSetup->channel[j].panning);
            sound_midi_set_controller(91, j, seqId, midiSetup->channel[j].reverb);
            sound_midi_set_controller(93, j, seqId, midiSetup->channel[j].chorus);
        }
    }
    nseq->valueFC6 = 0;
    if (para != NULL) {
        if (!(para->flags & 0x10)) {
            nseq->activeFC0 = 1;
        }
    } else {
        nseq->activeFC0 = 1;
    }
    i = sound_seq_assign_id(i);
    nseq->freeFC1 = 0;
    return i;
    }
    return 0xFFFFFFFF;
}

/* PROMOTED 2026-10-11 — sound_seq_play_note_events
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SequenceTrackEvents { unsigned int fraction; int whole; unsigned int event_time; unsigned char *events, *pitch, *modulation; unsigned short pitch_value, modulation_value; unsigned int pitch_time, modulation_time; unsigned char channel; signed char velocity_offset, transpose; unsigned char number; } SequenceTrackEvents;
/* PC: the slot's native layout (sequence_context.h) */
typedef struct SequenceContextEvents { unsigned char unknown000[SEQ_PC_OFF(first110)]; unsigned int enabled[2]; unsigned char unknown118[8]; unsigned int lookahead; unsigned char unknown124[SEQ_PC_SPAN(rate124, channels528)]; unsigned char groups[64]; SequenceTrackEvents tracks[64]; unsigned char unknownF68[SEQ_PC_SPAN(timingStartF68, unknownF80)]; unsigned int programs[16]; unsigned char unknownFC0[8]; unsigned char request[SEQ_PC_SPAN(unknownFC8, pendingFF0)]; unsigned int *result; unsigned char pending; unsigned char unknownFF5[3]; } SequenceContextEvents;
SEQ_PC_SAME(SequenceContextEvents, enabled, first110); SEQ_PC_SAME(SequenceContextEvents, lookahead, half120);
SEQ_PC_SAME(SequenceContextEvents, groups, channels528); SEQ_PC_SAME(SequenceContextEvents, tracks, tracks568);
SEQ_PC_SAME(SequenceContextEvents, programs, unknownF80); SEQ_PC_SAME(SequenceContextEvents, request, unknownFC8);
SEQ_PC_SAME(SequenceContextEvents, result, pendingFF0); SEQ_PC_SAME(SequenceContextEvents, pending, unknownFF4);
typedef char pc_seq_17D38_track[sizeof(SequenceTrackEvents) == sizeof(SequenceTrack) ? 1 : -1];
typedef struct SequenceTime { unsigned int fraction, whole; } SequenceTime;
#define CTX ((SequenceContextEvents *)sound_seq_current)
extern SequenceNode *sound_seq_alloc_note(void);
extern void sound_seq_free_note(SequenceNode *);
extern void sound_seq_set_program(SequenceContext *, unsigned char, unsigned char);
extern void sound_seq_set_channel_pressure(unsigned char, unsigned char);
extern void sound_seq_crossfade_unlocked(void *, unsigned int *, unsigned char);
extern void sound_midi_set_controller(unsigned char, unsigned char, unsigned char, unsigned char);
extern unsigned int sound_synth_start_note(unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned short, unsigned short, unsigned char, short);
unsigned char sound_seq_play_note_events(void)
{
    int unused0, unused1, unused2, unused3; /* frame residual, see header */
    unsigned char channel,active;
    int unused4;                            /* frame residual, see header */
    int i,advance,key,velocity,number;
    unsigned int due,program,identifier;
    SequenceNode *note;
    active=0;
    for (i=0;i<64;i++) {
        if (CTX->tracks[i].events!=0) active=1;
        while (CTX->tracks[i].events!=0) {
            due=*(unsigned short *)CTX->tracks[i].events+CTX->tracks[i].event_time;
            if (CTX->tracks[i].whole+CTX->lookahead<due) break;
            CTX->tracks[i].event_time=due;
            channel=CTX->tracks[i].channel;
            key=CTX->tracks[i].events[2];velocity=CTX->tracks[i].events[3];
            if (key==255 && velocity==255) {
                CTX->tracks[i].events=0;
            } else {
                advance=4;
                if ((key&128)==128 && velocity==0) {
                    sound_seq_set_program(sound_seq_current,key&127,channel);
                } else if ((key&128)==128 && velocity==1) {
                    sound_seq_set_channel_pressure(key&127,channel);
                } else if ((key&128)==128 && (velocity&128)==128) {
                    if ((velocity&127)==104) {
                        if (CTX->pending) {
                            sound_seq_crossfade_unlocked(CTX->request,CTX->result,1);
                            CTX->pending=0;
                        }
                    } else {
                        sound_midi_set_controller(velocity&127,channel,sound_seq_current_index_byte,key&127);
                    }
                } else if (key!=0 || velocity!=0) {
                    advance=6;
                    number=CTX->tracks[i].number;
                    if (CTX->enabled[number/32] & (1U<<(number&31))) {
                        program=CTX->programs[channel];
                        if (program!=0xFFFFFFFFU) {
                            key+=CTX->tracks[i].transpose;
                            key=key>127 ? 127 : key<0 ? 0 : key;
                            velocity+=CTX->tracks[i].velocity_offset;
                            velocity=velocity>127 ? 127 : velocity<0 ? 0 : velocity;
                            note=sound_seq_alloc_note();
                            if (note!=0) {
                                identifier=sound_synth_start_note(program,key,velocity,64,channel,
                                    sound_seq_current_index,0,number & 255,CTX->groups[number],
                                    sound_seq_fading_out ? -1 : 0);
                                if (identifier!=0xFFFFFFFFU) {
                                    note->identifier=identifier;
                                    note->deadline0C=*(unsigned short *)(CTX->tracks[i].events+4)+due;
                                    *(SequenceTime *)&note->fraction10=*(SequenceTime *)&CTX->tracks[i].fraction;
                                } else sound_seq_free_note(note);
                            }
                        }
                    }
                }
                CTX->tracks[i].events+=advance;
            }
        }
    }
    return active;
}

/* PROMOTED 2026-10-11 — sound_seq_advance_pattern_clocks
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_advance_pattern_clocks(void)
{
    int i;
    int sum;
    for (i = 0; i < 64; i++) {
        if (sound_seq_current->tracks568[i].active0C || sound_seq_current->tracks568[i].active10 ||
            sound_seq_current->tracks568[i].active14) {
            sum = (int)(sound_seq_current->tracks568[i].fraction + sound_seq_current->fraction118);
            sound_seq_current->tracks568[i].fraction = (unsigned int)sum & 65535;
            sum >>= 16;
            sound_seq_current->tracks568[i].whole = (int)((unsigned int)sound_seq_current->tracks568[i].whole +
                (unsigned int)sum + (unsigned int)sound_seq_current->whole11C);
        }
    }
}

/* PROMOTED 2026-10-11 — sound_seq_end_due_notes
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_key_off_unlocked(unsigned int);
int sound_seq_end_due_notes(void)
{
    SequenceNode *node;
    SequenceNode *next;
    int sum;
    if (sound_seq_current->active == 0) return 0;
    node = sound_seq_current->active;
    while (node != 0) {
        next = node->next;
        sum = (int)((unsigned int)node->whole14 + (unsigned int)sound_seq_current->half120);
        if (sum >= node->deadline0C) {
            sound_key_off_unlocked(node->identifier);
            sound_seq_release_note(node);
        } else {
            sum = (int)(node->fraction10 + sound_seq_current->fraction118);
            node->fraction10 = (unsigned int)sum & 65535;
            sum >>= 16;
            node->whole14 = (int)((unsigned int)node->whole14 + (unsigned int)sum +
                (unsigned int)sound_seq_current->whole11C);
        }
        node = next;
    }
    return 1;
}

/* PROMOTED 2026-10-11 — sound_seq_play_pitch_bend_streams
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SequenceTrackNotes { unsigned int fraction; int whole; unsigned int unknown08; unsigned char *notes0C, *first10, *second14; unsigned short first18, second1A; unsigned int firstTime1C, secondTime20; unsigned char channel24, unknown25[3]; } SequenceTrackNotes;
typedef char pc_seq_1824C_track[sizeof(SequenceTrackNotes) == sizeof(SequenceTrack) ? 1 : -1];
#define TRACK(i) (((SequenceTrackNotes *)sound_seq_current->tracks568)[i])
void sound_seq_play_pitch_bend_streams(void)
{
    int i;
    unsigned char done;
    unsigned char first,second;
    unsigned short increment;
    short change;
    unsigned int now,deadline;
    int width;
    for (i=0;i<64;i++) {
        done=0;
        if (TRACK(i).first10 != 0) {
            do {
                first=TRACK(i).first10[0];
                second=TRACK(i).first10[1];
                if (first==128 && second==0) {
                    TRACK(i).first10=0;
                    done=1;
                } else {
                    if (first & 128) {
                        increment=((first & 127) << 8) | second;
                        width=2;
                    } else {
                        increment=first;
                        width=1;
                    }
                    deadline=TRACK(i).firstTime1C+increment;
                    now=TRACK(i).whole+sound_seq_current->half120;
                    if (now >= deadline) {
                        TRACK(i).first10 += width;
                        TRACK(i).firstTime1C=deadline;
                        first=TRACK(i).first10[0];
                        second=TRACK(i).first10[1];
                        if (first & 128) {
                            change=((first & 255) << 8) | second;
                            change <<= 1;
                            change >>= 1;
                            TRACK(i).first10 += 2;
                        } else {
                            change=first;
                            change <<= 9;
                            change >>= 9;
                            ++TRACK(i).first10;
                        }
                        TRACK(i).first18 += change;
                        sound_midi_set_controller(128,TRACK(i).channel24,(unsigned char)sound_seq_current_index,TRACK(i).first18 >> 7);
                        sound_midi_set_controller(129,TRACK(i).channel24,(unsigned char)sound_seq_current_index,TRACK(i).first18 & 127);
                    } else {
                        done=1;
                    }
                }
            } while (!done);
        }
    }
}

/* PROMOTED 2026-10-12 — sound_seq_play_modulation_streams
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#define TRACK(i) (((SequenceTrackNotes *)sound_seq_current->tracks568)[i])
void sound_seq_play_modulation_streams(void)
{
    int i;
    unsigned char done;
    unsigned char first,second;
    short change;
    unsigned int now,deadline;
    int width;
    for (i=0;i<64;i++) {
        done=0;
        if (TRACK(i).second14 != 0) {
            do {
                first=TRACK(i).second14[0];
                second=TRACK(i).second14[1];
                if (first==128 && second==0) {
                    TRACK(i).second14=0;
                    done=1;
                } else {
                    if (first & 128) {
                        change=((short)(first & 127) << 8) | (short)second;
                        width=2;
                    } else {
                        change=first;
                        width=1;
                    }
                    deadline=TRACK(i).secondTime20+change;
                    now=TRACK(i).whole+sound_seq_current->half120;
                    if (now >= deadline) {
                        TRACK(i).second14 += width;
                        TRACK(i).secondTime20=deadline;
                        first=TRACK(i).second14[0];
                        second=TRACK(i).second14[1];
                        if (first & 128) {
                            change=((short)(first & 255) << 8) | (short)second;
                            change <<= 1;
                            change >>= 1;
                            TRACK(i).second14 += 2;
                        } else {
                            change=(signed char)first;
                            ++TRACK(i).second14;
                        }
                        TRACK(i).second1A += change;
                        sound_midi_set_controller(1,TRACK(i).channel24,(unsigned char)sound_seq_current_index,TRACK(i).second1A >> 7);
                        sound_midi_set_controller(33,TRACK(i).channel24,(unsigned char)sound_seq_current_index,TRACK(i).second1A & 127);
                    } else {
                        done=1;
                    }
                }
            } while (!done);
        }
    }
}

/* PROMOTED 2026-10-11 — sound_seq_play_tracks
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SongTrackEvent { u32 time; u8 program; u8 controller; u16 unknown06; u16 code; union { u16 jump; struct { s8 first; s8 second; } bytes; } value; } SongTrackEvent;
typedef struct SongHeaderView { u32 unknown00; u32 patterns; u32 channels; u32 unknown0C; u32 unknown10; u32 loop_time; } SongHeaderView;
typedef struct SongPattern { u32 unknown00; u32 optional04; u32 optional08; u8 data[1]; } SongPattern;
typedef struct SongTrackCursor { SongTrackEvent *start; SongTrackEvent *cursor; u32 fraction; u32 time; } SongTrackCursor;
typedef struct SongPatternPlayback { u32 counter00; u32 counter04; u32 counter08; u8 *cursor; u8 *optional10; u8 *optional14; u16 value18; u16 value1A; u32 counter1C; u32 counter20; u8 channel; s8 second; s8 first; u8 index; } SongPatternPlayback;
/* PC: the song pointer is where sound_seq_start stores it (sequence_context.h) */
typedef struct SequencePlayerSongView { u8 unknown000[SEQ_PC_OFF(song10C)]; SongHeaderView *sequence; } SequencePlayerSongView;
SEQ_PC_SAME(SequencePlayerSongView, sequence, song10C);
typedef char pc_seq_18634_track[sizeof(SongTrackCursor) == sizeof(SequenceSlotTrack) ? 1 : -1];
typedef char pc_seq_18634_play[sizeof(SongPatternPlayback) == sizeof(SequenceTrack) ? 1 : -1];
#define HEADER (((SequencePlayerSongView *)sound_seq_current)->sequence)
#define TRACK(i) (((SongTrackCursor *)sound_seq_current->unknown128)[i])
#define PLAY(i) (((SongPatternPlayback *)sound_seq_current->tracks568)[i])
extern void sound_seq_set_channel_volume(u8, u8);
extern void sound_seq_rewind_tempo_track(unsigned int);
u8 sound_seq_play_tracks(void)
{
    int u1, u2, u3, u4; /* unused: frame residual, see header */
    int i;
    u8 first_loop;
    u8 active;
    SongTrackEvent *event;
    SongHeaderView *sequence;
    u32 *patterns;
    SongPattern *pattern;
    u32 sum;
    u32 index;
    first_loop = 1;
    active = 0;
    for (i = 0; i < 64; i++) {
        if (TRACK(i).cursor != 0) {
            active = 1;
            while (TRACK(i).time + sound_seq_current->half120 >= TRACK(i).cursor->time) {
                event = TRACK(i).cursor;
                switch (event->code) {
                case 0xFFFF:
                    TRACK(i).cursor = 0;
                    goto next_track;
                case 0xFFFE:
                    if (sound_seq_current->disabledFC5 != 0) {
                        TRACK(i).cursor = 0;
                        goto next_track;
                    }
                    index = event->value.jump;
                    TRACK(i).cursor = TRACK(i).start + index;
                    TRACK(i).time = HEADER->loop_time;
                    TRACK(i).fraction = 0;
                    if (first_loop) {
                        first_loop = 0;
                        sound_seq_rewind_tempo_track(HEADER->loop_time);
                        sound_seq_current->valueFC6++;
                    }
                    break;
                default:
                    sequence = HEADER;
                    patterns = (u32 *)(sequence->patterns + (u32)sequence);
                    index = event->code;
                    pattern = (SongPattern *)(patterns[index] + (u32)sequence);
                    PLAY(i).counter04 = 0;
                    PLAY(i).counter00 = 0;
                    PLAY(i).cursor = pattern->data;
                    PLAY(i).counter08 = 0;
                    if (pattern->optional04) {
                        PLAY(i).optional10 = (u8 *)(pattern->optional04 + (u32)HEADER);
                    } else {
                        PLAY(i).optional10 = 0;
                    }
                    PLAY(i).counter1C = 0;
                    PLAY(i).value18 = 0x2000;
                    if (pattern->optional08) {
                        PLAY(i).optional14 = (u8 *)(pattern->optional08 + (u32)HEADER);
                    } else {
                        PLAY(i).optional14 = 0;
                    }
                    PLAY(i).counter20 = 0;
                    PLAY(i).value1A = 0;
                    sequence = HEADER;
                    PLAY(i).channel = ((u8 *)(sequence->channels + (u32)sequence))[i];
                    PLAY(i).second = event->value.bytes.second;
                    PLAY(i).first = event->value.bytes.first;
                    PLAY(i).index = i;
                    if (event->program != 0xFF) {
                        sound_seq_set_program(sound_seq_current, event->program, PLAY(i).channel);
                    }
                    if (event->controller != 0xFF) {
                        sound_seq_set_channel_volume(event->controller, PLAY(i).channel);
                    }
                    TRACK(i).cursor++;
                    break;
                }
            }
            sum = TRACK(i).fraction + sound_seq_current->fraction118;
            TRACK(i).fraction = sum & 0xFFFF;
            sum >>= 16;
            TRACK(i).time = TRACK(i).time + sum + sound_seq_current->whole11C;
        }
next_track:
        ;
    }
    return active;
}

/* PROMOTED 2026-10-11 — sound_seq_update_tick_step
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_synth_frequency;
void sound_seq_update_tick_step(void)
{
    unsigned int value;
    value = ((sound_seq_current->rate124 * 3072U / 60U) *
        (unsigned int)((int)((unsigned int)sound_samples_per_tick << 16) / sound_synth_frequency)) >> 3;
    value = (sound_seq_current->valueFC2 * value) >> 8;
    sound_seq_current->fraction118 = value & 65535;
    sound_seq_current->whole11C = value >> 16;
    sound_seq_current->half120 = sound_seq_current->whole11C >> 1;
}

/* PROMOTED 2026-10-05 — sound_seq_play_tempo_track
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_play_tempo_track(void)
{
    if (sound_seq_current->timingStartF68) {
        while (sound_seq_current->currentF6C->time != 0xFFFFFFFFU) {
            if (sound_seq_current->highF74 + sound_seq_current->half120 <
                sound_seq_current->currentF6C->time) break;
            sound_synth_set_tempo(sound_seq_current->rate124 =
                sound_seq_current->currentF6C->value, sound_seq_current_index);
            sound_seq_current->currentF6C++;
        }
    }
}

/* PROMOTED 2026-10-11 — sound_seq_advance_tempo_clock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_advance_tempo_clock(void)
{
    unsigned int value;
    if (sound_seq_current->timingStartF68) {
        value = sound_seq_current->lowF70 + sound_seq_current->fraction118;
        sound_seq_current->lowF70 = value & 0xFFFF;
        value >>= 16;
        sound_seq_current->highF74 = sound_seq_current->highF74 + value + sound_seq_current->whole11C;
    }
}

/* PROMOTED 2026-10-05 — sound_seq_rewind_tempo_track
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_rewind_tempo_track(unsigned int value)
{
    if (sound_seq_current->timingStartF68) {
        sound_seq_current->currentF6C = sound_seq_current->timingStartF68;
        sound_seq_current->highF74 = value;
        sound_seq_current->lowF70 = 0;
        sound_seq_play_tempo_track();
        sound_seq_update_tick_step();
    }
}

/* PROMOTED 2026-10-05 — sound_seq_get_loop_count
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
unsigned short sound_seq_get_loop_count(unsigned int identifier)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF && (index & 0x80000000) == 0) return sound_seq_players[index].valueFC6;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_seq_is_playing
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_initialized;
extern u32 sound_seq_find_slot(u32);
u8 sound_seq_is_playing(u32 identifier)
{
    if (sound_initialized) return sound_seq_find_slot(identifier) != 0xFFFFFFFF;
    return 0;
}

/* PROMOTED 2026-10-05 — sound_seq_pause_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_pause_unlocked(unsigned int identifier)
{
    identifier = sound_seq_find_slot(identifier);
    if (identifier != 0xFFFFFFFFU) {
        if ((identifier & 0x80000000U) == 0) {
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].activeFC0 = 0;
                sound_seq_stop_player_notes(&sound_seq_players[identifier]);
                sound_seq_free_player_notes(&sound_seq_players[identifier]);
            }
        } else {
            identifier &= 0x7FFFFFFFU;
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].flagsFEE |= 8;
            }
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_pause
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_seq_pause_unlocked(unsigned int);
void sound_seq_pause(unsigned int identifier)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_pause_unlocked(identifier);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-05 — sound_seq_stop_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_stop_unlocked(unsigned int identifier)
{
    identifier = sound_seq_find_slot(identifier);
    if (identifier != 0xFFFFFFFFU) {
        if ((identifier & 0x80000000U) == 0) {
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].activeFC0 = 0;
                sound_seq_stop_player_notes(&sound_seq_players[identifier]);
                sound_seq_free_player_notes(&sound_seq_players[identifier]);
            }
            sound_seq_players[identifier].inactiveFC1 = 1;
        } else {
            identifier &= 0x7FFFFFFFU;
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].pendingFF0 = 0;
            }
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_stop
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_stop_unlocked(unsigned int);
void sound_seq_stop(unsigned int identifier)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_stop_unlocked(identifier);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-05 — sound_seq_stop_all
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_stop_all(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        sound_seq_stop_unlocked(sound_seq_players[i].identifier);
    }
}

/* PROMOTED 2026-10-05 — sound_seq_set_looping
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_seq_set_looping(unsigned int identifier, unsigned char enabled)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF && (index & 0x80000000) == 0) {
            sound_seq_players[index].disabledFC5 = !enabled;
            return 1;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-05 — sound_seq_set_speed_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_set_speed_unlocked(unsigned int identifier, unsigned short value)
{
    unsigned int index;
    index = sound_seq_find_slot(identifier);
    if (index != 0xFFFFFFFFU) {
        if ((index & 0x80000000U) == 0) {
            sound_seq_players[index].valueFC2 = value;
        } else {
            index &= 0x7FFFFFFFU;
            sound_seq_players[index].valueFEC = value;
            sound_seq_players[index].flagsFEE |= 0x20;
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_set_speed
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_set_speed_unlocked(unsigned int, unsigned short);
void sound_seq_set_speed(unsigned int identifier, unsigned short value)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_set_speed_unlocked(identifier, value);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-05 — sound_seq_resume_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_resume_unlocked(unsigned int identifier)
{
    unsigned int index;
    index = sound_seq_find_slot(identifier);
    if (index != 0xFFFFFFFFU) {
        if ((index & 0x80000000U) == 0) {
            sound_seq_players[index].activeFC0 = 1;
        } else {
            sound_seq_players[index & 0x7FFFFFFFU].flagsFEE &= ~8;
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_resume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_resume_unlocked(unsigned int);
void sound_seq_resume(unsigned int identifier)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_resume_unlocked(identifier);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_seq_set_track_mask_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u32 sound_seq_find_slot(u32);
void sound_seq_set_track_mask_unlocked(u32 identifier, u32 first, u32 second)
{
    u32 index;
    index = sound_seq_find_slot(identifier);
    if (index != 0xFFFFFFFFU) {
        if ((index & 0x80000000U) == 0) {
            sound_seq_players[index].first110 = first;
            sound_seq_players[index].second114 = second;
        } else {
            index &= 0x7FFFFFFFU;
            sound_seq_players[index].firstFE4 = first;
            sound_seq_players[index].secondFE8 = second;
            sound_seq_players[index].flagsFEE |= 0x10;
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_set_track_mask
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_set_track_mask_unlocked(unsigned int, unsigned int, unsigned int);
void sound_seq_set_track_mask(unsigned int identifier, unsigned int first, unsigned int second)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_set_track_mask_unlocked(identifier, first, second);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-05 — sound_seq_fade_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_fade_unlocked(unsigned char channel,unsigned short duration,unsigned int identifier,unsigned char flags)
{
    unsigned int original;
    int i;
    original=identifier;
    identifier=sound_seq_find_slot(identifier);
    if (identifier != 0xFFFFFFFFU) {
        if (!(identifier & 0x80000000U)) {
            sound_fade_volume_group_unlocked(channel,duration,sound_seq_players[identifier].channelFC4,flags,original);
            for (i=0;i<64;i++) {
                if (sound_seq_players[identifier].channels528[i] != sound_seq_players[identifier].channelFC4) {
                    sound_fade_volume_group_unlocked(channel,duration,sound_seq_players[identifier].channels528[i],0,0);
                }
            }
        } else {
            identifier &= 0x7FFFFFFFU;
            switch (flags & 15) {
            case 0: sound_seq_players[identifier].valueFE0=channel; break;
            case 1: sound_seq_players[identifier].pendingFF0=0; break;
            case 2:
                sound_seq_players[identifier].valueFE0=channel;
                sound_seq_players[identifier].flagsFEE |= 8;
                break;
            case 3:
                sound_seq_players[identifier].valueFE0=channel;
                sound_seq_players[identifier].flagsFEE |= 128;
                break;
            }
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_fade
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_fade_unlocked(unsigned char, unsigned short, unsigned int, unsigned char);
void sound_seq_fade(unsigned char value, unsigned short duration, unsigned int identifier, unsigned char mode)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_fade_unlocked(value, duration, identifier, mode);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-05 — sound_seq_get_volume_group
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_seq_get_volume_group(unsigned int identifier)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF) return (*(SequenceContext *)((uintptr_t)sound_seq_players +
                index * sizeof(SequenceContext))).channelFC4;
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_seq_set_studio_group
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_seq_studio_groups[][sizeof(SequenceContext)];
extern void sound_set_group_type(unsigned char, unsigned char);
void sound_seq_set_studio_group(unsigned int identifier, unsigned char selector, unsigned char channel)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF) {
            sound_seq_studio_groups[index][selector] = channel;
            sound_set_group_type(channel, 2);
        }
    }
}

/* PROMOTED 2026-10-11 — sound_seq_crossfade_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SequenceRequest { unsigned int identifier; unsigned short duration; unsigned char unknown006[2]; unsigned int nextIdentifier; unsigned short nextDuration; unsigned char unknown00E[2]; void *stream; unsigned short group, program; unsigned char channel; unsigned char unknown019[3]; unsigned int first, second; unsigned short value; unsigned char flags; unsigned char unknown027; } SequenceRequest;
/* PC: a slot keeps a copy from FC8 to its result pointer, and names some of its fields (sequence_context.h) */
typedef char pc_seq_request_size[sizeof(SequenceRequest) == SEQ_PC_SPAN(unknownFC8, pendingFF0) ? 1 : -1];
typedef char pc_seq_request_stream[__builtin_offsetof(SequenceRequest, stream) == SEQ_PC_SPAN(unknownFC8, streamFD8) ? 1 : -1];
typedef char pc_seq_request_channel[__builtin_offsetof(SequenceRequest, channel) == SEQ_PC_SPAN(unknownFC8, valueFE0) ? 1 : -1];
typedef char pc_seq_request_first[__builtin_offsetof(SequenceRequest, first) == SEQ_PC_SPAN(unknownFC8, firstFE4) ? 1 : -1];
typedef char pc_seq_request_flags[__builtin_offsetof(SequenceRequest, flags) == SEQ_PC_SPAN(unknownFC8, flagsFEE) ? 1 : -1];
typedef struct SequenceOptions { unsigned int flags, first, second; unsigned short value, duration; unsigned char channel; unsigned char unknown011; unsigned char mapCount; unsigned char unknown013; void *map; unsigned char channelCount; unsigned char unknown019[3]; void *channels; } SequenceOptions;
extern void sound_seq_resume(unsigned int);
extern void sound_seq_set_track_mask(unsigned int, unsigned int, unsigned int);
extern void sound_seq_set_speed(unsigned int, unsigned short);
extern void sound_seq_fade(unsigned char, unsigned short, unsigned int, unsigned char);
/* Pointers passed as integers: pointer-sized on the PC (groups.c). */
extern int sound_start_song_in_group(unsigned short, unsigned short, uintptr_t, uintptr_t, unsigned char);
extern int sound_start_song(unsigned short, unsigned short, uintptr_t, uintptr_t);
void sound_seq_crossfade_unlocked(void *request, unsigned int *result, unsigned char unlocked)
{
    SequenceOptions options;
    unsigned int index;
    SequenceRequest *state = request;
    index = sound_seq_find_slot(state->identifier);
    if (index != 0xFFFFFFFFU) {
        if (state->flags & 4) {
            *(SequenceRequest *)sound_seq_players[index].unknownFC8 = *state;
            sound_seq_players[index].unknownFF4[0] = 1;
            sound_seq_players[index].flagsFEE &= ~4;
            sound_seq_players[index].pendingFF0 = (uintptr_t)result;
            *result = state->identifier | 0x80000000U;
        } else {
            if (unlocked) {
                if (state->flags & 1) sound_seq_fade_unlocked(0,state->duration,state->identifier,2);
                else if (state->flags & 0x40) sound_seq_fade_unlocked(0,state->duration,state->identifier,3);
                else sound_seq_fade_unlocked(0,state->duration,state->identifier,1);
            } else {
                if (state->flags & 1) sound_seq_fade(0,state->duration,state->identifier,2);
                else if (state->flags & 0x40) sound_seq_fade(0,state->duration,state->identifier,3);
                else sound_seq_fade(0,state->duration,state->identifier,1);
            }
            if (result != 0) {
                if (state->flags & 2) {
                    if (sound_seq_find_slot(state->nextIdentifier) != 0xFFFFFFFFU) {
                        if (unlocked) {
                            sound_seq_resume_unlocked(state->nextIdentifier);
                            sound_seq_fade_unlocked(state->channel,state->nextDuration,state->nextIdentifier,0);
                            if (state->flags & 0x10) sound_seq_set_track_mask_unlocked(state->nextIdentifier,state->first,state->second);
                            if (state->flags & 0x20) sound_seq_set_speed_unlocked(state->nextIdentifier,state->value);
                        } else {
                            sound_seq_resume(state->nextIdentifier);
                            sound_seq_fade(state->channel,state->nextDuration,state->nextIdentifier,0);
                            if (state->flags & 0x10) sound_seq_set_track_mask(state->nextIdentifier,state->first,state->second);
                            if (state->flags & 0x20) sound_seq_set_speed(state->nextIdentifier,state->value);
                        }
                        *result = state->nextIdentifier;
                    } else *result = 0xFFFFFFFFU;
                } else {
                    /* Only fields enabled by flags/counts are consumed by
                     * the constructor; unused option fields stay untouched. */
                    options.flags = 4;
                    if (state->flags & 8) options.flags = 0x14;
                    if (state->flags & 0x20) { options.flags |= 2; options.value = state->value; }
                    if (state->flags & 0x10) { options.flags |= 1; options.first = state->first; options.second = state->second; }
                    options.duration = state->nextDuration;
                    options.channel = state->channel;
                    options.channelCount = 0;
                    if (unlocked) {
                        *result = sound_start_song_in_group(state->group,state->program,(uintptr_t)state->stream,(uintptr_t)&options,1);
                        if (*result != 0xFFFFFFFFU && (state->flags & 0x80)) sound_seq_set_track_mask_unlocked(*result,0,0);
                    } else {
                        *result = sound_start_song(state->group,state->program,(uintptr_t)state->stream,(uintptr_t)&options);
                        if (*result != 0xFFFFFFFFU && (state->flags & 0x80)) sound_seq_set_track_mask(*result,0,0);
                    }
                }
            }
        }
    }
}

/* PROMOTED 2026-10-04 — sound_seq_crossfade
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_crossfade_unlocked(void *, unsigned int *, unsigned char);
void sound_seq_crossfade(void *state, unsigned int *result)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_crossfade_unlocked(state, result, 0);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-11 — sound_seq_crossfade_done
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_seq_crossfade_done(unsigned int *state) { if (*state != 0xFFFFFFFF) return (*state & 0x80000000) == 0; return 1; }

/* PROMOTED 2026-10-05 — sound_seq_tick
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_seq_tick(void)
{
    int i;
    unsigned char first, second, third;
    if (sound_samples_per_tick) {
        for (i = 0; i < 8; i++) {
            if (sound_seq_players[i].activeFC0) {
                sound_seq_current = &sound_seq_players[i];
                sound_seq_current_index = i;
                sound_seq_fading_out = sound_is_group_fading_out(sound_seq_players[i].channelFC4);
                sound_seq_play_tempo_track();
                sound_seq_update_tick_step();
                sound_seq_advance_tempo_clock();
                first = sound_seq_play_tracks();
                second = sound_seq_play_note_events();
                sound_seq_play_pitch_bend_streams();
                sound_seq_play_modulation_streams();
                sound_seq_advance_pattern_clocks();
                third = sound_seq_end_due_notes();
                sound_seq_reap_released_notes();
                if (!first && !second && !third) {
                    sound_seq_players[i].activeFC0 = 0;
                    sound_seq_players[i].inactiveFC1 = 1;
                }
            }
        }
    }
}

/* PROMOTED 2026-10-05 — sound_seq_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_seq_init_notes(void);
extern void sound_seq_reset_ids(void);
void sound_seq_init(void)
{
    unsigned int index;
    for (index = 0; index < 8; index++) {
        sound_seq_players[index].activeFC0 = 0;
        sound_seq_players[index].inactiveFC1 = 1;
    }
    sound_seq_init_notes();
    sound_seq_reset_ids();
}

