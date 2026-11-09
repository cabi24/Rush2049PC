/* include/game/physics.h: the car simulation: collision, the road, the drivetrain, tyres, respawning */
#ifndef GAME_PHYSICS_H
#define GAME_PHYSICS_H

#include "game/types.h"

/* Constants: the names of MAT3's rush_flags.h, the same values.
 * World units are feet, speeds feet a second, Y is up. */

#define MPH_TO_FTPS          1.4666667f /* miles an hour to feet a second: 88 ft/s = 60 mph */

/* tCarData.crash_state (CheckCrash) */
#define CRASH_NONE           0
#define CRASH_WRECKED        1      /* the wreck on show; after 3.5 s (2.25 s in battle) the car respawns */
#define CRASH_RESPAWN        2      /* UpdateModel starts the car again at its respawn point */

/* tCarData.dead. Only death mode ("last car standing") uses CAR_DYING and CAR_DEAD. */
#define CAR_ABORTED          (-1)   /* aborted, or respawned outside death mode; 0 again at the next start */
#define CAR_ALIVE            0
#define CAR_DYING            1      /* out of the race, coasting to a stop */
#define CAR_DEAD             2      /* stopped: no physics, only the collisions */

/* tModelData.resurrect.moving_state: the respawn's flight. Positive values count frames in flight. */
#define RES_IDLE             (-1)   /* not respawning */
#define RES_START            0      /* Resurrect set a flight up; its first frame */
#define RES_DONE             (-2)   /* the flight is over: FinishResurrecting drops the car in */

/* The mode of Init_MDrive, multiinit and SetResurrectionData */
#define INIT_MODE_START      0      /* the race start: the grid place, the respawn point the last checkpoint */
#define INIT_MODE_KEEP       1      /* SetResurrectionData leaves everything as it is */
#define INIT_MODE_CRASH      2      /* Init_MDrive: respawn the car; SetResurrectionData: keep the crash point and
                                     * time to respawn from */

/* tModelData.sviscode[tire]: the surface under each tire, as the visuals, sounds and skids see it */
#define SURF_VIS_PAVEMENT    0
#define SURF_VIS_DIRT        1
#define SURF_VIS_WATER       2      /* the sounds treat 3 as water too */
#define SURF_VIS_AIRBORNE    8      /* the tire touches nothing (roadcode is 8 then too) */

/* tModelData.roadcode[tire]: the road code under each tire (its road polygon's flags & 15, TireSurf) */
#define ROADCODE_DIRT        1      /* dirt (rear_dirt_drag_factor; the engine gives more torque, poor traction) */
#define ROADCODE_WATER       2      /* water (rear_water_drag_factor; likewise) */
#define ROADCODE_AIRBORNE    8      /* the tire touches no road (road, init_road) */

/* tModelData.drone_type (munge_gLink_data, init_cars) */
#define DRONE_TYPE_NONE      0      /* the slot is not in the race */
#define DRONE_TYPE_AI        1      /* a drone: its roster owner is below 6 */
#define DRONE_TYPE_HUMAN     2      /* a human player's car (listed in humans[]) */

