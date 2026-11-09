/* port/assets/tests/test_stuntend.c: the end of a stunt round (docs/port/QA.md 4), through the game's own
 * are_all_cars_stopped, which the race end asks whether the race may end now. When a stunt round's time is up and the cars
 * have settled, CheckForStunt sets gStuntsDone, the s32 gStuntsDone, to 1, and are_all_cars_stopped answers with
 * the word's low half, which the N64 read as the halfword at +2. On a little-endian host the halfword at +2 is the
 * high one, 0, so the answer was always 0 and a stunt round never ended: the PC reads the word as an s16.
 * game: src/blob/are_all_cars_stopped.c
 */
#include <string.h>

#include "pc_game_types.h"
#include "support.h"

int32_t gStuntsDone __attribute__((aligned(8)));   /* gStuntsDone */

int8_t gDeathMode;              /* deaths mode */
int16_t gNumPlayers = 1;
int32_t gAPlayerReallyFinished;
float gDeathWaitEnd, gFrameSeconds = 1.0f / 30;
struct pc_model pc_models[PC_MODEL_SLOTS];    /* the model records (pc_game_types.h) */
#define model ((uint8_t *)pc_models)
int16_t gNumLaps;
int32_t gRace;
int8_t gNumActiveCars;              /* the cars in the race */

int32_t are_all_cars_stopped(void);

int main(void)
{
    /* One car, settled: its record names itself (+0x7C6), it is in the race (+0x7CC = 2) at no speed (+0x3F0). */
    int16_t self = 0;
    int8_t racing = 2;
    float speed = 0.0f;
    memcpy(model + MODEL_OFS(0x7C6), &self, 2);
    memcpy(model + MODEL_OFS(0x7CC), &racing, 1);
    memcpy(model + MODEL_OFS(0x3F0), &speed, 4);
    gNumActiveCars = 1;
    gRace = 4;

    gStuntsDone = 0;
    CHECK(are_all_cars_stopped() == 0);   /* the stunts not done: go on */
    gStuntsDone = 1;
    CHECK(are_all_cars_stopped() == 1);   /* done: the round ends */

    speed = 20.0f;                 /* a car still moving: go on, whatever the flag */
    memcpy(model + MODEL_OFS(0x3F0), &speed, 4);
    CHECK(are_all_cars_stopped() == 0);

    gRace = 0;             /* not stunt mode: the settled cars alone decide */
    speed = 0.0f;
    memcpy(model + MODEL_OFS(0x3F0), &speed, 4);
    gStuntsDone = 0;
    CHECK(are_all_cars_stopped() == 1);
    return TEST_DONE();
}
