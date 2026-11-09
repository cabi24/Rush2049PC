/* port/assets/tests/test_model.c: the model thread's m2c readers of a car's model record (MAT3 model[],
 * docs/port/CARS.md 2.2), which keeps its N64 layout on the PC: its car descriptor and tune pointers at +0 and +4 are
 * u32 window addresses. calcaa (body scale by the descriptor), transmission (engine inertia from the tune and the
 * descriptor) and enginetorque (torque from the curve, scaled by the tune) read them as 8-byte host pointers before
 * (docs/port/SYNTH.md): the first race of the synthetic cartridge faulted in each. The record, the descriptor and
 * the tune are in an RDRAM window the test maps (exit 77, skipped, if it can't), as the game's are.
 * game: src/blob/calcaa.c src/blob/transmission.c src/blob/enginetorque.c
 * game: src/blob/interp.c
 */
#include <string.h>

#include "support.h"

#define W32(p) ((uint32_t)(uintptr_t)(p))

float carTransGearRatio[6 * 13];        /* per-car inertia factors: [car][gear + 1], 6 a row */
float carEngineTorque[13][3];         /* per-car torque scale: [car][mode] */

void calcaa(void **model, float *in, float *out);
void transmission(int8_t *model);
int16_t enginetorque(void *model, int16_t rpm, int16_t throttle, int16_t *curve);

static float F(uint8_t *p, unsigned off) { float f; memcpy(&f, p + off, 4); return f; }
static void SETF(uint8_t *p, unsigned off, float f) { memcpy(p + off, &f, 4); }

int main(void)
{
    uint8_t *w = test_rdram();
    if (!w)
        return 77;
    uint8_t *model = w + 0x100000, *car = w + 0x110000, *tune = w + 0x120000;
    memset(model, 0xEE, 0x808);
    *(uint32_t *)(model + 0) = W32(car);        /* the descriptor: a u32 window address */
    *(uint32_t *)(model + 4) = W32(tune);
    for (int i = 0; i < 45; i++)
        SETF(car, 4 * i, 100.0f + i);           /* descriptor word i = 100 + i */
    for (int i = 0; i < 11; i++)
        SETF(tune, 4 * i, 10.0f + i);           /* tune word i = 10 + i */

    /* Body scale: out = in * descriptor words 3..5. */
    float in[3] = {1.0f, 2.0f, 3.0f}, out[3];
    calcaa((void **)model, in, out);
    CHECK(out[0] == 103.0f && out[1] == 2 * 104.0f && out[2] == 3 * 105.0f);

    /* Inertia: +0x428 = tune word 3 x carTransGearRatio[car 2][gear 1 + 1]; +0x42C = descriptor word 41 x that. */
    model[9] = 2;                               /* car type */
    *(int16_t *)(model + 0x3F4) = 1;            /* gear */
    carTransGearRatio[2 * 6 + 1 + 1] = 0.5f;
    transmission((int8_t *)model);
    CHECK(F(model, 0x428) == 13.0f * 0.5f);
    CHECK(F(model, 0x42C) == 141.0f * 13.0f * 0.5f);

    /* Torque at the bottom of the curve: curve[0] x tune word 2 x carEngineTorque[car][mode]. */
    int16_t curve[12 * 10];
    for (int i = 0; i < 120; i++)
        curve[i] = 4;
    model[0xB] = 1, model[0xC] = 2;             /* mode, car */
    carEngineTorque[2][1] = 2.0f;
    CHECK(enginetorque(model, 0, 0, curve) == (int16_t)(4 * 12.0f * 2.0f));
    return TEST_DONE();
}
