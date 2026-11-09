/* copy_tire_info.c: a tire's constants from the car's descriptor into the model (MAT3 initiali.c,
 * copy_tire_info()). Part of the car set-up (carsetup). */
#include "game/game.h"

/* A tire's constants in the car's physics descriptor (MAT3 anon_tradius_springK_18; _tCar.tires[i]). */
typedef struct TireDesc {
    f32 tradius;                /* +0x00 the radius, feet */
    f32 springK;                /* +0x04 the tire's spring rate */
    f32 rubdamp;                /* +0x08 the rubber's damping */
    f32 cstiff;                 /* +0x0C the cornering stiffness */
    f32 cfmax;                  /* +0x10 the most cornering force (a coefficient of friction) */
    f32 invmi;                  /* +0x14 the inverse moment of inertia */
} TireDesc;

/* A tire in the model record (MAT3 anon_tradius_springK_5c; tModelData.tires[i]): the same six first. */
typedef struct Tire {
    f32 tradius;
    f32 springK;
    f32 rubdamp;
    f32 cstiff;
    f32 cfmax;
    f32 invmi;
} Tire;

/* Copy the six constants of descriptor tire tp1 into model tire tp2. */
void copy_tire_info(TireDesc *tp1, Tire *tp2) {
    tp2->tradius = tp1->tradius;
    tp2->springK = tp1->springK;
    tp2->rubdamp = tp1->rubdamp;
    tp2->cstiff = tp1->cstiff;
    tp2->cfmax = tp1->cfmax;
    tp2->invmi = tp1->invmi;
}
