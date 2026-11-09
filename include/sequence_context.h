/* include/sequence_context.h: the MusyX sequencer's song player ("slot") and note records (src/rom/sound/sequencer.c).
 *
 * Eight slots (sound_seq_players) each play one song: its 64 tracks are lists of pattern references, each
 * pattern a stream of note events with optional pitch-bend and modulation streams; a tempo track changes the tempo;
 * notes that are playing or released are kept on lists of SequenceNode. sequencer.c reads a slot through this
 * header and through a few views of its own (each tied to the header's offsets by SEQ_PC_SAME).
 *
 * Several fields still carry their N64 offset in their name (normtab004, song10C, rate124, unknown128, activeFC0,
 * the request copy from unknownFC8 to unknownFF4 ...): port/assets/tests/test_musyx.c reads them by those names,
 * and include/game/types.h has a copy of the layout. The comment beside each says what it is. */
#ifndef BOOT_TAIL_SEQUENCE_CONTEXT_H
#define BOOT_TAIL_SEQUENCE_CONTEXT_H

/* A note the sequencer started (or a free one, on sound_seq_free_notes). */
typedef struct SequenceNode {
    struct SequenceNode *next;
    struct SequenceNode *previous;
    unsigned int identifier;                  /* the note's voice handle (sound_synth_start_note) */
    int end_time;                             /* the track time at which the note is keyed off */
    unsigned int clock_fraction;              /* the note's own 16.16 clock (from its track's): fraction */
    int clock_time;                           /* and whole ticks */
} SequenceNode;

/* An entry of the tempo track: from `time` (ticks) on, the tempo is `value` (beats per minute); 0xFFFFFFFF ends it. */
typedef struct SequenceTimedValue {
    unsigned int time;
    unsigned int value;
} SequenceTimedValue;

/* PC: a slot holds host pointers (into the song and the project, to sequence nodes, to a caller's result
 * word), so it is laid out natively: 0x1628 bytes on LP64 for the N64's 0xFF8. This header has the slot's
 * whole field sequence with every pointer as a pointer, also where the N64 header has a byte range, and on the
 * PC every other view of a slot in sequencer.c is this sequence under its own names (SEQ_PC_OFF, SEQ_PC_SAME).
 * gen_data counts the pointers as 4 bytes, which gives the N64 layout back (docs/port/MUSYX_DATA.md 8.2). */
typedef struct SequenceSlotTrack {            /* one of a song's 64 tracks: its list of pattern references */
    void *start;                              /* the track's first event (SongTrackEvent) */
    void *cursor;                             /* the next event */
    unsigned int fraction;                    /* the track's 16.16 clock: fraction */
    unsigned int time;                        /* and whole ticks */
} SequenceSlotTrack;

typedef struct SequenceTrack {                /* the pattern a track plays, and its streams */
    unsigned int fraction;                    /* the pattern's 16.16 clock: fraction */
    int whole;                                /* and whole ticks, from the pattern's start */
    unsigned int event_time;                  /* the time of the last note event */
    unsigned char *events;                    /* the next note event (sound_seq_play_note_events) */
    unsigned char *pitch_bend;                /* the next pitch-bend change (sound_seq_play_pitch_bend_streams) */
    unsigned char *modulation;                /* the next modulation change (sound_seq_play_modulation_streams) */
    unsigned short pitch_value;               /* the current pitch bend, 14 bits (0x2000 centre) */
    unsigned short modulation_value;          /* the current modulation, 14 bits */
    unsigned int pitch_time;                  /* the time of the last pitch-bend change */
    unsigned int modulation_time;             /* the time of the last modulation change */
    unsigned char channel;                    /* the MIDI channel the track plays on */
    signed char velocity_offset;              /* added to every note's velocity */
    signed char transpose;                    /* added to every note's key */
    unsigned char number;                     /* the track's number (its bit in track_mask, its volume group) */
} SequenceTrack;

