/* src/rom/sound/sequencer.c: the MusyX sound library's song sequencer.
 *
 * Eight song players ("slots", sound_seq_players, include/sequence_context.h). sound_seq_start sets a slot up for a
 * song (from sound_start_song_in_group, groups.c): its program pages, MIDI setup, tempo, 64 tracks and options.
 * Each library tick sound_seq_tick runs every playing slot: the tempo track, the tracks (each a list of pattern
 * references, with a loop point), the patterns' note events (notes start through the synth, sound_synth_start_note),
 * their pitch-bend and modulation streams (as MIDI controllers), and the notes' ends. Songs are paused, resumed,
 * stopped, faded, sped up, have tracks muted, and crossfade into another song, at once or at a marker in the song
 * (controller 104), through a copy of the request kept in the slot. Times are song ticks (384 a beat) in 16.16.
 *
 * MusyX library code with no MAT3 counterpart. The game plays its music through it (AudioSongPlay starts songs,
 * AudioSongStop stops them). A song (waves 10-21) holds offsets from its start to its tables, tracks, patterns and
 * streams; the sequencer adds them to the song's pointer (docs/port/MUSYX_DATA.md 3.4). */
#include "../rom_tu.h"
#include "sequence_context.h"

#define SEQ_PLAYERS 8               /* song slots */
#define SEQ_TRACKS 64               /* tracks a song has */
#define SEQ_CHANNELS 16             /* MIDI channels a slot plays on */
#define SEQ_NOTE_POOL 256           /* note records shared by the slots */
#define SEQ_DRUM_CHANNEL 9          /* MIDI channel 10: programs from the drum page */
#define SEQ_NO_PROGRAM 255          /* a program map entry with no page entry */
#define SEQ_SPEED_NORMAL 256        /* SequenceContext.speed for 1.0 */
#define SEQ_VOLUME_GROUP_BASE 23    /* a slot's volume group is this plus its number */
/* A song handle with this bit stands for the song a deferred crossfade request will start: the commands then
 * change the request's copy in the slot (find_slot keeps the bit). */
#define SEQ_REQUEST_BIT 0x80000000U
#define SEQ_ID_MASK 0x7FFFFFFFU

/* The event codes of a track's pattern list (SongTrackEvent.code); others are a pattern number. */
#define SEQ_TRACK_END 0xFFFF
#define SEQ_TRACK_LOOP 0xFFFE

/* MIDI controllers the sequencer sets, and the library's own numbers past MIDI's (sound_midi_set_controller) */
#define MIDI_CC_MODULATION 1
#define MIDI_CC_VOLUME 7
#define MIDI_CC_PAN 10
#define MIDI_CC_MODULATION_LSB 33
#define MIDI_CC_REVERB 91
#define MIDI_CC_CHORUS 93
#define SEQ_CC_CROSSFADE_MARKER 104 /* in a pattern: carry out the slot's deferred crossfade request now */
#define SOUND_CC_PITCH_BEND 128     /* pitch bend, high 7 bits */
#define SOUND_CC_PITCH_BEND_LSB 129 /* and low 7 bits */
#define SOUND_CC_CHANNEL_PRESSURE 130

/* What a volume fade does when it ends (sound_fade_volume_group_unlocked's mode, synth.c) */
#define SEQ_FADE_NONE 0
#define SEQ_FADE_STOP 1
#define SEQ_FADE_PAUSE 2
#define SEQ_FADE_MUTE 3             /* every track off (track mask 0) */

/* SequencePlayParams.flags: which options sound_seq_start takes */
#define SEQ_PLAY_TRACK_MASK 0x1
#define SEQ_PLAY_SPEED 0x2
#define SEQ_PLAY_FADE 0x4           /* fade the song's volume groups to fade_volume over fade_time */
#define SEQ_PLAY_TRACK_GROUPS 0x8   /* give tracks their own volume groups */
#define SEQ_PLAY_PAUSED 0x10        /* set up, but do not play yet */

/* SequenceRequest.flags (and the slot's copy, flagsFEE): a crossfade from one song to the next */
#define SEQ_REQUEST_PAUSE_OLD 0x01  /* fade the old song out to a pause, not a stop */
#define SEQ_REQUEST_RESUME_NEW 0x02 /* the next song exists (paused): resume it instead of starting one */
#define SEQ_REQUEST_AT_MARKER 0x04  /* wait for the old song's crossfade marker */
#define SEQ_REQUEST_NEW_PAUSED 0x08 /* start the next song paused */
#define SEQ_REQUEST_TRACK_MASK 0x10 /* give the next song track_mask */
#define SEQ_REQUEST_SPEED 0x20      /* give the next song speed */
#define SEQ_REQUEST_MUTE_OLD 0x40   /* fade the old song out to silence (tracks off), not a stop */
#define SEQ_REQUEST_NEW_MUTED 0x80  /* start the next song with every track off */

/* An entry of a program page (normal or drum): the program's sound macro, voice priority and limit, the MIDI
 * channel it is set on when the song starts (255 none), and its program number (255 ends the page). */
#pragma pack(1)
typedef struct SequencePageEntry { u16 macro; u8 priority, max_voices, channel, program, reserved[2]; } SequencePageEntry;
#pragma pack()
/* A song group's MIDI setup: each channel's starting program, volume, pan, reverb and chorus. */
typedef struct MidiSetupChannel { u8 program, volume, panning, reverb, chorus, reserved[3]; } MidiSetupChannel;
typedef struct MidiSetup { u16 songId, reserved; MidiSetupChannel channel[SEQ_CHANNELS]; } MidiSetup;
typedef struct SequenceTrackGroup { u8 track, group; } SequenceTrackGroup;
/* sound_seq_start's options (SEQ_PLAY_* flags say which are given). */
typedef struct SequencePlayParams {
    u32 flags;
    u32 track_mask[2];
    u16 speed;
    u16 fade_time;                  /* milliseconds */
    u8 fade_volume;
    u8 pad11;
    u8 group_count;                 /* entries of track_groups */
    SequenceTrackGroup *track_groups;
    u8 faded_count;                 /* entries of faded */
    u8 *faded;                      /* more volume groups to fade with the song's */
} SequencePlayParams;
/* A song: offsets from its start to the track table (SEQ_TRACKS words, 0 for an empty track), the pattern table,
 * the track channels (a byte per track) and the tempo track (0 for none); the starting tempo; the loop's time. */
typedef struct SongHeader { u32 track_table, pattern_table, channel_table, tempo_track, tempo, loop_time; } SongHeader;
/* An event of a track's pattern list: at `time`, play pattern `code` (with a program change and a volume unless
 * 255, a transpose and a velocity offset), or loop back to entry `value.jump`, or end. */
typedef struct SongTrackEvent { u32 time; u8 program; u8 controller; u16 unknown06; u16 code; union { u16 jump; struct { s8 transpose; s8 velocity_offset; } bytes; } value; } SongTrackEvent;
/* A pattern: offsets (from the song) of its pitch-bend and modulation streams (0 for none), then its note events. */
typedef struct SongPattern { u32 unknown00; u32 pitch_bend; u32 modulation; u8 data[1]; } SongPattern;
/* The slot's tracks (SequenceSlotTrack) with typed event pointers. */
typedef struct SongTrackCursor { SongTrackEvent *start; SongTrackEvent *cursor; u32 fraction; u32 time; } SongTrackCursor;
typedef char pc_seq_18634_track[sizeof(SongTrackCursor) == sizeof(SequenceSlotTrack) ? 1 : -1];
typedef struct SequenceTime { unsigned int fraction, whole; } SequenceTime;