/* Data */
extern s32 sTrackSongs[EXTENT(19)];
extern u8 cont_bit_pattern;
extern f32 rear_dirt_drag_factor;
extern f32 rear_water_drag_factor;
extern f32 offroad_tires_drag_factor;
extern f32 gWingDrag[EXTENT(3)];
extern f32 gWingLift[EXTENT(3)];
extern f32 gWingAngVDrag;
extern f32 gWingAngVDrag_1;
extern s32 percentTable[EXTENT(36)];
extern f32 res_last_time[EXTENT(4)];
extern s16 *demo_path_points[EXTENT(12)];
extern s16 num_demo_ppoints[EXTENT(12)];
extern Row3 drone_sched[EXTENT(12)];
extern u32 sSmoke[EXTENT(4)];
extern u32 sSmoke2[EXTENT(4)];
extern u32 sSkid[EXTENT(4)];
extern tSkidData sSkidData[EXTENT(4)];
extern f32 pole_pos_offset[EXTENT(12)][3];
extern f32 calcalpha_far_scale_positive;
extern f32 calcalpha_far_scale_negative;
extern f32 frictioncircle_brake_threshold;
extern f32 force_apart_min_dist_sq;
extern f32 force_apart_force;
extern f32 force_apart_force_over_dist;
extern f32 super_speed_scale_x;
extern f32 super_speed_scale_y;
extern f32 super_speed_scale_z;
extern f32 rad_per_sec_to_rpm;
extern f32 mcommunication_rpm_scale;
extern f32 steer_deg_to_rad;
extern f32 drift_soft_suspension_bonus;
extern f32 drift_stiff_suspension_penalty;
extern f32 mass_unit;
extern f32 mass_durability_base;
extern f32 carsetup_durability_base;
extern f32 carsetup_crash_threshold_scale;
extern f32 model_step_dt;
extern f32 initial_thump_threshold;
extern f32 super_speed_unit_diagonal;
extern f32 engine_rpm_scale_crashed;
extern f32 engine_rpm_scale_reverse;
extern f32 engine_rpm_scale;
extern f32 autotrans_torque_scale;
extern f32 roadboost_time_scale;
extern f32 solid_delay_arena;
extern f32 clutch_idle_throttle;
extern f32 clutch_brake_threshold;
extern f32 clutch_rpm_slope;
extern f32 mini_force_normal;
extern f32 mini_force_crashed;
extern f32 mini_force_min_dist2;
extern s8 gNoAutoAbort;
extern s8 gResurrectInPlace;
extern s8 gWind;
extern f32 gGravity;
extern s8 gSuperSpeed;
extern s8 gInvincible;
extern s8 gSpecialTires;
extern s16 humans[EXTENT(8)];
extern s32 gModelIteration;
extern s16 gNumPlayers;
/* gPlayers: declared by its files (src/blob/groups/game_sounds/InitOptions.c: defines it) */
extern s8 gCatchup;
extern s8 gUseCatchup;
extern OSMesg si_event_msgs[EXTENT(8)];
extern tStuntPlayer gStuntPlayer[EXTENT(4)];
extern volatile u16 modelrun;
extern f32 gDeathWaitEnd;
extern s32 gStuntsDone;
extern s8 gNumActiveCars;
extern s16 num_our_drones;
extern volatile s16 num_drones;
extern s16 sched_to_use;
extern s16 new_sched_to_use;
extern s16 sched_index;
extern s16 cur_drone;
extern volatile s16 num_humans;
extern s16 gDemoPathPt;
extern f32 resurrectTime;
extern volatile s16 gNumCars;
extern f32 gModelTime;
extern s16 cur_demo_ppoint[EXTENT(12)];
extern OSMesgQueueBytes model_thread_queue;
extern OSMesg model_thread_msgs[EXTENT(8)];
extern s8 gNoCarCollisions;
extern s8 gSuicideMode;
/* pc_pole_offsets: declared by its files (src/blob/groups/resurrect/resurrect.c: defines it) */
/* pc_tire_records: declared by its files (src/blob/groups/carsetup/group.c: defines it) */

