/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (the file must also define StopSound: -O3 inlines it at all six call sites) */
/*
 * Stops and clears every looping sound handle owned by one car (N64 sound
 * bookkeeping; no arcade ancestor identified -- the arcade keeps these in
 * the MODELDAT/sound tables differently).   is the car's MODELDAT
 * (slot at +0x7C6, s8 mode at +0x7CC).  Runs once per armed flag
 * D_8010FFC4[slot]; D_8010FFCC[slot] is set to 1 for even slots.
 *   - D_80140420[slot] (0x54 bytes: s32 + four 0x14-byte handle records):
 *     records 0, 1 and 3 are released if their handle is not -1;
 *   - D_80140640[slot] is always released;
 *   - only when mode == 2: the three records gRoadNoises[slot][0..2] and the
 *     bare handles gScrapes/gHornSounds/gBogSounds[slot] (set to -1), with
 *     gScrapeState[slot] and gScrapeTime[slot] zeroed.
 * A record is released by StopSound(rec, flag) (0x800D54E0,
 * already matched; reproduced unchanged here): flag != 0 calls
 * AudStopEmitter(handle), flag == 0 calls AudStopEffect(handle), then
 * InitSoundState resets the record (handle -1, four -2.0f).
 * The historical labels of all four callees are misleading.
 *
 * Whole-program fact: StopSound is inlined by umerge at every
 * call (frame +8, 'li at,40' loop bound left unhoisted).  Written out by hand
 * the same statements keep 40 in s7.
 * Shaping quirk: the code-free dead read 'if (i) {}' in the first loop.  It
 * raises the counter web above the pointer web (s1/s2) and with it m above
 * the constant 2 (s3/s4); without it 19/157 words differ, all register names.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef float f32;

typedef struct {
    s32 handle;
    f32 unk4[4];
} SndRec; /* 0x14 */

typedef struct {
    intptr_t unk0;   /* Player84's definition pointer (car_sounds/d5e64.c) */
    SndRec rec[4];
} SndSet; /* 0x54 */

typedef struct {
    u8 pad0[0x7C6];
    s16 slot;
    u8 pad7C8[4];
    s8 unk7CC;
} MODELDAT;

/* On the PC the cars' sound records are PC data, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 7). */
extern SndRec gRoadNoises[][3];
extern s32 gScrapes[];
extern s16 gScrapeState[];
extern f32 gScrapeTime[];
extern s32 gHornSounds[];
extern s32 gBogSounds[];

void AudStopEmitter(s32 handle);
void AudStopEffect(s32 handle);
void InitSoundState(SndRec *rec);

void StopSound(SndRec *rec, s32 arg1);

void StopCarSounds(MODELDAT *m) {
    s32 slot = m->slot;
    s32 i;

    if (PC_CAR_SOUNDS_ON(slot)) {
        PC_CAR_SOUNDS_ON(slot) = 0;
        PC_ENGINE_PHASE(slot) = (slot & 1) == 0;
        for (i = 0; i < 2; i++) {
            if (i) {}
            if (PC_ENGINE_SOUND_HANDLE(slot, i) != -1) {
                StopSound(PC_ENGINE_SOUND_REC(slot, i), m->unk7CC != 2);
            }
        }
        if (PC_ENGINE_SOUND_HANDLE(slot, 3) != -1) {
            StopSound(PC_ENGINE_SOUND_REC(slot, 3), m->unk7CC != 2);
        }
        StopSound(PC_SCREECH(slot), m->unk7CC != 2);
        if (m->unk7CC == 2) {
            StopSound(&gRoadNoises[slot][0], 0);
            StopSound(&gRoadNoises[slot][1], 0);
            StopSound(&gRoadNoises[slot][2], 0);
            AudStopEffect(gScrapes[slot]);
            gScrapes[slot] = -1;
            gScrapeState[slot] = 0;
            gScrapeTime[slot] = 0.0f;
            AudStopEffect(gHornSounds[slot]);
            gHornSounds[slot] = -1;
            AudStopEffect(gBogSounds[slot]);
            gBogSounds[slot] = -1;
        }
    }
}