/* sound_seq_set_program's view of a slot. PC: the slot's native layout (sequence_context.h) */
typedef struct SequenceProgramView { u32 identifier; SequencePageEntry *normtab; u8 normTrans[128]; SequencePageEntry *drumtab; u8 drumTrans[128]; u8 unknown10C[SEQ_PC_SPAN(song10C, unknownF80)]; u32 prgState[SEQ_CHANNELS]; } SequenceProgramView;
SEQ_PC_SAME(SequenceProgramView, normtab, normtab004); SEQ_PC_SAME(SequenceProgramView, drumtab, drumtab088);
SEQ_PC_SAME(SequenceProgramView, drumTrans, drumTrans08C); SEQ_PC_SAME(SequenceProgramView, prgState, unknownF80);

/* sound_seq_start's view of a slot, with the header's fields typed. PC: the request and its result pointer from
 * FC8 to FF4 are 0x38 bytes (sequence_context.h) */
typedef struct SequencePlayer {
    u32 identifier;
    SequencePageEntry *normal_page;
    u8 normal_programs[128];
    SequencePageEntry *drum_page;
    u8 drum_programs[128];
    SongHeader *song;
    u32 track_mask[2];
    u32 step_fraction, step_whole, lookahead;
    u32 tempo;
    SequenceSlotTrack track[SEQ_TRACKS];
    u8 track_groups[SEQ_TRACKS];
    SequenceTrack patterns[SEQ_TRACKS];
    u32 *tempo_track, *tempo_cursor;
    u32 tempo_fraction, tempo_time;
    SequenceNode *notes, *released_notes;
    u32 programs[SEQ_CHANNELS];
    u8 playing, free;
    u16 speed;
    u8 volume_group, loop_disabled;
    u16 loop_count;
    u8 request[SEQ_PC_SPAN(unknownFC8, unknownFF4)];
    u8 request_waiting;
    u8 unknownFF5[3];
} SequencePlayer;
SEQ_PC_SAME(SequencePlayer, normal_page, normtab004); SEQ_PC_SAME(SequencePlayer, drum_programs, drumTrans08C);
SEQ_PC_SAME(SequencePlayer, song, song10C); SEQ_PC_SAME(SequencePlayer, track_mask, track_mask);
SEQ_PC_SAME(SequencePlayer, tempo, rate124); SEQ_PC_SAME(SequencePlayer, track, unknown128);
SEQ_PC_SAME(SequencePlayer, track_groups, track_groups); SEQ_PC_SAME(SequencePlayer, patterns, patterns);
SEQ_PC_SAME(SequencePlayer, tempo_track, timingStartF68); SEQ_PC_SAME(SequencePlayer, notes, notes);
SEQ_PC_SAME(SequencePlayer, released_notes, released_notes); SEQ_PC_SAME(SequencePlayer, programs, unknownF80);
SEQ_PC_SAME(SequencePlayer, free, inactiveFC1); SEQ_PC_SAME(SequencePlayer, speed, speed);
SEQ_PC_SAME(SequencePlayer, volume_group, channelFC4); SEQ_PC_SAME(SequencePlayer, loop_count, loop_count);
SEQ_PC_SAME(SequencePlayer, request_waiting, unknownFF4);
typedef char pc_seq_player_size_check[sizeof(SequencePlayer) == sizeof(SequenceContext) ? 1 : -1];

/* sound_seq_play_note_events's view of a slot. PC: the slot's native layout (sequence_context.h) */
typedef struct SequenceContextEvents { unsigned char unknown000[SEQ_PC_OFF(track_mask)]; unsigned int track_mask[2]; unsigned char unknown118[8]; unsigned int lookahead; unsigned char unknown124[SEQ_PC_SPAN(rate124, track_groups)]; unsigned char track_groups[SEQ_TRACKS]; SequenceTrack tracks[SEQ_TRACKS]; unsigned char unknownF68[SEQ_PC_SPAN(timingStartF68, unknownF80)]; unsigned int programs[SEQ_CHANNELS]; unsigned char unknownFC0[8]; unsigned char request[SEQ_PC_SPAN(unknownFC8, pendingFF0)]; unsigned int *result; unsigned char pending; unsigned char unknownFF5[3]; } SequenceContextEvents;
SEQ_PC_SAME(SequenceContextEvents, track_mask, track_mask); SEQ_PC_SAME(SequenceContextEvents, lookahead, lookahead);
SEQ_PC_SAME(SequenceContextEvents, track_groups, track_groups); SEQ_PC_SAME(SequenceContextEvents, tracks, patterns);
SEQ_PC_SAME(SequenceContextEvents, programs, unknownF80); SEQ_PC_SAME(SequenceContextEvents, request, unknownFC8);
SEQ_PC_SAME(SequenceContextEvents, result, pendingFF0); SEQ_PC_SAME(SequenceContextEvents, pending, unknownFF4);

/* A crossfade request (sound_seq_crossfade): fade `song` out over fade_out_time ms and start (or resume)
 * the next song, fading in to next_volume over fade_in_time ms, with the options its flags give. */
typedef struct SequenceRequest {
    unsigned int song;
    unsigned short fade_out_time;
    unsigned char unknown006[2];
    unsigned int next_song;         /* SEQ_REQUEST_RESUME_NEW: the paused song to resume */
    unsigned short fade_in_time;
    unsigned char unknown00E[2];
    void *next_data;                /* the next song's data (sound_start_song_in_group's) */
    unsigned short group, song_number; /* its song group and number in the group */
    unsigned char next_volume;
    unsigned char unknown019[3];
    unsigned int track_mask[2];
    unsigned short speed;
    unsigned char flags;            /* SEQ_REQUEST_* */
    unsigned char unknown027;
} SequenceRequest;
/* PC: a slot keeps a copy from FC8 to its result pointer, and names some of its fields (sequence_context.h) */
typedef char pc_seq_request_size[sizeof(SequenceRequest) == SEQ_PC_SPAN(unknownFC8, pendingFF0) ? 1 : -1];
typedef char pc_seq_request_stream[__builtin_offsetof(SequenceRequest, next_data) == SEQ_PC_SPAN(unknownFC8, streamFD8) ? 1 : -1];
typedef char pc_seq_request_channel[__builtin_offsetof(SequenceRequest, next_volume) == SEQ_PC_SPAN(unknownFC8, valueFE0) ? 1 : -1];
typedef char pc_seq_request_first[__builtin_offsetof(SequenceRequest, track_mask) == SEQ_PC_SPAN(unknownFC8, firstFE4) ? 1 : -1];
typedef char pc_seq_request_flags[__builtin_offsetof(SequenceRequest, flags) == SEQ_PC_SPAN(unknownFC8, flagsFEE) ? 1 : -1];

/* PC: the low byte of the word sound_seq_current_index (the current sequence slot, sound_seq_tick), which the N64 reads as the
 * word's byte 3. One name, as an optimizing GCC takes two external variables for two objects and sound_seq_tick
 * stores the word, then calls the sequencer, which reads this byte and may be inlined into it
 * (tools/port/alias_check.py); and the low byte, as the PC holds the word little-endian, where byte 3 is its high
 * byte, 0, which sent these controller messages of every sequence to slot 0 (docs/port/OPTIMIZED.md section 8). */
#define sound_seq_current_index_byte ((u8)sound_seq_current_index)

/* The library's data (more in sequence_context.h) */
extern SequenceNode sound_seq_note_pool[SEQ_NOTE_POOL];
extern unsigned int sound_seq_next_id;
extern u8 sound_track_volumes[SEQ_TRACKS];
extern int sound_synth_frequency;
extern unsigned char sound_initialized;
/* the data loader's label for slot 0's track_groups (MUSYX_DATA.md 8.2), indexed by slot */

