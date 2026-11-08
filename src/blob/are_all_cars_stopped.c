typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    u8 pad000[0x3F0];
    f32 unk3F0;
    u8 pad3F4[0x7C6 - 0x3F4];
    s16 unk7C6;
    u8 pad7C8[0x7CC - 0x7C8];
    s8 unk7CC;
    u8 pad7CD[0x7E8 - 0x7CD];
    s8 unk7E8;
    u8 pad7E9[0x808 - 0x7E9];
} Car;

extern s8 gDeathMode;
extern s16 gNumPlayers;
extern s32 gAPlayerReallyFinished;
extern f32 gDeathWaitEnd;
extern f32 gFrameSeconds;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s16 gNumLaps;
/* The stunt answer is the low half of gStuntsDone (s32), which CheckForStunt sets to 1 when the stunt round's time is
   up and the cars have settled: on the N64, big-endian, the code read its halfword at +2. On a little-endian host
   that halfword is the high one, always 0, and a stunt round never ended (docs/port/QA.md 4): the PC reads the
   word's value as an s16. */
extern s32 gStuntsDone;
extern s32 gRace;
extern s8 gNumActiveCars;

s32 are_all_cars_stopped(void)
{
    s16 i;
    s32 n;
    s32 result;
    s16 idx;

    if (gDeathMode != 0 && gNumPlayers >= 2 && gAPlayerReallyFinished != 0) {
        if (gDeathWaitEnd > 0.0f) {
            gDeathWaitEnd += gFrameSeconds;
            if (gDeathWaitEnd > 4.0f) {
                return 1;
            }
        } else {
            for (i = 0; i < gNumPlayers; i++) {
                if (gNumLaps == PC_MODEL_LAP(i)) {
                    gDeathWaitEnd = gFrameSeconds;
                }
            }
        }
        return 0;
    }
    result = 1;
    if (gRace == 2) {
        n = 1;
    } else {
        n = gNumActiveCars;
    }
    for (i = 0; i < n; i++) {
        idx = PC_MODEL_SLOT(i);
        if (PC_MODEL_DRONE_TYPE(idx) == 2) {
            if (gDeathMode == 0 || gNumLaps == PC_MODEL_LAP(idx)) {
                if (PC_MODEL_SPEED(idx) > 5.0f) {
                    result = 0;
                    break;
                }
            }
        }
    }
    if (result != 0 && gRace == 4) {
        return (s16)gStuntsDone;
    }
    return result;
}
