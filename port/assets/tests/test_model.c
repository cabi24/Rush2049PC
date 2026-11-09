/* port/assets/tests/test_model.c: the model thread's m2c readers of a car's model record (MAT3 model[],
 * docs/port/CARS.md 4), through its car descriptor and tune pointers (at +0 and +8 on the PC: host pointers). calcaa
 * (body scale by the descriptor), transmission (the gear ratios from the tune and the descriptor) and enginetorque
 * (torque from the curve, scaled by the tune) read the record and the descriptor through their own views; each once
 * read the N64's 32-bit addresses where the record held host pointers, and the first race of the synthetic
 * cartridge faulted in each (docs/port/SYNTH.md). Here the record, the descriptor and the tune are host memory, as in
 * the game, and the fields are set where the host layout has them (MODEL_OFS, support.h).
 * game: src/blob/calcaa.c src/blob/transmission.c src/blob/enginetorque.c
 * game: src/blob/interp.c
 */
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

float carTransGearRatio[6 * 13];        /* per-car inertia factors: [car][gear + 1], 6 a row */
float carEngineTorque[13][3];         /* per-car torque scale: [car][mode] */

void calcaa(void *model, float *in, float *out);
void transmission(void *model);
int16_t enginetorque(void *model, int16_t rpm, int16_t throttle, const int16_t *curve);

static float F(uint8_t *p, unsigned off) { float f; memcpy(&f, p + off, 4); return f; }
static void SETF(uint8_t *p, unsigned off, float f) { memcpy(p + off, &f, 4); }

/* The physics descriptor (MAT3 _tCar, 0xB4 bytes on the N64) in its host layout: its four tire pointers at +0x60 are
 * 8 bytes each, so the words after them lie 0x10 further on (0xC8 bytes). */
static union { uint8_t b[0xC8]; void *align; } g_car;
static union { uint8_t b[sizeof(struct pc_model)]; struct pc_model m; } g_model;

int main(void)
{
    uint8_t *model = g_model.b, *car = g_car.b;
    float tune[11];

    memset(model, 0xEE, sizeof g_model);
    g_model.m.car = (void *)car;
    g_model.m.tune = (void *)tune;
    for (int i = 0; i < 45; i++)                /* descriptor word i = 100 + i, but the tire pointers (24..27) */
        if (i < 24 || i >= 28)
            SETF(car, 4 * i + (i >= 28 ? 0x10 : 0), 100.0f + i);
    for (int i = 0; i < 11; i++)
        tune[i] = 10.0f + i;                    /* tune word i = 10 + i */

    /* Body scale: out = in * descriptor words 3..5. */
    float in[3] = {1.0f, 2.0f, 3.0f}, out[3];
    calcaa(model, in, out);
    CHECK(out[0] == 103.0f && out[1] == 2 * 104.0f && out[2] == 3 * 105.0f);

    /* Ratios: transratio (+0x428) = tune word 3 x carTransGearRatio[car 2][gear 1 + 1]; totalratio (+0x42C) =
     * descriptor word 41 (dwratio) x that. */
    model[MODEL_OFS(9)] = 2;                                /* the transmission */
    *(int16_t *)(model + MODEL_OFS(0x3F4)) = 1;             /* gear */
    carTransGearRatio[2 * 6 + 1 + 1] = 0.5f;
    transmission(model);
    CHECK(F(model, MODEL_OFS(0x428)) == 13.0f * 0.5f);
    CHECK(F(model, MODEL_OFS(0x42C)) == 141.0f * 13.0f * 0.5f);

    /* Torque at the bottom of the curve: curve[0] x tune word 2 x carEngineTorque[car][mode]. */
    int16_t curve[12 * 10];
    for (int i = 0; i < 120; i++)
        curve[i] = 4;
    model[MODEL_OFS(0xB)] = 1, model[MODEL_OFS(0xC)] = 2;   /* mode, car */
    carEngineTorque[2][1] = 2.0f;
    CHECK(enginetorque(model, 0, 0, curve) == (int16_t)(4 * 12.0f * 2.0f));
    return TEST_DONE();
}
