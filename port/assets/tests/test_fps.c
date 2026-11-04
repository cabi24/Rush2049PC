/* port/assets/tests/test_fps.c: game.frame_rate 60's follow camera (docs/port/FRAMERATE.md 7), through the game's own
 * func_800E8D50 (UpdateCarObj). It moves the camera pos = pos * e + target * (1 - e) once a frame, e being 0.6 x the
 * slot's elasticity for a frame of two retraces, scaled by the N64 for frames of 3, 4 and 5. At original every frame
 * must be the N64's expression to the bit, whatever the setting is (the game's copy latches it). At 60 two frames of
 * one retrace must follow a car as one frame of two does: to a target that stands still, exactly e of the way left
 * (sqrt(e) twice), and to a moving car within what the finer steps change; frames of 2-5 retraces at 60 are the N64's.
 * The frame's steps come from port/config/game_fps.c, as in the game (pc_game_frame_note, pc_fps_filter).
 * game: src/blob/func_800E8D50.c
 */
#include <math.h>
#include <string.h>

#include "pc_game_fps.h"
#include "pc_game_types.h"
#include "support.h"

void pc_game_fps_set(int fps);

/* What func_800E8D50 reads and writes. A camera slot is 152 bytes: the basis at +96, the position at +132. */
typedef struct { unsigned char other[96]; float matrix[3][3]; float position[3]; unsigned char tail[8]; } Slot152;
Slot152 gCamera[4];
float D_80152708[4], D_80152720[4];
volatile int32_t D_8002EB98;
struct pc_model pc_models[PC_MODEL_SLOTS];
typedef struct { unsigned char other[0x110]; struct pc_visual visuals[21]; unsigned char other_308[0x35B - 0x308];
                 signed char car, slot; } Object;

void func_800E8CB8(void *object, void *pos, void *matrix) { (void)object, (void)pos, (void)matrix; }
void vector_normalize_length(float *v, float m[3][3]) { (void)v, (void)m; }
void func_800E8D50(Object *object, float *carpos, float (*matrix)[3][3], float *res);

static Object car;

/* One frame of `retraces` retraces at the setting in effect: the game's viUpdateTime notes it, then the camera runs. */
static void frame(int retraces, const float *carpos)
{
    float res[3] = {0.0f, 4.0f, -20.0f};
    D_8002EB98 = retraces;
    pc_game_frame_note(retraces);
    func_800E8D50(&car, (float *)carpos, NULL, res);
}

/* The N64's frame, written out: what the game computes at original. */
static float n64_frame(float pos, float target, int retraces, float elastic)
{
    float e = elastic * 0.6f;
    if (retraces == 3) e *= 0.75f;
    else if (retraces == 4) e *= 0.6f;
    else if (retraces == 5) e *= 0.5f;
    return pos * e + target * (1.0f - e);
}

int main(void)
{
    static const float elastic[] = {1.0f, 0.5f, 0.85f};
    car.car = 0;
    car.slot = 1;
    pc_models[0].moving_state = -1;   /* not resurrecting: the camera follows */

    for (size_t k = 0; k < sizeof elastic / sizeof *elastic; k++) {
        D_80152720[1] = elastic[k];
        /* original, the setting at 60 but not latched: the N64's frames to the bit, for 1-5 retraces */
        pc_game_fps_set(60);
        for (int r = 1; r <= 5; r++) {
            float carpos[3] = {100.0f, 0.0f, 50.0f};
            gCamera[1].position[0] = 10.0f, gCamera[1].position[1] = 2.0f, gCamera[1].position[2] = -7.0f;
            float want = n64_frame(10.0f, 100.0f + 0.0f, r, elastic[k]);
            frame(r, carpos);
            CHECK(pc_fps60 == 0 && gCamera[1].position[0] == want);
        }
        /* 60, latched: a car standing still, one frame of two at original against two of one at 60 */
        pc_game_fps_begin();
        CHECK(pc_fps60 == 1);
        {
            float carpos[3] = {100.0f, 0.0f, 50.0f};
            gCamera[1].position[0] = 10.0f;
            frame(1, carpos);
            frame(1, carpos);
            float want = n64_frame(10.0f, 100.0f, 2, elastic[k]);
            CHECK(fabsf(gCamera[1].position[0] - want) < 1e-4f);
            /* frames of 2-5 retraces at 60: the N64's */
            for (int r = 2; r <= 5; r++) {
                gCamera[1].position[0] = 10.0f;
                frame(r, carpos);
                CHECK(gCamera[1].position[0] == n64_frame(10.0f, 100.0f, r, elastic[k]));
            }
        }
        /* a car moving at 60 feet a second along x for 2 s: the camera's lag behind it, at 30 frames a second
         * (original) against 60 (game.frame_rate 60) */
        {
            float lag30, lag60, carpos[3] = {0.0f, 0.0f, 0.0f};
            pc_game_fps_set(0);
            pc_game_fps_begin();
            gCamera[1].position[0] = 0.0f;
            for (int i = 1; i <= 60; i++) {
                carpos[0] = 60.0f * i * 2 / 60.0f;
                frame(2, carpos);
            }
            lag30 = carpos[0] - gCamera[1].position[0];
            pc_game_fps_set(60);
            pc_game_fps_begin();
            gCamera[1].position[0] = 0.0f;
            for (int i = 1; i <= 120; i++) {
                carpos[0] = 60.0f * i / 60.0f;
                frame(1, carpos);
            }
            lag60 = carpos[0] - gCamera[1].position[0];
            /* the steady lag behind a car at speed v is v x T x e / (1 - e) for frames of T: per retrace at 60
             * sqrt(e) / (1 - sqrt(e)) of a retrace's move, against e / (1 - e) of two retraces' at 30 */
            float e = elastic[k] * 0.6f, s = sqrtf(e);
            float want30 = 2.0f * e / (1.0f - e), want60 = s / (1.0f - s);
            fprintf(stderr, "elasticity %.2f: lag behind the car %.3f ft at 30, %.3f at 60 (steady %.3f, %.3f)\n",
                    (double)elastic[k], (double)lag30, (double)lag60, (double)want30, (double)want60);
            CHECK(fabsf(lag30 - want30) < 1e-2f && fabsf(lag60 - want60) < 1e-2f);
            CHECK(fabsf(lag60 - lag30) <= 0.5f * (60.0f / 30.0f));   /* within half the car's move in a 30 Hz frame */
        }
        pc_game_fps_set(0);
        pc_game_fps_begin();
    }
    /* resurrecting: the camera holds its place, at either rate */
    pc_models[0].moving_state = 0;
    pc_game_fps_set(60);
    pc_game_fps_begin();
    {
        float carpos[3] = {500.0f, 0.0f, 0.0f};
        gCamera[1].position[0] = 10.0f;
        frame(1, carpos);
        CHECK(D_80152708[1] == 0.0f && gCamera[1].position[0] == 500.0f);
    }
    return TEST_DONE();
}
