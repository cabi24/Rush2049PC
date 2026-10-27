/* Native N64 sequence-context layout, reconciled from lib_17dc0 consumers and
 * constructors. Unknown ranges are real record storage, not stack/code padding.
 * Byte-matched against the retail ROM, 2026-10.
 * sequence_context.h (identical declarations; adopted).
 * names 0xFC5 (byte) and 0xFC6 (halfword) from the unknown range,
 * per func_80018EB4/func_80018B8C; the layout is unchanged. */
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

#ifdef TARGET_PC
/* PC: a slot holds host pointers (into the song and the project, to sequence nodes, to a caller's result
 * word), so it is laid out natively: 0x1628 bytes on LP64 for the N64's 0xFF8. This header has the slot's
 * whole field sequence with every pointer as a pointer, also where the N64 header has a byte range, and on the
 * PC every other view of a slot in lib_17dc0.c is this sequence under its own names (SEQ_PC_OFF, SEQ_PC_SAME).
 * gen_data counts the pointers as 4 bytes, which gives the N64 layout back (docs/port/MUSYX_DATA.md 8.2). */
typedef struct SequenceSlotTrack {            /* 0x10: an event track (func_800178B0, func_80018634) */
    void *start;
    void *cursor;
    unsigned int fraction;
    unsigned int time;
} SequenceSlotTrack;

typedef struct SequenceTrack {                /* 0x28; per-track 16.16 clock */
    unsigned int fraction;
    int whole;
    unsigned int unknown08;
    unsigned char *active0C;                  /* note events (func_80017D38) */
    unsigned char *active10;                  /* pitch-bend stream (func_8001824C) */
    unsigned char *active14;                  /* modulation stream (func_80018448) */
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
    unsigned char unknownFC8[0x10];           /* FC8: a SequenceRequest (func_80019490) to FF0 */
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
#else
typedef struct SequenceTrack {                /* 0x28; per-track 16.16 clock */
    unsigned int fraction;
    int whole;
    unsigned int unknown08;
    unsigned int active0C;
    unsigned int active10;
    unsigned int active14;
    unsigned char unknown18[16];
} SequenceTrack;

typedef struct SequenceContext {
    unsigned int identifier;                  /* 000 */
    unsigned char unknown004[0x10C];
    unsigned int first110;
    unsigned int second114;
    unsigned int fraction118;                 /* 118: tempo increment, 16-bit fraction */
    int whole11C;                             /* 11C: tempo increment, whole part */
    unsigned int half120;
    unsigned int rate124;
    unsigned char unknown128[0x400];
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
    unsigned char unknownFC8[0x18];
    unsigned char valueFE0;
    unsigned char unknownFE1[3];
    unsigned int firstFE4;
    unsigned int secondFE8;
    unsigned short valueFEC;
    unsigned char flagsFEE;
    unsigned char unknownFEF;
    unsigned int pendingFF0;
    unsigned char unknownFF4[4];
} SequenceContext;
#endif

extern SequenceNode *D_80043EB0;
extern SequenceContext D_80043EB8[8];
extern SequenceContext *D_8004BE80;
extern unsigned int D_8004BE78;
extern unsigned char D_8004BE7C;
extern unsigned int D_8004F808;

void func_8001729C(SequenceContext *);
void func_8001734C(SequenceContext *);
void func_80017470(SequenceNode *);
void func_80017540(void);
unsigned int func_80017644(unsigned int);
unsigned char func_80017D38(void);
void func_800180A0(void);
int func_80018184(void);
void func_8001824C(void);
void func_80018448(void);
unsigned char func_80018634(void);
void func_8001897C(void);
void func_80018A30(void);
void func_80018AEC(void);
void func_80018C2C(unsigned int);
void func_80018D40(unsigned int);
void func_80019194(unsigned char, unsigned short, unsigned int, unsigned char);
void func_80019A60(unsigned int, unsigned char);
void func_8001B9F8(unsigned char, unsigned short, unsigned char,
                   unsigned char, unsigned int);
int func_8001BDB8(unsigned char);
unsigned int func_800201D0(unsigned int);
#endif
