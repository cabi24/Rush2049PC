/* include/game/modeldata.h: the model record (MAT3 tModelData), one per car slot, the three records it holds (its
 * tires, its respawn and its dead reckoning), and the records themselves (pc_models). physics.h, carphys.h and track.h
 * include it; so does the port's code that reads the records (the state log, port/main.c, and
 * the state hash, port/ultra/statehash.c), which doesn't see the rest of the game's headers: the record needs only
 * the base types (basic.h), and PC_MODEL_SLOTS, which port/include/pc_game_types.h defines (the prelude includes it
 * for the game's files). */
#ifndef GAME_MODELDATA_H
#define GAME_MODELDATA_H

#include "game/basic.h"

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

/* A car's respawn (MAT3's anonymous resurrect record, tModelData.resurrect): where it is put back after a crash or an
 * abort, or on the grid at the start, and its flight there (SetResurrectionData, Resurrect, UpdateResurrectingCar,
 * FinishResurrecting: docs/code/race.md). MAT3's names. */
typedef struct tResurrect {
    f32 pos[3];             /* the respawn point */
    f32 save_pos[3];        /* the point it is worked out from: the grid's at the start, else where the car crashed */
    f32 uvs[3][3];          /* the orientation at the respawn point (at save_pos until Resurrect moves it) */
    f32 quat_start[4];      /* the orientation the flight leaves with, as a quaternion */
    f32 quat_end[4];        /* and the one it arrives with */
    s32 velocity;           /* the speed it arrives with, feet a second */
    f32 crash_time;         /* the model time of the crash, 0 at the race start */
    s16 moving_state;       /* RES_IDLE, RES_START, then the frames in flight, RES_DONE (physics.h) */
    f32 resurrect_time;     /* the model time the flight began */
    s8 autoAbort;           /* the game aborted the car (a drone left behind or stuck: DetectDroneAbort), not a crash */
    s8 waterAbort;          /* the road under it aborted the car (TireSurf: road codes 3 and 7), so a human isn't
                             * respawned in place */
} tResurrect;

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

/* The model record (MAT3 tModelData): one per car slot, pc_models[slot], the car's simulation. The model thread steps
 * it (model_iteration: syminit, then sym and regular each step: forces, torques, accelerations, velocities,
 * positions, the road and the collisions); the game thread sets its controls (mainin), reads its dead-reckoned place
 * (reckon) for drawing, and the cameras, effects, sounds and HUD read the rest. Units are feet, seconds and radians.
 * "The car's frame" has x across the car, y up and z forward; "the world" is the track's frame, y up. The fields and
 * their order are MAT3's, and their names but for color (the N64 game keeps three paint colours where MAT3 has
 * PaintJob and an unused short).
 *
 * The vectors and matrices are float arrays, as MAT3 declares them (wave 4: B2's vector and matrix types). The tires'
 * states, the respawn and the dead reckoning are records of their own (anonymous in MAT3): Tire, tResurrect and
 * tReckon, above. The record has no pointer but its five (car, tune, lasttp, torquecurve, movingSurface), and every
 * pointee is game data or the road file. */
