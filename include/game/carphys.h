/* include/game/carphys.h: the car's physics descriptors: a car type's descriptor (MAT3's _tCar), the handling tunes and
 * the tires' constants, which the model record (physics.h) points to for each car; and two records the model record
 * holds: its tires, filled from those constants when a car is set up (carsetup), and its dead reckoning. */
#ifndef GAME_CARPHYS_H
#define GAME_CARPHYS_H

#include "game/types.h"

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
/* wave 4: the model record's views in the physics files (physics.h's tModelData, agent A1) still name the tune by its
 * old name; this goes with them at the merge. */
typedef tTune Tune44;

/* A tire of the model record (MAT3's anonymous tire state, tModelData.tires[4]: the front two, then the rear, the
 * driven pair). Its first six are the TireDesc's constants (copy_tire_info), then the brush model's coefficients for
 * the tire's load (tire_constants), then what the tire model keeps from step to step (frictioncircle, dotireforce).
 * MAT3's field names. */
typedef struct Tire {
    f32 tradius;            /* the radius, feet */
    f32 springK;            /* the tire's spring rate: the contact patch's sideways spring */
    f32 rubdamp;            /* the rubber's damping */
    f32 cstiff;             /* the cornering stiffness */
    f32 cfmax;              /* the most grip, as a coefficient of friction */
    f32 invmi;              /* the wheel's inverse moment of inertia */
    f32 zforce;             /* the static load: half the axle's share of the weight */
    f32 afmax;              /* the slip angle of the full slide: 3 cfmax zforce / cstiff */
    f32 k1, k2, k3;         /* the side force's cubic in the slip angle: F = Fz (k1 a - k2 a^2 + k3 a^3) */
    f32 l2, l3;             /* its slopes' terms, 2 k2 and 3 k3 (MAT3's; no code reads them) */
    f32 m1, m2, m3, m4;     /* the forward slip's (MAT3's; no code reads them) */
    f32 patchy;             /* the contact patch's sideways deflection, feet */
    f32 angvel;             /* the wheel's spin, radians a second */
    f32 sliptorque;         /* the torque the road takes from the wheel this step */
    f32 sideforce;          /* the last side force */
    f32 traction;           /* the last forward force */
    s8 slipflag;            /* which case the friction circle took: 3 in the air, 10/20 wheelspin, 30/40 locking ...
                             * (frictioncircle.c); a front tire at 40 cuts a drone's gas and brake (AdjustSpeed) */
} Tire;

/* The model record's dead reckoning (MAT3's anonymous reckon record, tModelData.reckon): what the rest of the game sees
 * of a car between the model's steps. The model thread takes a snapshot each step (dead_reckon_posuv, the base from
 * update_reckon_base); the game thread moves the position on to the frame's time (game_reckon_all). MAT3's names. */
typedef struct tReckon {
    f32 base_rwa[3];        /* the world acceleration, velocity and position at the last base update */
    f32 base_rwv[3];
    f32 base_rwr[3];
    s16 rwmph;              /* the speed shown: mph times 4 */
    f32 suscomp[4];         /* each tire's suspension compression */
    f32 airdist[4];         /* each tire's height above the road */
    f32 rwa[3];             /* the acceleration, velocity and position to draw */
    f32 rwv[3];
    f32 rwr[3];
    f32 uvs[3][3];          /* the orientation to draw */
} tReckon;

/* Data */
/* The six physics descriptors (MAT3's basic, forward, backward, shortie, stretch and truck, in MAT3's order), which
 * gCarModels points at by car type; nothing names them but the car set-up's pc_car_setup_data. */
extern _tCar car_descriptors[EXTENT(6)];
extern _tCar *gCarModels[EXTENT(13)];
extern tTune gTune[EXTENT(3)];
extern TireDesc frontTire[EXTENT(2)];   /* MAT3 frontTire and backTire: the front and the rear tires' */

#endif /* GAME_CARPHYS_H */