/* Functions */
struct tMiniModelData;   /* a track object as a collision body (MAT3; setMiniFBCollisionForce.c) */
struct TireDesc;   /* copy_tire_info: a tire's constants in the car's physics descriptor (MAT3 anon_tradius_springK_18) */
void AbortCar(s16 arg0, s32 arg1);
void accelerations(f32 *arg0);
void antispin(struct Car *m);
s32 are_all_cars_stopped(void);
s32 AreWeSolid(s16 arg0);
void autoshift(struct Model2056 *m);
void BlinkResurrectingCars(void);
void calcaa(void **arg0, f32 *arg1, f32 *arg2);
f32 calcalpha(f32 *vector);
f32 CalcDrift(void *arg0);
f32 CalcMass(void *arg0);
f32 CalcOffroadTires(void *arg0);
f32 CalcTireSideForce(void *arg0);
void calctireuv(const struct Vec3 *origin, const struct Vec3 *first, const struct Vec3 *second, f32 angle, const struct Basis *transform, const struct Basis *source, struct Basis *output, struct Vec3 *position);
void carsetup(struct Vehicle2056 *m);
void CheckCrash(s16 arg0);
void checkok(void *arg0);
void collision(struct Vehicle2056 *vehicle);
void copy_tire_info(void *arg0, void *arg1);
void dead_reckon_all(void);
void dead_reckon_posuv(struct ModelSnapshot *object);
void DetectDroneAbort(void);
void displaylist_set_texture_image(u32 *dl, u32 value);
void downshift(void *arg0);
void drivetrain(struct Model2056 *m);
void End_MDrive(s16 arg0);
void engine(struct CarE *arg0);
void fake_init(s8 track);
void find_best_gear(void *arg0, f32 arg1, f32 arg2);
void FindBestQuat(f32 *q1, f32 *q2);
void FindHumanResPt(struct tModelData *m, s32 *outPath, s32 *outIdx);
void FindSurfaceUvsFromResPoint(s32 pathMode, s32 route, s32 point, f32 *matrix);
void FinishResurrecting(void *arg0);
void ForceApart(struct Car *car, struct Car *a, struct Car *b, f32 *dir);
void forces(void *arg0);
void forces1(struct VehicleModel *vehicle);
void forces2(struct Model2056 *m);
void frictioncircle(struct Model *m, f32 *tirev, f32 normalforce, f32 torque, struct Tire *tire, f32 *sfp, f32 *trp);
void game_reckon_all(f32 time);
void GetResPt(void);
void init_cars(void);
void init_collision(struct State *p);
void init_controllers(void);
void Init_MDrive(s16 action, s16 index);
void init_model_task(s32 arg0);
void init_road(struct MODELDAT *m);
void InitDemoPathPoint(void);
void initialize(struct Car *car);
s16 interp(s32 a, s32 b, s32 rem, s32 total);
void Interpolate(f32 *a, f32 *b, s16 n, f32 lo, f32 hi, f32 t, f32 *out);
void make_super_speed(void *arg0);
void MakeQuatFromUvs(f32 (*uvs)[3], f32 *q);
void MakeUvsFromQuat(f32 *q, f32 (*uvs)[3]);
void mcommunication(struct Obj *a0);
void MiniForceApart(struct Car *car, f32 *dir);
void model_iteration(void);
void ModelControls(struct MODELDAT *m);
void modelgo(void);
void modelstop(void);
void ModelThread(void *arg);
void MovingSurfaceForce(struct Car *m);
void multigo(s16 arg0);
void munge_gLink_data(void);
s32 PointInBody(void *arg0, void *arg1, f32 arg2);
void positions(struct MODELDAT *m);
void ReadShifter(void);
void regular(void **arg0);
void Resurrect(struct tModelData *m);
void road(struct MODELDAT *m);
void SetCatchup(void);
void SetClutch(void *object);
void setMiniFBCollisionForce(s32 arg0, struct MODELDAT *m, struct Reckon *m2, f32 *dir, f32 *pos);
void SetResurrectionData(s16 mode, struct Vehicle2056 *state, u32 callbackFlag);
void StartGameMusicInternal(s32 resume);
void steer_constants(void *arg0);
void sym(struct MODELDAT *m);
void syminit(struct MODELDAT *m);
void tire_constants(struct Model *m, struct Tire *tdes, f32 otw);
void torques(struct State *state);
void transmission(s8 *arg0);
void update_game_data(void);
void Update_MDrive(void);
void update_reckon_base(s32 arg0, s32 arg1);
void UpdateModel(s32 ipa_s4);
void UpdateResurrectingCar(struct MODELDAT *m);
void UpdateTireAppearance(s16 droneIndex);
void upshift(void *arg0);
void velocities(struct ModelView *m);
void whatslips(struct Model2056 *m);

#endif /* GAME_PHYSICS_H */