/* The synth, the volume groups and this file's functions */
extern int sound_stop_unlocked(u32);
extern void sound_midi_set_controller(u8 controller, u8 channel, u8 set, u8 value);
extern void sound_seq_build_program_map(u8 *map, SequencePageEntry *record);
extern void sound_seq_set_program(SequenceContext *, unsigned char, unsigned char);
extern void sound_set_group_type(unsigned char, unsigned char);
extern void sound_midi_reset_channel(u8, u8);
extern unsigned int sound_seq_assign_id(unsigned int slot);
extern SequenceNode *sound_seq_alloc_note(void);
extern void sound_seq_free_note(SequenceNode *);
extern void sound_seq_set_channel_pressure(unsigned char, unsigned char);
extern void sound_seq_crossfade_unlocked(void *, unsigned int *, unsigned char);
extern unsigned int sound_synth_start_note(unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned short, unsigned short, unsigned char, short);
extern int sound_key_off_unlocked(unsigned int);
extern void sound_seq_set_channel_volume(u8, u8);
extern void sound_seq_rewind_tempo_track(unsigned int);
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_seq_set_speed_unlocked(unsigned int, unsigned short);
extern void sound_seq_set_track_mask_unlocked(unsigned int, unsigned int, unsigned int);
extern void sound_seq_resume(unsigned int);
extern void sound_seq_set_track_mask(unsigned int, unsigned int, unsigned int);
extern void sound_seq_set_speed(unsigned int, unsigned short);
extern void sound_seq_fade(unsigned char, unsigned short, unsigned int, unsigned char);
extern int sound_start_song_in_group(unsigned short, unsigned short, SongHeader *, SequencePlayParams *, unsigned char);
extern int sound_start_song(unsigned short, unsigned short, SongHeader *, SequencePlayParams *);
extern void sound_seq_init_notes(void);
extern void sound_seq_reset_ids(void);

/* Links the 256 note records into the free list. */
void sound_seq_init_notes(void)
{
    SequenceNode *previous;
    int i;
    previous = 0;
    sound_seq_free_notes = sound_seq_note_pool;
    for (i = 0; i < SEQ_NOTE_POOL; i++) {
        sound_seq_note_pool[i].previous = previous;
        if (previous != 0) {
            previous->next = &sound_seq_note_pool[i];
        }
        previous = &sound_seq_note_pool[i];
    }
    previous->next = 0;
}

/* Gives a slot's playing and released notes back to the free list (their voices are not touched). */
void sound_seq_free_player_notes(SequenceContext *state)
{
    SequenceNode *node;
    node = state->notes;
    if (node != 0) {
        while (node->next != 0) {
            node = node->next;
        }
        if (sound_seq_free_notes != 0) {
            node->next = sound_seq_free_notes;
            sound_seq_free_notes->previous = node;
        }
        sound_seq_free_notes = state->notes;
        state->notes = 0;
    }
    node = state->released_notes;
    if (node != 0) {
        while (node->next != 0) {
            node = node->next;
        }
        if (sound_seq_free_notes != 0) {
            node->next = sound_seq_free_notes;
            sound_seq_free_notes->previous = node;
        }
        sound_seq_free_notes = state->released_notes;
        state->released_notes = 0;
    }
}

/* Stops the voices of a slot's playing and released notes. */
void sound_seq_stop_player_notes(SequenceContext *context)
{
    SequenceNode *entry;
    entry = context->notes;
    while (entry != 0) {
        sound_stop_unlocked(entry->identifier);
        entry = entry->next;
    }
    entry = context->released_notes;
    while (entry != 0) {
        sound_stop_unlocked(entry->identifier);
        entry = entry->next;
    }
}

/* Takes a note record from the free list onto the current slot's playing notes. Returns it, or null. */
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
        head = sound_seq_current->notes;
        node->next = head;
        if (head != 0) {
            sound_seq_current->notes->previous = node;
        }
        sound_seq_current->notes = node;
    }
    return node;
}

/* Moves a note from the current slot's playing notes back to the free list (its voice did not start). */
void sound_seq_free_note(SequenceNode *node)
{
    if (node->next != 0) {
        node->next->previous = node->previous;
    }
    if (node->previous != 0) {
        node->previous->next = node->next;
    } else {
        sound_seq_current->notes = node->next;
    }
    node->next = sound_seq_free_notes;
    if (node->next != 0) {
        sound_seq_free_notes->previous = node;
    }
    node->previous = 0;
    sound_seq_free_notes = node;
}

/* Moves a note from the current slot's released notes back to the free list (its voice has ended). */
void sound_seq_free_released_note(SequenceNode *node)
{
    if (node->next != 0) {
        node->next->previous = node->previous;
    }
    if (node->previous != 0) {
        node->previous->next = node->next;
    } else {
        sound_seq_current->released_notes = node->next;
    }
    node->next = sound_seq_free_notes;
    if (node->next != 0) {
        sound_seq_free_notes->previous = node;
    }
    node->previous = 0;
    sound_seq_free_notes = node;
}

/* Moves a note keyed off from the current slot's playing notes to its released notes, where it stays until its
 * voice ends. */
void sound_seq_release_note(SequenceNode *node)
{
    SequenceNode *head;
    if (node->next != 0) {
        node->next->previous = node->previous;
    }
    if (node->previous != 0) {
        node->previous->next = node->next;
    } else {
        sound_seq_current->notes = node->next;
    }
    head = sound_seq_current->released_notes;
    node->next = head;
    if (head != 0) {
        sound_seq_current->released_notes->previous = node;
    }
    node->previous = 0;
    sound_seq_current->released_notes = node;
}

/* Frees the current slot's released notes whose voices have ended. */
void sound_seq_reap_released_notes(void)
{
    SequenceNode *entry;
    SequenceNode *next;
    entry = sound_seq_current->released_notes;
    while (entry != 0) {
        next = entry->next;
        if (sound_check(entry->identifier) == 0xFFFFFFFFU) sound_seq_free_released_note(entry);
        entry = next;
    }
}

/* Restarts the song handles from 0. */
void sound_seq_reset_ids(void)
{
    sound_seq_next_id = 0;
}

