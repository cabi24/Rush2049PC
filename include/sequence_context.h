/* Native N64 sequence-context layout, reconciled from lib_17dc0 consumers and
 * constructors. Unknown ranges are real record storage, not stack/code padding.
 * Byte-matched against the retail ROM, 2026-10.
 * sequence_context.h (identical declarations; adopted).
 * names 0xFC5 (byte) and 0xFC6 (halfword) from the unknown range,
 * per sound_seq_set_looping/sound_seq_get_loop_count; the layout is unchanged. */
#ifndef BOOT_TAIL_SEQUENCE_CONTEXT_H
#define BOOT_TAIL_SEQUENCE_CONTEXT_H

typedef struct SequenceNode {
    struct SequenceNode *next;
    struct SequenceNode *previous;
    unsigned int identifier;
    int deadline0C;                           /* timed nodes: due time (whole ticks) */
    unsigned int fraction10;                  /* timed nodes: 16.16 clock, fraction */
    int whole14;                              /* timed nodes: 16.16 clock, whole */
} SequenceNode;

typedef struct SequenceTimedValue {
    unsigned int time;
    unsigned int value;
} SequenceTimedValue;

/* PC: a slot holds host pointers (into the song and the project, to sequence nodes, to a caller's result
 * word), so it is laid out natively: 0x1628 bytes on LP64 for the N64's 0xFF8. This header has the slot's
 * whole field sequence with every pointer as a pointer, also where the N64 header has a byte range, and on the
 * PC every other view of a slot in sequencer.c is this sequence under its own names (SEQ_PC_OFF, SEQ_PC_SAME).
 * gen_data counts the pointers as 4 bytes, which gives the N64 layout back (docs/port/MUSYX_DATA.md 8.2). */
typedef struct SequenceSlotTrack {            /* 0x10: an event track (sound_seq_start, sound_seq_play_tracks) */
    void *start;
    void *cursor;
    unsigned int fraction;
    unsigned int time;
} SequenceSlotTrack;

typedef struct SequenceTrack {                /* 0x28; per-track 16.16 clock */
    unsigned int fraction;
    int whole;
    unsigned int unknown08;
    unsigned char *active0C;                  /* note events (sound_seq_play_note_events) */
    unsigned char *active10;                  /* pitch-bend stream (sound_seq_play_pitch_bend_streams) */
    unsigned char *active14;                  /* modulation stream (sound_seq_play_modulation_streams) */
    unsigned char unknown18[16];
} SequenceTrack;

typedef struct SequenceContext {
    unsigned int identifier;                  /* 000 */
    void *normtab004;                         /* 004: the normal page */
    unsigned char normTrans008[128];
    void *drumtab088;                         /* 088: the drum page */
    unsigned char drumTrans08C[128];
    void *song10C;                            /* 10C: the song header */
    unsigned int first110;
    unsigned int second114;
    unsigned int fraction118;
    int whole11C;
    unsigned int half120;
    unsigned int rate124;
    SequenceSlotTrack unknown128[64];
    unsigned char channels528[64];
    SequenceTrack tracks568[64];
    SequenceTimedValue *timingStartF68;
    SequenceTimedValue *currentF6C;
    unsigned int lowF70;
    unsigned int highF74;
    SequenceNode *active;                      /* F78 */
    SequenceNode *pending;                     /* F7C */
    unsigned char unknownF80[0x40];
    unsigned char activeFC0;
    unsigned char inactiveFC1;
    unsigned short valueFC2;
    unsigned char channelFC4;
    unsigned char disabledFC5;
    unsigned short valueFC6;
    unsigned char unknownFC8[0x10];           /* FC8: a SequenceRequest (sound_seq_crossfade_unlocked) to FF0 */
    void *streamFD8;
    unsigned char unknownFDC[4];
    unsigned char valueFE0;
    unsigned char unknownFE1[3];
    unsigned int firstFE4;
    unsigned int secondFE8;
    unsigned short valueFEC;
    unsigned char flagsFEE;
    unsigned char unknownFEF;
    uintptr_t pendingFF0;                     /* FF0: the request's result word, an unsigned int * */
    unsigned char unknownFF4[4];
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