typedef struct SequenceContext {
    unsigned int identifier;                  /* the song's handle (sound_seq_assign_id) */
    void *normtab004;                         /* the normal program page (SequencePageEntry: macro, priority ...) */
    unsigned char normTrans008[128];          /* program number to normal page entry, 255 for none */
    void *drumtab088;                         /* the drum page, for MIDI channel 10 */
    unsigned char drumTrans08C[128];          /* program number to drum page entry */
    void *song10C;                            /* the song header (SongHeader) */
    unsigned int track_mask[2];               /* a set bit lets that track's notes play (sound_seq_set_track_mask) */
    unsigned int step_fraction;               /* ticks the song advances per library tick, 16.16: fraction */
    int step_whole;                           /* and whole ticks */
    unsigned int lookahead;                   /* half the step: events up to now + this play this tick */
    unsigned int rate124;                     /* the tempo, beats per minute */
    SequenceSlotTrack unknown128[64];         /* the 64 tracks' pattern lists */
    unsigned char track_groups[64];           /* each track's volume group */
    SequenceTrack patterns[64];               /* the pattern each track plays */
    SequenceTimedValue *timingStartF68;       /* the tempo track, or null */
    SequenceTimedValue *tempo_cursor;         /* its next entry */
    unsigned int tempo_fraction;              /* the tempo track's 16.16 clock: fraction */
    unsigned int tempo_time;                  /* and whole ticks */
    SequenceNode *notes;                      /* the notes playing */
    SequenceNode *released_notes;             /* the notes keyed off whose voices still sound */
    unsigned char unknownF80[0x40];           /* programs[16], per MIDI channel: macro << 16 | priority << 8 |
                                               * max voices, or 0xFFFFFFFF (sound_seq_set_program) */
    unsigned char activeFC0;                  /* the song plays (0: paused, or done) */
    unsigned char inactiveFC1;                /* the slot is free */
    unsigned short speed;                     /* tempo scale, 256 for 1.0 (sound_seq_set_speed) */
    unsigned char channelFC4;                 /* the song's volume group (23 + the slot) */
    unsigned char loop_disabled;              /* a loop event ends the track instead (sound_seq_set_looping) */
    unsigned short loop_count;                /* the loops played (sound_seq_get_loop_count) */
    /* From here to pendingFF0: a copy of a deferred crossfade request (sequencer.c's SequenceRequest), carried out
     * when the song reaches its controller-104 marker. Commands to the request's handle (the song's id with bit 31)
     * change the copy. */
    unsigned char unknownFC8[0x10];           /* the request's ids and fade times */
    void *streamFD8;                          /* the next song's data */
    unsigned char unknownFDC[4];              /* its group and song numbers */
    unsigned char valueFE0;                   /* the next song's volume */
    unsigned char unknownFE1[3];
    unsigned int firstFE4;                    /* its track mask */
    unsigned int secondFE8;
    unsigned short valueFEC;                  /* its speed */
    unsigned char flagsFEE;                   /* the request's SEQ_REQUEST_* flags */
    unsigned char unknownFEF;
    uintptr_t pendingFF0;                     /* the caller's result word (an unsigned int *), 0 when cancelled */
    unsigned char unknownFF4[4];              /* [0]: a deferred request is waiting */
} SequenceContext;

/* Where a field of the slot is on the PC, and a check that a view's field is there. */
#define SEQ_PC_OFF(field) __builtin_offsetof(SequenceContext, field)
#define SEQ_PC_SPAN(from, to) (SEQ_PC_OFF(to) - SEQ_PC_OFF(from))
#define SEQ_PC_SAME(view, vfield, field) \
    typedef char pc_seq_##view##_##vfield[__builtin_offsetof(view, vfield) == SEQ_PC_OFF(field) ? 1 : -1]

extern SequenceNode *sound_seq_free_notes;
extern SequenceContext sound_seq_players[8];
extern SequenceContext *sound_seq_current;
extern unsigned int sound_seq_current_index;
extern unsigned char sound_seq_fading_out;
extern unsigned int sound_samples_per_tick;

void sound_seq_free_player_notes(SequenceContext *);
void sound_seq_stop_player_notes(SequenceContext *);
void sound_seq_free_released_note(SequenceNode *);
void sound_seq_reap_released_notes(void);
unsigned int sound_seq_find_slot(unsigned int);
unsigned char sound_seq_play_note_events(void);
void sound_seq_advance_pattern_clocks(void);
int sound_seq_end_due_notes(void);
void sound_seq_play_pitch_bend_streams(void);
void sound_seq_play_modulation_streams(void);
unsigned char sound_seq_play_tracks(void);
void sound_seq_update_tick_step(void);
void sound_seq_play_tempo_track(void);
void sound_seq_advance_tempo_clock(void);
void sound_seq_pause_unlocked(unsigned int);
void sound_seq_stop_unlocked(unsigned int);
void sound_seq_fade_unlocked(unsigned char, unsigned short, unsigned int, unsigned char);
void sound_synth_set_tempo(unsigned int, unsigned char);
void sound_fade_volume_group_unlocked(unsigned char, unsigned short, unsigned char,
                   unsigned char, unsigned int);
int sound_is_group_fading_out(unsigned char);
unsigned int sound_check(unsigned int);
#endif