typedef struct tModelData {
    /* The car and its owner's choices (init_cars, carsetup) */
    struct _tCar *car;          /* the physics descriptor: car_descriptors[], as gCarModels[body_type] */
    struct tTune *tune;         /* its handling tune: &gTune[handling] */
    u8 body_type;               /* the car, 0-12 (the roster's car) */
    s8 trans;                   /* the transmission choice (the input record's; bit 0 manual) */
    s8 autotrans;               /* automatic transmission: (trans & 1) ^ 1 */
    s8 handling;                /* the handling choice, a row of gTune */
    s8 engine_type;             /* the engine choice (a drone's: its slot & 3) */
    s8 tire_type;               /* the tire choice */
    s8 frame;                   /* the frame choice: the durability (carDurability) */
    s8 tireRims;                /* the rims (gTireRims) */

    /* Forces and motion in the car's frame (forces1, forces2, torques, accelerations, velocities) */
    f32 f[3];                   /* the total force; wave 4: Vec3 */
    f32 m[3];                   /* the total moment; wave 4: Vec3 */
    f32 a[3];                   /* the acceleration, feet a second squared; wave 4: Vec3 */
    f32 aa[3];                  /* the angular acceleration, radians a second squared; wave 4: Vec3 */
    f32 v[3];                   /* the velocity, feet a second (v[2] the forward speed); wave 4: Vec3 */
    f32 w[3];                   /* the angular velocity, radians a second (w[1] the yaw rate); wave 4: Vec3 */
    f32 d[3];                   /* the drag and the wind: [0] across, [1] up, [2] along; wave 4: Vec3 */
    f32 tireForce[4][3];        /* each tire's force; [i][1] its load, the road pushing up; wave 4: Vec3 */
    f32 tireV[4][3];            /* each tire's contact patch's velocity; wave 4: Vec3 */
    f32 bodyForce[4][3];        /* the force at each body corner, where the body touches the world */
    f32 bodyR[4][3];            /* the body's collision corners from the centre, [0] front right; wave 4: Vec3 */
    f32 centerForce[3];         /* the force at the centre of mass this step (collisions ...); wave 4: Vec3 */
    f32 centerForceLast[3];     /* the last step's (sym) */
    f32 centerMoment[3];        /* the moment about the centre of mass; wave 4: Vec3 */
    f32 peak_body_force[2][3];  /* the body's largest forces since the cameras last read them: +x +z, -x -z */
    f32 peak_center_force[2][3]; /* the centre's (the cameras' shake, the bump sounds) */
    f32 tireuvs[4][3][3];       /* each tire's orientation; wave 4: Basis */
    f32 g[3];                   /* gravity; wave 4: Vec3 */

    /* Motion in the world (positions) */
    f32 rwa[3];                 /* the acceleration */
    f32 rwv[3];                 /* the velocity, feet a second */
    f32 rwr[3];                 /* the position, feet; wave 4: Vec3 */
    f32 rwr2[3];                /* the position a step ago; wave 4: Vec3 */
    f32 tireRwr[4][3];          /* each tire's position */
    f32 bodyrwr[4][3];          /* each body corner's position */
    f32 bodyrwr2[4][3];         /* the same a step ago */
    f32 grw[3];                 /* gravity: (0, -weight, 0); wave 4: Vec3 */
    f32 stuntW[3];              /* the rotation summed for the stunt score */
    f32 uvs[3][3];              /* the orientation, rows the car's x, y, z axes (uvs[1][1] 1 upright); wave 4: Basis */
    f32 roaduvs[4][3][3];       /* the road's orientation under each tire; wave 4: Basis */
    struct streetyp *lasttp[4]; /* each tire's last node of the road's surface tree, where TireSurf starts */

    /* The steering, the pedals as the simulation sees them, the engine and the drivetrain */
    f32 steerangle;             /* the front wheels' angle, radians */
    f32 torque[4];              /* each wheel's drive torque */
    f32 steergain;              /* the road wheels' angle at full lock, radians (steer_constants) */
    f32 trail;                  /* the steering's trail (steer_constants) */
    f32 clutch;                 /* 1 disengaged (pedal down), 0 engaged (SetClutch) */
    f32 throttle;               /* 0-1 */
    f32 brake;                  /* 0-1 */
    f32 brakeroll;              /* set to 3 by initialize, read by nothing */
    f32 brakegain[4];           /* each tire's braking force at full brake */
    f32 pitchangle;             /* cleared by initialize, read by nothing */
    f32 magvel;                 /* the speed, feet a second */
    s16 gear;                   /* the gear: -1 reverse, 0 neutral, 1-4 */
    s16 commandgear;            /* the gear the driver asks for */
    s16 wing;                   /* the wing deployed */
    s16 isCollideWithWall;      /* no code reads it (MAT3's name) */
    s32 wingSnd;                /* the wing's sound, -1 none (AnimateWing) */
    f32 catchup;                /* the catch-up's scale of the car's time step (SetCatchup) */
    f32 engtorque;              /* the engine's torque */
    f32 engangvel;              /* the engine's speed, radians a second */
    const s16 *torquecurve;     /* the engine's torque curve: stdtorquecurve */
    f32 dwtorque;               /* the torque at the driven wheels (whatslips) */
    f32 dwinvmi;                /* 1 / the driveline's moment of inertia (half a rear tire's) */
    f32 efdwinvmi;              /* the same with the engine's, through the clutch when it holds */
    f32 dwangvel;               /* the driven wheels' speed, radians a second */
    f32 clutchtorque;           /* the torque through the clutch */
    f32 clutchangvel;           /* the clutch's speed, radians a second */
    f32 transratio;             /* the gearbox's ratio */
    f32 totalratio;             /* the gearbox's and the differential's */

    /* The tires (Tire), set from the descriptor's tire constants (copy_tire_info, tire_constants) and stepped by the
     * tire forces (dotireforce, frictioncircle) */
    Tire tires[4];
    u16 tiresli[4];             /* each tire's road surface last found (an index into stree_surf; TireSurf) */
    s8 magicdiff;               /* the "magic" (limited-slip) differential (gCarDifferential bit 0) */
    s8 nothrusttorque;          /* average the rear tires' forward force (gCarDifferential bit 1) */
    f32 drift;                  /* the drift factor (CalcDrift) */
    f32 sidefk;                 /* the tires' side grip (CalcTireSideForce) */
    f32 offroadtires;           /* the tires' off-road factor (CalcOffroadTires) */
    f32 wingEffect;             /* the wing's size (gWingSizes) */
    f32 weight;                 /* the weight, a force: mass times gravity */
    f32 mass;                   /* the mass (CalcMass) */
    f32 collisionmass;          /* the mass in car-to-car collisions */
    f32 wheelbase;              /* front tire to rear, along the car */

    /* The suspension and the road under each tire (road, TireSurf) */
    f32 suscomp[4];             /* each suspension's compression, feet (negative: the tire in the air) */
    f32 tpcomp[4];              /* the same, at least 0 */
    f32 airdist[4];             /* each tire's height above the road (0 or less: touching) */
    f32 airvel[4];              /* its rate of change, feet a second */
    s32 roadcode[4];            /* each tire's road code (ROADCODE_*: its road polygon's flags & 15) */
    u16 sviscode[4];            /* each tire's surface as the visuals, sounds and skids see it (SURF_VIS_*) */
    s16 roadboost[4];           /* each tire's boost (the surface's bits 8-11, negative with 0x8000) */
    u16 sound_flags[4];         /* each tire's surface's sound (snd_flags) */
    f32 dt;                     /* the model step, seconds */
    f32 idt;                    /* 1 / dt, 0 while paused */

    /* Crashes, thumps and the moving surface under the car */
    f32 crashthreshold;         /* the impact that wrecks the car */
    s8 crashflag;               /* wrecked: no shifting, not collidable */
    s8 topscrape;               /* the roof scraping */
    s8 thumpflag;               /* the hardest suspension thump this step, 0-3 */
    s8 mthumpflag;              /* the last thump's, for the bump sounds */
    s16 thump_side;             /* the tires that thumped, a bit each */
    struct surftyp *movingSurface; /* the moving surface the car is on, or NULL (TireSurf; MovingSurfaceForce) */
    f32 thumpthreshold;         /* the body force a thump takes */
    f32 bodyForces;             /* the size of the body's contact force (velocities damps the spin by it) */
    f32 colrad;                 /* the radius of the sphere around the car, feet */
    f32 shifttime;              /* the model time before which no other shift happens */
    s16 bogState;               /* the engine bog's sound: 1 start it, 3 stop it */

    /* The respawn (tResurrect): Resurrect flies the car from where it stopped to the point it respawns at */
    tResurrect resurrect;

    /* Where and how the car starts (syminit): its grid place or respawn point */
    struct {
        s16 polepos;            /* the start place (PC_RESPAWN_BASE and up: a respawn) */
        f32 pos[3];             /* the position */
        f32 uvs[3][3];          /* the orientation */
        f32 offset[3];          /* the grid row's offset from the start point */
    } initin;

    /* The model thread's clock for the car (model_iteration) */
    struct {
        s32 modelIteration;     /* the steps run */
        f32 modelTime;          /* the race clock, seconds */
        f32 dt;                 /* the step, seconds, kept across a reset */
        s16 modelrun;           /* the model thread steps this car */
    } fastin;

    /* The controls, which the game thread sets from the player's input (model_controls_input) or the drone code
     * (maxpath) and the model thread reads */
    struct {
        f32 wheel;              /* the steering, -1 to 1 */
        f32 clutch;             /* 1 disengaged, 0 engaged */
        f32 brake;              /* 0-1, in fifteenths for a player */
        f32 throttle;           /* 0-1, in fifteenths for a player */
        s8 gear;                /* the gear asked for: -1 reverse, 1-4 */
        s8 wing;                /* the wing button held */
        s8 abort;               /* the abort button held */
        s8 unused;
    } mainin;

    /* The dead reckoning (tReckon): the place the game thread draws the car at between model steps, extrapolated
     * from the last step's (game_reckon_all) */
    tReckon reckon;

    /* The race */
    s16 obResPt;                /* the respawn path's high-water mark (GetResPt) */
    s16 slot;                   /* the car slot: the roster's, the car record's index */
    s16 in_game;                /* in the race */
    s16 we_control;             /* this machine drives it: a drone of ours */
    s8 drone_type;              /* DRONE_TYPE_* */
    u8 color[3];                /* the three paint colours (the roster's) */
    s16 rpm;                    /* the engine's speed for the sounds and the HUD, rpm (negative in reverse) */
    u32 appearance;             /* APPEAR_* (cars.h): copied each frame to the car record's */
    f32 stuck_time;             /* when the car became stuck, 0 not stuck (DetectDroneAbort) */
    s8 started;                 /* it has passed the start checkpoint, so its laps count */
    s8 data_valid;              /* the record is filled in */
    s8 can_we_cheat;            /* the drone may cheat (maxpath) */
    s8 hide_car;                /* hidden, during the respawn's flight */
    s16 please_abort_me;        /* the drone code asks for the car to be put back on the road (DetectDroneAbort) */
    s16 last_cp_index;          /* the last checkpoint it passed */
    s16 next_cp_index;          /* the next one */
    s16 drone_target;           /* the car a drone paces itself against (the drones' catch-up) */
    s8 laps;                    /* laps done */
    s8 cpSign;                  /* the side of the next checkpoint's plane the car was on last frame */
    s8 collidable;              /* the other cars collide with it */
    s8 solid;                   /* solid: 0 for a while after a respawn */
    f32 drone_scale;            /* a drone's speed scale (the catch-up) */
    f32 time_boost;             /* a drone's boost of its time step (the drones' catch-up; 1 for a human) */
    f32 time_fudge;             /* this step's time scale: catchup x time_boost x the road's boost (UpdateModel) */
    f32 skidIntensity[4];       /* each tire's skid sound's level */
} tModelData;

/* The model records: one per car slot (pc_game_types.h's PC_MODEL_SLOTS: 6 on the N64, PC_MAX_LINKS here; the game
 * activates gNumCars of them), PC data with no N64 address (docs/port/DECOUPLE.md 3) */
extern tModelData pc_models[PC_MODEL_SLOTS];

#endif /* GAME_MODELDATA_H */