/* Gives slot `slot` the next 31-bit song handle no busy slot has, and returns it. */
unsigned int sound_seq_assign_id(unsigned int slot)
{
    unsigned int identifier;
    int i;
    do {
        identifier = sound_seq_next_id++;
        sound_seq_next_id &= SEQ_ID_MASK;
        for (i = 0; i < SEQ_PLAYERS; i++) {
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

/* Finds the busy slot playing song handle `identifier`. Returns its number, with the handle's SEQ_REQUEST_BIT
 * kept (then the command is for the slot's deferred request), or 0xFFFFFFFF. */
u32 sound_seq_find_slot(u32 identifier)
{
    int i;
    for (i = 0; i < SEQ_PLAYERS; ++i) {
        if (sound_seq_players[i].inactiveFC1 == 0 &&
            sound_seq_players[i].identifier == (identifier & SEQ_ID_MASK)) {
            return (identifier & SEQ_REQUEST_BIT) | i;
        }
    }
    return 0xFFFFFFFFU;
}

#define SEQ_PLAYER ((SequenceProgramView *)context)
/* A program change on a slot's MIDI channel: looks the program up in the normal page (the drum page on channel 10)
 * and sets the channel's macro, priority and voice limit; a program the page lacks changes nothing. */
void sound_seq_set_program(SequenceContext *context, unsigned char program, unsigned char channel)
{
    if (channel != SEQ_DRUM_CHANNEL) {
        program = SEQ_PLAYER->normTrans[program];
        if (program != SEQ_NO_PROGRAM) {
            SEQ_PLAYER->prgState[channel] = ((u32)SEQ_PLAYER->normtab[program].macro << 16) |
                ((u32)SEQ_PLAYER->normtab[program].priority << 8) | SEQ_PLAYER->normtab[program].max_voices;
        }
    } else {
        program = SEQ_PLAYER->drumTrans[program];
        if (program != SEQ_NO_PROGRAM) {
            SEQ_PLAYER->prgState[channel] = ((u32)SEQ_PLAYER->drumtab[program].macro << 16) |
                ((u32)SEQ_PLAYER->drumtab[program].priority << 8) | SEQ_PLAYER->drumtab[program].max_voices;
        }
    }
}
#undef SEQ_PLAYER

/* A pattern's channel pressure event, for the current slot's channel. */
void sound_seq_set_channel_pressure(u8 value, u8 channel)
{
    sound_midi_set_controller(SOUND_CC_CHANNEL_PRESSURE, channel, sound_seq_current_index_byte, value);
}

/* A track's volume (its pattern-list event's), for the current slot's channel. */
void sound_seq_set_channel_volume(u8 value, u8 channel)
{
    sound_midi_set_controller(MIDI_CC_VOLUME, channel, sound_seq_current_index_byte, value);
}

/* Builds a program map from a program page: map[program] is the program's entry in the page, 255 for programs the
 * page lacks. The page ends at an entry whose program is 255. */
void sound_seq_build_program_map(u8 *map, SequencePageEntry *record)
{
    u8 index;
    for (index = 0; index < 128; index++) map[index] = SEQ_NO_PROGRAM;
    index = 0;
    while (record->program != SEQ_NO_PROGRAM) {
        map[record->program] = index;
        index++;
        record++;
    }
}

/* Starts a song on a free slot: the normal and drum program pages (with their maps), the MIDI setup's programs and
 * controllers per channel (if any), the song's tempo and tempo track, its 64 tracks at their starts, and the
 * options (para may be null: all tracks, normal speed): track mask, speed, per-track volume groups, a fade of the
 * song's groups, starting paused. Each slot has its own volume group (SEQ_VOLUME_GROUP_BASE + its number). Returns
 * the song's handle, or 0xFFFFFFFF when every slot is busy. */
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

    for (i = 0; i < SEQ_PLAYERS; i++) {
        if (sound_seq_players[i].inactiveFC1 != 0) break;
    }
    if (i != SEQ_PLAYERS) {
    nseq = (SequencePlayer *)&sound_seq_players[i];
    nseq->request_waiting = 0;
    nseq->normal_page = norm;
    nseq->drum_page = drum;
    nseq->song = song;
    sound_seq_build_program_map(nseq->normal_programs, nseq->normal_page);
    sound_seq_build_program_map(nseq->drum_programs, nseq->drum_page);
    nseq->volume_group = i + SEQ_VOLUME_GROUP_BASE;
    for (j = 0; j < SEQ_TRACKS; j++) {
        nseq->track_groups[j] = nseq->volume_group;
    }
    if (para == NULL) {
        nseq->track_mask[0] = 0xFFFFFFFF;
        nseq->track_mask[1] = 0xFFFFFFFF;
        nseq->speed = SEQ_SPEED_NORMAL;
    } else {
        if (para->flags & SEQ_PLAY_TRACK_MASK) {
            nseq->track_mask[0] = para->track_mask[0];
            nseq->track_mask[1] = para->track_mask[1];
        } else {
            nseq->track_mask[0] = 0xFFFFFFFF;
            nseq->track_mask[1] = 0xFFFFFFFF;
        }
        if (para->flags & SEQ_PLAY_SPEED) {
            nseq->speed = para->speed;
        } else {
            nseq->speed = SEQ_SPEED_NORMAL;
        }
        if (para->flags & SEQ_PLAY_TRACK_GROUPS) {
            for (j = 0; j < para->group_count; j++) {
                nseq->track_groups[para->track_groups[j].track] = para->track_groups[j].group;
                sound_set_group_type(para->track_groups[j].group, 0);
            }
        }
        if (para->flags & SEQ_PLAY_FADE) {
            sound_fade_volume_group_unlocked(para->fade_volume, para->fade_time, nseq->volume_group, SEQ_FADE_NONE, 0);
            for (j = 0; j < para->faded_count; j++) {
                sound_fade_volume_group_unlocked(para->fade_volume, para->fade_time, para->faded[j], SEQ_FADE_NONE, 0);
            }
        }
    }
    arr = song;
    seqId = i;
    nseq->tempo = arr->tempo;
    sound_synth_set_tempo(nseq->tempo, seqId);
    if (arr->tempo_track != 0) {
        nseq->tempo_track = (u32 *)((u8 *)arr + arr->tempo_track);
        nseq->tempo_cursor = nseq->tempo_track;
        nseq->tempo_fraction = 0;
        nseq->tempo_time = 0;
    } else {
        nseq->tempo_track = NULL;
    }
    nseq->loop_disabled = 0;
    tracktab = (u32 *)((u8 *)arr + arr->track_table);
    for (j = 0; j < SEQ_TRACKS; j++) {
        sound_track_volumes[j] = 127;
        nseq->track[j].fraction = 0;
        nseq->track[j].time = 0;
        nseq->patterns[j].events = 0;
        nseq->patterns[j].pitch_bend = 0;
        nseq->patterns[j].modulation = 0;
        if (tracktab[j] != 0) {
            nseq->track[j].cursor = nseq->track[j].start = (u8 *)arr + tracktab[j];
        } else {
            nseq->track[j].cursor = nseq->track[j].start = NULL;
        }
    }
    nseq->notes = NULL;
    nseq->released_notes = NULL;
    for (j = 0; j < SEQ_CHANNELS; j++) {
        sound_midi_reset_channel(j, seqId);
    }
    for (j = 0; j < SEQ_CHANNELS; j++) {
        nseq->programs[j] = 0xFFFFFFFF;
    }
    page = nseq->normal_page;
    trans = nseq->normal_programs;
    for (t = 0; t < 2; t++) {
        for (j = 0; j < 128; j++) {
            if (trans[j] != 0xFF) {
                entry = page;
                entry += trans[j];
                if (entry->channel != 0xFF) {
                    nseq->programs[entry->channel] = entry->max_voices | (entry->macro << 16) | (entry->priority << 8);
                }
            }
        }
        page = nseq->drum_page;
        trans = nseq->drum_programs;
    }
    if (midiSetup != NULL) {
        for (j = 0; j < SEQ_CHANNELS; j++) {
            sound_seq_set_program((SequenceContext *)nseq, midiSetup->channel[j].program, j);
            sound_midi_set_controller(MIDI_CC_VOLUME, j, seqId, midiSetup->channel[j].volume);
            sound_midi_set_controller(MIDI_CC_PAN, j, seqId, midiSetup->channel[j].panning);
            sound_midi_set_controller(MIDI_CC_REVERB, j, seqId, midiSetup->channel[j].reverb);
            sound_midi_set_controller(MIDI_CC_CHORUS, j, seqId, midiSetup->channel[j].chorus);
        }
    }
    nseq->loop_count = 0;
    if (para != NULL) {
        if (!(para->flags & SEQ_PLAY_PAUSED)) {
            nseq->playing = 1;
        }
    } else {
        nseq->playing = 1;
    }
    i = sound_seq_assign_id(i);
    nseq->free = 0;
    return i;
    }
    return 0xFFFFFFFF;
}

#define CTX ((SequenceContextEvents *)sound_seq_current)
/* Plays the current slot's due pattern events (those up to now plus the lookahead), track by track. A note event
 * (6 bytes: time delta, key, velocity, duration) on an enabled track starts a note with the channel's program,
 * the track's transpose and velocity offset (clamped to 0..127), and keeps it on the playing notes until its end
 * time. 4-byte events with the key's top bit: a program change (velocity 0), channel pressure (velocity 1), or a
 * controller (velocity's top bit: its number in the velocity, the value in the key), where the crossfade marker
 * carries out the slot's deferred request. Key and velocity 255 end the pattern. Returns 1 if any track still
 * plays a pattern. */
unsigned char sound_seq_play_note_events(void)
{
    int unused0, unused1, unused2, unused3; /* unused: the N64 build's frame layout */
    unsigned char channel,active;
    int unused4;                            /* unused: the N64 build's frame layout */
    int i,advance,key,velocity,number;
    unsigned int due,program,identifier;
    SequenceNode *note;
    active=0;
    for (i=0;i<SEQ_TRACKS;i++) {
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
                    if ((velocity&127)==SEQ_CC_CROSSFADE_MARKER) {
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
                    if (CTX->track_mask[number/32] & (1U<<(number&31))) {
                        program=CTX->programs[channel];
                        if (program!=0xFFFFFFFFU) {
                            key+=CTX->tracks[i].transpose;
                            key=key>127 ? 127 : key<0 ? 0 : key;
                            velocity+=CTX->tracks[i].velocity_offset;
                            velocity=velocity>127 ? 127 : velocity<0 ? 0 : velocity;
                            note=sound_seq_alloc_note();
                            if (note!=0) {
                                identifier=sound_synth_start_note(program,key,velocity,64,channel,
                                    sound_seq_current_index,0,number & 255,CTX->track_groups[number],
                                    sound_seq_fading_out ? -1 : 0);
                                if (identifier!=0xFFFFFFFFU) {
                                    note->identifier=identifier;
                                    note->end_time=*(unsigned short *)(CTX->tracks[i].events+4)+due;
                                    *(SequenceTime *)&note->clock_fraction=*(SequenceTime *)&CTX->tracks[i].fraction;
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
#undef CTX

/* Advances the clocks of the current slot's patterns that still play something by one step. */
void sound_seq_advance_pattern_clocks(void)
{
    int i;
    int sum;
    for (i = 0; i < SEQ_TRACKS; i++) {
        if (sound_seq_current->patterns[i].events || sound_seq_current->patterns[i].pitch_bend ||
            sound_seq_current->patterns[i].modulation) {
            sum = (int)(sound_seq_current->patterns[i].fraction + sound_seq_current->step_fraction);
            sound_seq_current->patterns[i].fraction = (unsigned int)sum & 65535;
            sum >>= 16;
            sound_seq_current->patterns[i].whole = (int)((unsigned int)sound_seq_current->patterns[i].whole +
                (unsigned int)sum + (unsigned int)sound_seq_current->step_whole);
        }
    }
}

/* Keys off the current slot's notes whose end time has come (now plus the lookahead), moving them to the released
 * notes, and advances the others' clocks by one step. Returns 0 if the slot has no playing notes, else 1. */
int sound_seq_end_due_notes(void)
{
    SequenceNode *node;
    SequenceNode *next;
    int sum;
    if (sound_seq_current->notes == 0) return 0;
    node = sound_seq_current->notes;
    while (node != 0) {
        next = node->next;
        sum = (int)((unsigned int)node->clock_time + (unsigned int)sound_seq_current->lookahead);
        if (sum >= node->end_time) {
            sound_key_off_unlocked(node->identifier);
            sound_seq_release_note(node);
        } else {
            sum = (int)(node->clock_fraction + sound_seq_current->step_fraction);
            node->clock_fraction = (unsigned int)sum & 65535;
            sum >>= 16;
            node->clock_time = (int)((unsigned int)node->clock_time + (unsigned int)sum +
                (unsigned int)sound_seq_current->step_whole);
        }
        node = next;
    }
    return 1;
}

/* (through a pointer to the first pattern: the code the per-function views of the patterns gave) */
#define PATTERN(i) (((SequenceTrack *)sound_seq_current->patterns)[i])
/* Plays the current slot's due pitch-bend changes. A stream is pairs of a time delta (1 byte, or 2 with the top
 * bit: 15 bits) and a change (1 byte: 7 bits signed, or 2 with the top bit: 15 bits signed); 0x80 0x00 ends it.
 * Each change moves the track's 14-bit pitch bend, sent as the library's two pitch-bend controllers. */
void sound_seq_play_pitch_bend_streams(void)
{
    int i;
    unsigned char done;
    unsigned char first,second;
    unsigned short increment;
    short change;
    unsigned int now,deadline;
    int width;
    for (i=0;i<SEQ_TRACKS;i++) {
        done=0;
        if (PATTERN(i).pitch_bend != 0) {
            do {
                first=PATTERN(i).pitch_bend[0];
                second=PATTERN(i).pitch_bend[1];
                if (first==128 && second==0) {
                    PATTERN(i).pitch_bend=0;
                    done=1;
                } else {
                    if (first & 128) {
                        increment=((first & 127) << 8) | second;
                        width=2;
                    } else {
                        increment=first;
                        width=1;
                    }
                    deadline=PATTERN(i).pitch_time+increment;
                    now=PATTERN(i).whole+sound_seq_current->lookahead;
                    if (now >= deadline) {
                        PATTERN(i).pitch_bend += width;
                        PATTERN(i).pitch_time=deadline;
                        first=PATTERN(i).pitch_bend[0];
                        second=PATTERN(i).pitch_bend[1];
                        if (first & 128) {
                            change=((first & 255) << 8) | second;
                            change <<= 1;
                            change >>= 1;
                            PATTERN(i).pitch_bend += 2;
                        } else {
                            change=first;
                            change <<= 9;
                            change >>= 9;
                            ++PATTERN(i).pitch_bend;
                        }
                        PATTERN(i).pitch_value += change;
                        sound_midi_set_controller(SOUND_CC_PITCH_BEND,PATTERN(i).channel,(unsigned char)sound_seq_current_index,PATTERN(i).pitch_value >> 7);
                        sound_midi_set_controller(SOUND_CC_PITCH_BEND_LSB,PATTERN(i).channel,(unsigned char)sound_seq_current_index,PATTERN(i).pitch_value & 127);
                    } else {
                        done=1;
                    }
                }
            } while (!done);
        }
    }
}

/* Plays the current slot's due modulation changes, as sound_seq_play_pitch_bend_streams: the stream has the same
 * form, and each change moves the track's 14-bit modulation, sent as MIDI controllers 1 and 33. */
void sound_seq_play_modulation_streams(void)
{
    int i;
    unsigned char done;
    unsigned char first,second;
    short change;
    unsigned int now,deadline;
    int width;
    for (i=0;i<SEQ_TRACKS;i++) {
        done=0;
        if (PATTERN(i).modulation != 0) {
            do {
                first=PATTERN(i).modulation[0];
                second=PATTERN(i).modulation[1];
                if (first==128 && second==0) {
                    PATTERN(i).modulation=0;
                    done=1;
                } else {
                    if (first & 128) {
                        change=((short)(first & 127) << 8) | (short)second;
                        width=2;
                    } else {
                        change=first;
                        width=1;
                    }
                    deadline=PATTERN(i).modulation_time+change;
                    now=PATTERN(i).whole+sound_seq_current->lookahead;
                    if (now >= deadline) {
                        PATTERN(i).modulation += width;
                        PATTERN(i).modulation_time=deadline;
                        first=PATTERN(i).modulation[0];
                        second=PATTERN(i).modulation[1];
                        if (first & 128) {
                            change=((short)(first & 255) << 8) | (short)second;
                            change <<= 1;
                            change >>= 1;
                            PATTERN(i).modulation += 2;
                        } else {
                            /* Original game: a one-byte change reads as 0..127 here (its top bit is clear), where
                             * the pitch-bend stream takes it as 7 bits signed. */
                            change=(signed char)first;
                            ++PATTERN(i).modulation;
                        }
                        PATTERN(i).modulation_value += change;
                        sound_midi_set_controller(MIDI_CC_MODULATION,PATTERN(i).channel,(unsigned char)sound_seq_current_index,PATTERN(i).modulation_value >> 7);
                        sound_midi_set_controller(MIDI_CC_MODULATION_LSB,PATTERN(i).channel,(unsigned char)sound_seq_current_index,PATTERN(i).modulation_value & 127);
                    } else {
                        done=1;
                    }
                }
            } while (!done);
        }
    }
}

#define HEADER ((SongHeader *)sound_seq_current->song10C)
#define SONG_TRACK(i) (((SongTrackCursor *)sound_seq_current->unknown128)[i])
/* Plays the current slot's due pattern-list events, track by track, and advances the tracks' clocks. A pattern
 * event starts the pattern on the track (its note events, its streams, the track's channel, transpose and velocity
 * offset, a program change and a volume); a loop event jumps back to the loop point at the song's loop time
 * (the first loop of the tick also rewinds the tempo track and counts the loop), or ends the track if looping is
 * off; an end event ends the track. Returns 1 if any track still has events. */
u8 sound_seq_play_tracks(void)
{
    int u1, u2, u3, u4; /* unused: the N64 build's frame layout */
    int i;
    u8 first_loop;
    u8 active;
    SongTrackEvent *event;
    SongHeader *sequence;
    u32 *patterns;
    SongPattern *pattern;
    u32 sum;
    u32 index;
    first_loop = 1;
    active = 0;
    for (i = 0; i < SEQ_TRACKS; i++) {
        if (SONG_TRACK(i).cursor != 0) {
            active = 1;
            while (SONG_TRACK(i).time + sound_seq_current->lookahead >= SONG_TRACK(i).cursor->time) {
                event = SONG_TRACK(i).cursor;
                switch (event->code) {
                case SEQ_TRACK_END:
                    SONG_TRACK(i).cursor = 0;
                    goto next_track;
                case SEQ_TRACK_LOOP:
                    if (sound_seq_current->loop_disabled != 0) {
                        SONG_TRACK(i).cursor = 0;
                        goto next_track;
                    }
                    index = event->value.jump;
                    SONG_TRACK(i).cursor = SONG_TRACK(i).start + index;
                    SONG_TRACK(i).time = HEADER->loop_time;
                    SONG_TRACK(i).fraction = 0;
                    if (first_loop) {
                        first_loop = 0;
                        sound_seq_rewind_tempo_track(HEADER->loop_time);
                        sound_seq_current->loop_count++;
                    }
                    break;
                default:
                    sequence = HEADER;
                    patterns = (u32 *)((u8 *)sequence + sequence->pattern_table);
                    index = event->code;
                    pattern = (SongPattern *)((u8 *)sequence + patterns[index]);
                    PATTERN(i).whole = 0;
                    PATTERN(i).fraction = 0;
                    PATTERN(i).events = pattern->data;
                    PATTERN(i).event_time = 0;
                    if (pattern->pitch_bend) {
                        PATTERN(i).pitch_bend = (u8 *)HEADER + pattern->pitch_bend;
                    } else {
                        PATTERN(i).pitch_bend = 0;
                    }
                    PATTERN(i).pitch_time = 0;
                    PATTERN(i).pitch_value = 0x2000;
                    if (pattern->modulation) {
                        PATTERN(i).modulation = (u8 *)HEADER + pattern->modulation;
                    } else {
                        PATTERN(i).modulation = 0;
                    }
                    PATTERN(i).modulation_time = 0;
                    PATTERN(i).modulation_value = 0;
                    sequence = HEADER;
                    PATTERN(i).channel = ((u8 *)sequence + sequence->channel_table)[i];
                    PATTERN(i).velocity_offset = event->value.bytes.velocity_offset;
                    PATTERN(i).transpose = event->value.bytes.transpose;
                    PATTERN(i).number = i;
                    if (event->program != 0xFF) {
                        sound_seq_set_program(sound_seq_current, event->program, PATTERN(i).channel);
                    }
                    if (event->controller != 0xFF) {
                        sound_seq_set_channel_volume(event->controller, PATTERN(i).channel);
                    }
                    SONG_TRACK(i).cursor++;
                    break;
                }
            }
            sum = SONG_TRACK(i).fraction + sound_seq_current->step_fraction;
            SONG_TRACK(i).fraction = sum & 0xFFFF;
            sum >>= 16;
            SONG_TRACK(i).time = SONG_TRACK(i).time + sum + sound_seq_current->step_whole;
        }
next_track:
        ;
    }
    return active;
}
#undef SONG_TRACK
#undef HEADER
#undef PATTERN

/* Works out how far the current slot's song moves per library tick: song ticks per second at its tempo (384 a
 * beat, as 3072 / 8) times the tick's length in seconds (samples per tick over the synth's rate, 16.16), scaled
 * by the slot's speed; and half of it as the lookahead. */
void sound_seq_update_tick_step(void)
{
    unsigned int value;
    value = ((sound_seq_current->rate124 * 3072U / 60U) *
        (unsigned int)((int)((unsigned int)sound_samples_per_tick << 16) / sound_synth_frequency)) >> 3;
    value = (sound_seq_current->speed * value) >> 8;
    sound_seq_current->step_fraction = value & 65535;
    sound_seq_current->step_whole = value >> 16;
    sound_seq_current->lookahead = sound_seq_current->step_whole >> 1;
}

/* Applies the current slot's due tempo changes (tempo track entries up to now plus the lookahead). */
void sound_seq_play_tempo_track(void)
{
    if (sound_seq_current->timingStartF68) {
        while (sound_seq_current->tempo_cursor->time != 0xFFFFFFFFU) {
            if (sound_seq_current->tempo_time + sound_seq_current->lookahead <
                sound_seq_current->tempo_cursor->time) break;
            sound_synth_set_tempo(sound_seq_current->rate124 =
                sound_seq_current->tempo_cursor->value, sound_seq_current_index);
            sound_seq_current->tempo_cursor++;
        }
    }
}

/* Advances the current slot's tempo-track clock by one step. */
void sound_seq_advance_tempo_clock(void)
{
    unsigned int value;
    if (sound_seq_current->timingStartF68) {
        value = sound_seq_current->tempo_fraction + sound_seq_current->step_fraction;
        sound_seq_current->tempo_fraction = value & 0xFFFF;
        value >>= 16;
        sound_seq_current->tempo_time = sound_seq_current->tempo_time + value + sound_seq_current->step_whole;
    }
}

/* At a loop: restarts the current slot's tempo track from `time` (the loop time), applies its due entries and
 * recomputes the step. */
void sound_seq_rewind_tempo_track(unsigned int time)
{
    if (sound_seq_current->timingStartF68) {
        sound_seq_current->tempo_cursor = sound_seq_current->timingStartF68;
        sound_seq_current->tempo_time = time;
        sound_seq_current->tempo_fraction = 0;
        sound_seq_play_tempo_track();
        sound_seq_update_tick_step();
    }
}

/* Returns how many times the song has looped (0 for a request handle, an unknown song, or no library). */
unsigned short sound_seq_get_loop_count(unsigned int identifier)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF && (index & SEQ_REQUEST_BIT) == 0) return sound_seq_players[index].loop_count;
    }
    return 0;
}

/* Is the song (or the request it stands for) still on a slot? */
u8 sound_seq_is_playing(u32 identifier)
{
    if (sound_initialized) return sound_seq_find_slot(identifier) != 0xFFFFFFFF;
    return 0;
}

/* Pauses a playing song: its notes stop and are freed. For a request handle, the request will start its song
 * paused. Takes no lock (the caller holds the library's). */
void sound_seq_pause_unlocked(unsigned int identifier)
{
    identifier = sound_seq_find_slot(identifier);
    if (identifier != 0xFFFFFFFFU) {
        if ((identifier & SEQ_REQUEST_BIT) == 0) {
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].activeFC0 = 0;
                sound_seq_stop_player_notes(&sound_seq_players[identifier]);
                sound_seq_free_player_notes(&sound_seq_players[identifier]);
            }
        } else {
            identifier &= SEQ_ID_MASK;
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].flagsFEE |= SEQ_REQUEST_NEW_PAUSED;
            }
        }
    }
}

/* sound_seq_pause_unlocked under the library's lock. */
void sound_seq_pause(unsigned int identifier)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_pause_unlocked(identifier);
        sound_hw_unlock();
    }
}

