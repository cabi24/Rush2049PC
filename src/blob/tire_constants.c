/* tire_constants.c: a tire's model coefficients from its load (MAT3 initiali.c, tire_constants()).
 *
 * Part of the car set-up (carsetup), once per tire. The side force follows the Fiala brush model, a cubic in the
 * slip angle up to the full slide: F = Fz (k1 a - k2 a^2 + k3 a^3) for |a| < afmax (frictioncircle evaluates it).
 * The reciprocal coefficients are written as the N64 has them, (f32)(1.0 / 3.4), rounded to float from the double:
 * 1.0f / 3.4f is one unit in the last place off. */
#include "game/game.h"

/* A tire in the model record (MAT3 anon_tradius_springK_5c, tModelData.tires[i]). */
typedef struct Tire { f32 tradius,springK,rubdamp,cstiff,cfmax,invmi,zforce,afmax,k1,k2,k3,l2,l3,m1,m2,m3,m4,patchy; u8 tail48[20]; } Tire;
/* The model record (tModelData), the fields tire_constants reads. */
typedef struct tModelData { _tCar *car; Tune44 *tune; u8 pad8[0x398]; streetyp *lasttp[4]; u8 pad3B0[0x5C]; const s16 *torquecurve; u8 pad410[0x20]; Tire tires[4]; u8 pad5A0[0x5BC - 0x5A0]; f32 weight; u8 pad5C0[0x5C8 - 0x5C0]; f32 wheelbase; u8 tail5CC[0x648 - 0x5CC]; surftyp *movingSurface; u8 tail64C[0x808 - 0x64C]; } tModelData;

/* Tire tdes's coefficients on model m. otw is the other axle's distance from the centre of mass, so the tire's
 * static load is half its axle's share of the weight: zforce = weight * otw / wheelbase / 2. Then the Fiala terms
 * from the cornering stiffness cstiff and the friction cfmax: afmax = 3 cfmax zforce / cstiff (the slip angle of the
 * full slide), k1 = cstiff / zforce, k2 = k1^2 / (3 cfmax), k3 = k1^3 / (27 cfmax^2), and their slopes l2 = 2 k2,
 * l3 = 3 k3; m1 .. m4 the longitudinal slip's (m1 = 4.16 / cfmax, m2 = m1^2 / 3.4, m3 = m1^3 / 46.3,
 * m4 = 1 / 0.000055); patchy 0. */
void tire_constants(tModelData *m,Tire *tdes,f32 otw) {
 tdes->zforce = (m->weight * otw / m->wheelbase) * .5f;
 tdes->afmax = 3*tdes->cfmax*tdes->zforce/tdes->cstiff;
 tdes->k1 = tdes->cstiff/tdes->zforce;
 tdes->k2 = tdes->k1*tdes->k1/(3*tdes->cfmax);
 tdes->k3 = tdes->k1*tdes->k1*tdes->k1/(27*tdes->cfmax*tdes->cfmax);
 tdes->l2 = tdes->k2+tdes->k2;
 tdes->l3 = tdes->k3*3;
 tdes->m1 = 4.16f/tdes->cfmax;
 tdes->m2 = tdes->m1*tdes->m1*(f32)(1.0/3.4);
 tdes->m3 = tdes->m1*tdes->m1*tdes->m1*(f32)(1.0/46.3);
 tdes->m4 = (f32)(1.0/.000055);
 tdes->patchy = 0;
}
