/* include/game/carphys.h: the car's physics descriptors: a car type's descriptor (MAT3's _tCar), the handling tunes and
 * the tires' constants, which the model record (modeldata.h) points to for each car. The records the model record
 * holds, its tires (filled from those constants when a car is set up, carsetup) and its dead reckoning, are
 * modeldata.h's, beside it. */
#ifndef GAME_CARPHYS_H
#define GAME_CARPHYS_H

#include "game/types.h"
#include "game/modeldata.h"   /* Tire and tReckon */

/* A tire's constants (MAT3's anonymous tire record): frontTire[0] the front tires', frontTire[1] the rear's (MAT3
 * frontTire and backTire), which a physics descriptor's tires[] point at. copy_tire_info copies them into the model
 * record's tires (Tire, which begins with the same six). */
typedef struct TireDesc {
    f32 tradius;            /* the radius, feet */
    f32 springK;            /* the tire's spring rate */
    f32 rubdamp;            /* the rubber's damping */
    f32 cstiff;             /* the cornering stiffness */
    f32 cfmax;              /* the most grip, as a coefficient of friction (the friction circle's radius) */
    f32 invmi;              /* the wheel's inverse moment of inertia */
} TireDesc;

/* A car type's physics descriptor (MAT3 _tCar): car_descriptors[], which gCarModels[body] points at, and the model
 * record's car. Loaded from the game image; its tires[] are host pointers to frontTire's records, which the data
 * loader resolves (docs/port/CARS.md 4). */
typedef struct _tCar {
    f32 i[3];               /* the moments of inertia about x, y, z (the car's frame) */
    f32 invi[3];            /* their inverses (calcaa) */
    f32 springrate[4];      /* each corner's spring rate */
    f32 farspringrate;      /* the front anti-roll bar */
    f32 rarspringrate;      /* the rear one (no code reads it: forces1 gives every corner the front's) */
    f32 cdamping[4];        /* each corner's damping in compression */
    f32 rdamping[4];        /* and in rebound */
    f32 steerratio;         /* the steering ratio: steering-wheel angle over the road wheels' */
    f32 swtpg;              /* the trail's factor (steer_constants) */
    f32 srefpcybo2;         /* the air drag coefficient */
    f32 rollresist;         /* the rolling resistance */
    TireDesc *tires[4];     /* each tire's constants (frontTire's records) */
    f32 tireR[4][3];        /* each tire's position from the centre of mass, feet (the car's frame) */
    f32 enginvmi;           /* the engine's inverse moment of inertia */
    f32 dwratio;            /* the differential's ratio */
    f32 clutchmaxt;         /* the clutch's most torque */
    f32 upshiftangvel;      /* the automatic gearbox's shift points: up, the engine's radians a second */
    f32 downshiftangvel;    /* and down */
} _tCar;

/* A handling tune, an entry of gTune (MAT3's anonymous tune record): the factors the model applies to the car's
 * descriptor and parts for the handling chosen (the model record's tune, &gTune[handling]). MAT3's field names. */
typedef struct tTune {
    f32 diffbl;             /* how much of a pushing rear tire's own forward force a no-thrust differential keeps
                             * against the pair's mean (forces2) */
    f32 weight_scale;       /* the mass's (CalcMass) */
    f32 torque_scale;       /* the engine's torque (enginetorque) */
    f32 trans_scale;        /* the gear ratios (transmission, find_best_gear) */
    f32 clutchmaxtk;        /* the clutch's most torque (whatslips) */
    f32 shiftk;             /* the automatic gearbox's shift points (autoshift) */
    f32 fwdfk;              /* the driven tires' forward grip (dotireforce) */
    f32 sidefk;             /* how much of the driven tires' side grip they lose (dotireforce) */
    f32 skidk;              /* the force a skid starts at, a scale of the surface's force table (update_game_data) */
    f32 antispin;           /* the anti-spin moment (antispin) */
    f32 drift;              /* MAT3's; no code reads it (CalcDrift takes gTireDrift) */
} tTune;

/* Data */
/* The six physics descriptors (MAT3's basic, forward, backward, shortie, stretch and truck, in MAT3's order), which
 * gCarModels points at by car type; nothing names them but the car set-up's pc_car_setup_data. */
extern _tCar car_descriptors[EXTENT(6)];
extern _tCar *gCarModels[EXTENT(13)];
extern tTune gTune[EXTENT(3)];
extern TireDesc frontTire[EXTENT(2)];   /* MAT3 frontTire and backTire: the front and the rear tires' */

#endif /* GAME_CARPHYS_H */