/* Stops a song: its notes stop, and its slot is freed. For a request handle, the request is cancelled (it will
 * start no song). Takes no lock. */
void sound_seq_stop_unlocked(unsigned int identifier)
{
    identifier = sound_seq_find_slot(identifier);
    if (identifier != 0xFFFFFFFFU) {
        if ((identifier & SEQ_REQUEST_BIT) == 0) {
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].activeFC0 = 0;
                sound_seq_stop_player_notes(&sound_seq_players[identifier]);
                sound_seq_free_player_notes(&sound_seq_players[identifier]);
            }
            sound_seq_players[identifier].inactiveFC1 = 1;
        } else {
            identifier &= SEQ_ID_MASK;
            if (sound_seq_players[identifier].activeFC0 && !sound_seq_players[identifier].inactiveFC1) {
                sound_seq_players[identifier].pendingFF0 = 0;
            }
        }
    }
}

/* sound_seq_stop_unlocked under the library's lock (AudioSongStop). */
void sound_seq_stop(unsigned int identifier)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_stop_unlocked(identifier);
        sound_hw_unlock();
    }
}

/* Stops every slot's song (sound_stop_everything). */
void sound_seq_stop_all(void)
{
    int i;
    for (i = 0; i < SEQ_PLAYERS; i++) {
        sound_seq_stop_unlocked(sound_seq_players[i].identifier);
    }
}

