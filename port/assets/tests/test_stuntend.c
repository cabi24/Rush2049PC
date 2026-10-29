/* port/assets/tests/test_stuntend.c: the end of a stunt round (docs/port/QA.md 4), through the game's own
 * func_800E7DD0, which the race end asks whether the race may end now. When a stunt round's time is up and the cars
 * have settled, camera_scene_manager sets gStuntsDone, the s32 D_80152738, to 1, and func_800E7DD0 answers with
 * D_8015273A, the halfword at +2: the word's low half on the N64. Here the two names overlap as they do in the
 * game's arena. On a little-endian host the halfword at +2 is the high one, 0, so the answer was always 0 and a
 * stunt round never ended.
 * game: src/blob/func_800E7DD0.c
 */
#include <string.h>

#include "pc_game_types.h"
#include "support.h"

int32_t D_80152738 __attribute__((aligned(8)));   /* gStuntsDone */
__asm__(".globl D_8015273A\n.set D_8015273A, D_80152738 + 2");

int8_t D_80142760;              /* deaths mode */
int16_t active_player_count = 1;
int32_t D_80143F10;
float D_801525F4, D_8002EB94 = 1.0f / 30;
struct pc_model pc_models[PC_MODEL_SLOTS];    /* the model records (pc_game_types.h), N64 layout */
#define D_8014A250 ((uint8_t *)pc_models)
int16_t D_80152734;
int32_t gameplay_mode;
int8_t D_80152744;              /* the cars in the race */

int32_t func_800E7DD0(void);

int main(void)
{
    /* One car, settled: its record names itself (+0x7C6), it is in the race (+0x7CC = 2) at no speed (+0x3F0). */
    int16_t self = 0;
    int8_t racing = 2;
    float speed = 0.0f;
    memcpy(D_8014A250 + 0x7C6, &self, 2);
    memcpy(D_8014A250 + 0x7CC, &racing, 1);
    memcpy(D_8014A250 + 0x3F0, &speed, 4);
    D_80152744 = 1;
    gameplay_mode = 4;

    D_80152738 = 0;
    CHECK(func_800E7DD0() == 0);   /* the stunts not done: go on */
    D_80152738 = 1;
    CHECK(func_800E7DD0() == 1);   /* done: the round ends */

    speed = 20.0f;                 /* a car still moving: go on, whatever the flag */
    memcpy(D_8014A250 + 0x3F0, &speed, 4);
    CHECK(func_800E7DD0() == 0);

    gameplay_mode = 0;             /* not stunt mode: the settled cars alone decide */
    speed = 0.0f;
    memcpy(D_8014A250 + 0x3F0, &speed, 4);
    D_80152738 = 0;
    CHECK(func_800E7DD0() == 1);
    return TEST_DONE();
}