/* Turns a song's looping on or off (off: a loop event ends its track). Returns 1, or 0 for a request handle or an
 * unknown song. */
int sound_seq_set_looping(unsigned int identifier, unsigned char enabled)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF && (index & SEQ_REQUEST_BIT) == 0) {
            sound_seq_players[index].loop_disabled = !enabled;
            return 1;
        }
    }
    return 0;
}

/* Sets a song's speed (SEQ_SPEED_NORMAL is 1.0), or the speed its request will start the next song at. Takes no
 * lock. */
void sound_seq_set_speed_unlocked(unsigned int identifier, unsigned short speed)
{
    unsigned int index;
    index = sound_seq_find_slot(identifier);
    if (index != 0xFFFFFFFFU) {
        if ((index & SEQ_REQUEST_BIT) == 0) {
            sound_seq_players[index].speed = speed;
        } else {
            index &= SEQ_ID_MASK;
            sound_seq_players[index].valueFEC = speed;
            sound_seq_players[index].flagsFEE |= SEQ_REQUEST_SPEED;
        }
    }
}

/* sound_seq_set_speed_unlocked under the library's lock. */
void sound_seq_set_speed(unsigned int identifier, unsigned short speed)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_set_speed_unlocked(identifier, speed);
        sound_hw_unlock();
    }
}

/* Resumes a paused song, or for a request handle lets the request start its song playing. Takes no lock. */
void sound_seq_resume_unlocked(unsigned int identifier)
{
    unsigned int index;
    index = sound_seq_find_slot(identifier);
    if (index != 0xFFFFFFFFU) {
        if ((index & SEQ_REQUEST_BIT) == 0) {
            sound_seq_players[index].activeFC0 = 1;
        } else {
            sound_seq_players[index & SEQ_ID_MASK].flagsFEE &= ~SEQ_REQUEST_NEW_PAUSED;
        }
    }
}

/* sound_seq_resume_unlocked under the library's lock. */
void sound_seq_resume(unsigned int identifier)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_resume_unlocked(identifier);
        sound_hw_unlock();
    }
}

/* Sets which of a song's 64 tracks play (a bit each, tracks 0-31 in low_tracks), or the mask its request will
 * give the next song. Takes no lock. */
void sound_seq_set_track_mask_unlocked(u32 identifier, u32 low_tracks, u32 high_tracks)
{
    u32 index;
    index = sound_seq_find_slot(identifier);
    if (index != 0xFFFFFFFFU) {
        if ((index & SEQ_REQUEST_BIT) == 0) {
            sound_seq_players[index].track_mask[0] = low_tracks;
            sound_seq_players[index].track_mask[1] = high_tracks;
        } else {
            index &= SEQ_ID_MASK;
            sound_seq_players[index].firstFE4 = low_tracks;
            sound_seq_players[index].secondFE8 = high_tracks;
            sound_seq_players[index].flagsFEE |= SEQ_REQUEST_TRACK_MASK;
        }
    }
}

/* sound_seq_set_track_mask_unlocked under the library's lock. */
void sound_seq_set_track_mask(unsigned int identifier, unsigned int low_tracks, unsigned int high_tracks)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_set_track_mask_unlocked(identifier, low_tracks, high_tracks);
        sound_hw_unlock();
    }
}

/* Fades a song's volume groups (its own and every track's other group) to `volume` over `duration` ms, then does
 * `mode` (SEQ_FADE_*: nothing, stop, pause, mute). For a request handle: sets the next song's volume; stop cancels
 * the request, pause and mute make it start the song paused or muted. Takes no lock. */
void sound_seq_fade_unlocked(unsigned char volume,unsigned short duration,unsigned int identifier,unsigned char mode)
{
    unsigned int original;
    int i;
    original=identifier;
    identifier=sound_seq_find_slot(identifier);
    if (identifier != 0xFFFFFFFFU) {
        if (!(identifier & SEQ_REQUEST_BIT)) {
            sound_fade_volume_group_unlocked(volume,duration,sound_seq_players[identifier].channelFC4,mode,original);
            for (i=0;i<SEQ_TRACKS;i++) {
                if (sound_seq_players[identifier].track_groups[i] != sound_seq_players[identifier].channelFC4) {
                    sound_fade_volume_group_unlocked(volume,duration,sound_seq_players[identifier].track_groups[i],SEQ_FADE_NONE,0);
                }
            }
        } else {
            identifier &= SEQ_ID_MASK;
            switch (mode & 15) {
            case SEQ_FADE_NONE: sound_seq_players[identifier].valueFE0=volume; break;
            case SEQ_FADE_STOP: sound_seq_players[identifier].pendingFF0=0; break;
            case SEQ_FADE_PAUSE:
                sound_seq_players[identifier].valueFE0=volume;
                sound_seq_players[identifier].flagsFEE |= SEQ_REQUEST_NEW_PAUSED;
                break;
            case SEQ_FADE_MUTE:
                sound_seq_players[identifier].valueFE0=volume;
                sound_seq_players[identifier].flagsFEE |= SEQ_REQUEST_NEW_MUTED;
                break;
            }
        }
    }
}

/* sound_seq_fade_unlocked under the library's lock. */
void sound_seq_fade(unsigned char volume, unsigned short duration, unsigned int identifier, unsigned char mode)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_fade_unlocked(volume, duration, identifier, mode);
        sound_hw_unlock();
    }
}

/* Returns the song's volume group (0 for an unknown song, or no library).
 * Original game: a request handle is not refused, and its slot number keeps bit 31. On the N64 the 32-bit product
 * drops the bit (the slot is an even number of bytes) and finds the right slot; on the PC the product is 64-bit
 * and would read far outside the slots. Nothing passes a request handle here. */
unsigned char sound_seq_get_volume_group(unsigned int identifier)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF) return sound_seq_players[index].channelFC4;
    }
    return 0;
}

/* Gives track `track` of a song the volume group `group` (a studio group: sound_set_group_type 2). */
void sound_seq_set_studio_group(unsigned int identifier, unsigned char track, unsigned char group)
{
    unsigned int index;
    if (sound_initialized) {
        index = sound_seq_find_slot(identifier);
        if (index != 0xFFFFFFFF) {
            sound_seq_players[index].track_groups[track] = group;
            sound_set_group_type(group, 2);
        }
    }
}

/* Carries out a crossfade request (SequenceRequest), or defers it: with SEQ_REQUEST_AT_MARKER, a copy goes into the
 * old song's slot with the result word's address, *result becomes the request handle (the old song's id with
 * SEQ_REQUEST_BIT), and the song's crossfade marker carries it out later (sound_seq_play_note_events). Otherwise:
 * fades the old song out (to a stop, a pause or silence), then, if there is a result word, resumes the next song
 * (SEQ_REQUEST_RESUME_NEW: fades it in, with its track mask and speed) or starts it (sound_start_song_in_group,
 * fading in, maybe paused, muted, with track mask and speed), and stores the next song's handle in *result
 * (0xFFFFFFFF if none). unlocked: the caller holds the library's lock (the marker), else each step takes it. */
void sound_seq_crossfade_unlocked(void *request, unsigned int *result, unsigned char unlocked)
{
    SequencePlayParams options;
    unsigned int index;
    SequenceRequest *state = request;
    index = sound_seq_find_slot(state->song);
    if (index != 0xFFFFFFFFU) {
        if (state->flags & SEQ_REQUEST_AT_MARKER) {
            *(SequenceRequest *)sound_seq_players[index].unknownFC8 = *state;
            sound_seq_players[index].unknownFF4[0] = 1;
            sound_seq_players[index].flagsFEE &= ~SEQ_REQUEST_AT_MARKER;
            sound_seq_players[index].pendingFF0 = result;
            *result = state->song | SEQ_REQUEST_BIT;
        } else {
            if (unlocked) {
                if (state->flags & SEQ_REQUEST_PAUSE_OLD) sound_seq_fade_unlocked(0,state->fade_out_time,state->song,SEQ_FADE_PAUSE);
                else if (state->flags & SEQ_REQUEST_MUTE_OLD) sound_seq_fade_unlocked(0,state->fade_out_time,state->song,SEQ_FADE_MUTE);
                else sound_seq_fade_unlocked(0,state->fade_out_time,state->song,SEQ_FADE_STOP);
            } else {
                if (state->flags & SEQ_REQUEST_PAUSE_OLD) sound_seq_fade(0,state->fade_out_time,state->song,SEQ_FADE_PAUSE);
                else if (state->flags & SEQ_REQUEST_MUTE_OLD) sound_seq_fade(0,state->fade_out_time,state->song,SEQ_FADE_MUTE);
                else sound_seq_fade(0,state->fade_out_time,state->song,SEQ_FADE_STOP);
            }
            if (result != 0) {
                if (state->flags & SEQ_REQUEST_RESUME_NEW) {
                    if (sound_seq_find_slot(state->next_song) != 0xFFFFFFFFU) {
                        if (unlocked) {
                            sound_seq_resume_unlocked(state->next_song);
                            sound_seq_fade_unlocked(state->next_volume,state->fade_in_time,state->next_song,SEQ_FADE_NONE);
                            if (state->flags & SEQ_REQUEST_TRACK_MASK) sound_seq_set_track_mask_unlocked(state->next_song,state->track_mask[0],state->track_mask[1]);
                            if (state->flags & SEQ_REQUEST_SPEED) sound_seq_set_speed_unlocked(state->next_song,state->speed);
                        } else {
                            sound_seq_resume(state->next_song);
                            sound_seq_fade(state->next_volume,state->fade_in_time,state->next_song,SEQ_FADE_NONE);
                            if (state->flags & SEQ_REQUEST_TRACK_MASK) sound_seq_set_track_mask(state->next_song,state->track_mask[0],state->track_mask[1]);
                            if (state->flags & SEQ_REQUEST_SPEED) sound_seq_set_speed(state->next_song,state->speed);
                        }
                        *result = state->next_song;
                    } else *result = 0xFFFFFFFFU;
                } else {
                    /* Only the options the flags and counts name are read by sound_seq_start; the rest stay
                     * unset. */
                    options.flags = SEQ_PLAY_FADE;
                    if (state->flags & SEQ_REQUEST_NEW_PAUSED) options.flags = SEQ_PLAY_FADE | SEQ_PLAY_PAUSED;
                    if (state->flags & SEQ_REQUEST_SPEED) { options.flags |= SEQ_PLAY_SPEED; options.speed = state->speed; }
                    if (state->flags & SEQ_REQUEST_TRACK_MASK) { options.flags |= SEQ_PLAY_TRACK_MASK; options.track_mask[0] = state->track_mask[0]; options.track_mask[1] = state->track_mask[1]; }
                    options.fade_time = state->fade_in_time;
                    options.fade_volume = state->next_volume;
                    options.faded_count = 0;
                    if (unlocked) {
                        *result = sound_start_song_in_group(state->group,state->song_number,state->next_data,&options,1);
                        if (*result != 0xFFFFFFFFU && (state->flags & SEQ_REQUEST_NEW_MUTED)) sound_seq_set_track_mask_unlocked(*result,0,0);
                    } else {
                        *result = sound_start_song(state->group,state->song_number,state->next_data,&options);
                        if (*result != 0xFFFFFFFFU && (state->flags & SEQ_REQUEST_NEW_MUTED)) sound_seq_set_track_mask(*result,0,0);
                    }
                }
            }
        }
    }
}

/* sound_seq_crossfade_unlocked under the library's lock (each step takes it). */
void sound_seq_crossfade(void *state, unsigned int *result)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_seq_crossfade_unlocked(state, result, 0);
        sound_hw_unlock();
    }
}

/* Has a crossfade's result word been filled (no longer the request handle)? A failed one (0xFFFFFFFF) counts as
 * done. */
unsigned char sound_seq_crossfade_done(unsigned int *state) { if (*state != 0xFFFFFFFF) return (*state & SEQ_REQUEST_BIT) == 0; return 1; }

/* The sequencer's tick (from the synth's tick): for each playing slot, the tempo track, the step, the tracks, the
 * note events, the streams, the pattern clocks and the notes' ends; a slot with no tracks, patterns or notes left
 * is done and freed. */
void sound_seq_tick(void)
{
    int i;
    unsigned char tracks_active, patterns_active, notes_active;
    if (sound_samples_per_tick) {
        for (i = 0; i < SEQ_PLAYERS; i++) {
            if (sound_seq_players[i].activeFC0) {
                sound_seq_current = &sound_seq_players[i];
                sound_seq_current_index = i;
                sound_seq_fading_out = sound_is_group_fading_out(sound_seq_players[i].channelFC4);
                sound_seq_play_tempo_track();
                sound_seq_update_tick_step();
                sound_seq_advance_tempo_clock();
                tracks_active = sound_seq_play_tracks();
                patterns_active = sound_seq_play_note_events();
                sound_seq_play_pitch_bend_streams();
                sound_seq_play_modulation_streams();
                sound_seq_advance_pattern_clocks();
                notes_active = sound_seq_end_due_notes();
                sound_seq_reap_released_notes();
                if (!tracks_active && !patterns_active && !notes_active) {
                    sound_seq_players[i].activeFC0 = 0;
                    sound_seq_players[i].inactiveFC1 = 1;
                }
            }
        }
    }
}

/* Starts the sequencer: every slot free, the note records free, the handles from 0. */
void sound_seq_init(void)
{
    unsigned int index;
    for (index = 0; index < SEQ_PLAYERS; index++) {
        sound_seq_players[index].activeFC0 = 0;
        sound_seq_players[index].inactiveFC1 = 1;
    }
    sound_seq_init_notes();
    sound_seq_reset_ids();
}
