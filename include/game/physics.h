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
void AbortCar(s16 slot, s32 death);
void accelerations(struct tModelData *m);
void antispin(struct tModelData *m);
s32 are_all_cars_stopped(void);
s32 AreWeSolid(s16 slot);
void autoshift(struct tModelData *m);
void BlinkResurrectingCars(void);
void calcaa(struct tModelData *m, f32 *moment, f32 *angaccel);
f32 calcalpha(f32 *tirev);
f32 CalcDrift(struct tModelData *m);
f32 CalcMass(struct tModelData *m);
f32 CalcOffroadTires(struct tModelData *m);
f32 CalcTireSideForce(struct tModelData *m);
void calctireuv(const struct Vec3 *v, const struct Vec3 *w, const struct Vec3 *r, f32 steer, const struct Basis *caruvs, const struct Basis *roaduvs, struct Basis *tireuvs, struct Vec3 *tirev);
struct Surface *CameraCollision(f32 *ipos, f32 *lookat);
void carsetup(struct tModelData *m);
void CheckCrash(s16 slot);
void checkok(struct tModelData *m);
void collision(struct tModelData *m);
void copy_tire_info(struct TireDesc *tp1, struct Tire *tp2);
void dead_reckon_all(void);
void dead_reckon_posuv(struct tModelData *m);
void DetectDroneAbort(void);
void dotireforce(struct tModelData *m, f32 *tirev, void *tireuvs, struct Tire *tire, f32 torque, f32 *forcevec,
                 f32 suscomp, f32 otsuscomp, f32 springrate, f32 arspringrate, f32 cdamping, f32 rdamping,
                 s32 poortract);
streetyp *downleaf(streetyp *tp, s16 posh, s16 posv, s16 *quad);
void downshift(struct tModelData *m);
void drivetrain(struct tModelData *m);
void End_MDrive(s16 slot);
void engine(struct tModelData *m);
s16 enginetorque(struct tModelData *m, s16 rpm, s16 throttle, s16 *torquecurve);
void fake_init(s8 track);
void find_best_gear(struct tModelData *m, f32 usang, f32 dsang);
void FindBestQuat(f32 *q1, f32 *q2);
void FindHumanResPt(struct tModelData *m, s32 *outPath, s32 *outIdx);
void FindSurfaceUvsFromResPoint(s32 res, s32 path, s32 idx, f32 *uvs);
void FinishResurrecting(struct tModelData *m);
void ForceApart(struct tModelData *m, struct tModelData *m1, struct tModelData *m2, f32 *dir);
void forces(struct tModelData *m);
void forces1(struct tModelData *m);
void forces2(struct tModelData *m);
void frictioncircle(struct tModelData *m, f32 *tirev, f32 normalforce, f32 torque, struct Tire *tire, f32 *sfp, f32 *trp);
void game_reckon_all(f32 time);
void GetResPt(void);
void init_cars(void);
void init_collision(struct tModelData *m);
void init_controllers(void);
void Init_MDrive(s16 mode, s16 slot);
void init_model_task(s32 frames);
void init_road(struct tModelData *m);
void InitDemoPathPoint(void);
void initialize(struct tModelData *m);
s16 interp(s32 a, s32 b, s32 rem, s32 total);
void Interpolate(f32 *a, f32 *b, s16 n, f32 lo, f32 hi, f32 t, f32 *out);
void make_super_speed(struct tModelData *m);
void MakeQuatFromUvs(f32 (*uvs)[3], f32 *q);
void MakeUvsFromQuat(f32 *q, f32 (*uvs)[3]);
void mcommunication(struct tModelData *m);
void MiniForceApart(struct tModelData *m, f32 *dir);
void model_iteration(void);
void ModelControls(struct tModelData *m);
void modelgo(void);
void modelstop(void);
void ModelThread(void *arg);
void MovingSurfaceForce(struct tModelData *m);
void multigo(s16 slot);
void munge_gLink_data(void);
s32 PointInBody(struct tModelData *m, f32 *pt, f32 top);
void positions(struct tModelData *m);
void ReadShifter(void);
void regular(struct tModelData *m);
void Resurrect(struct tModelData *m);
void road(struct tModelData *m);
struct surftyp *RoadPosition(f32 *pos, f32 *out, f32 (*mat)[3]);
void SetCatchup(void);
void SetClutch(struct tModelData *m);
void setFBCollisionForce(struct tModelData *m, struct tModelData *m1, struct tModelData *m2, f32 *dir, f32 *pos);
void setMiniFBCollisionForce(s32 m_unused, struct tModelData *m, struct tMiniModelData *m2, f32 *dir, f32 *pos);
void SetResurrectionData(s16 mode, struct tModelData *m, u32 abort);
struct surftyp *ShotToWorldCollision(f32 *start, f32 *end, f32 (*mat)[3], f32 radius, s32 snap, s32 skipType);
void StartGameMusicInternal(s32 resume);
void steer_constants(struct tModelData *m);
void sym(struct tModelData *m);
void syminit(struct tModelData *m);
void tire_constants(struct tModelData *m, struct Tire *tdes, f32 otw);
void torques(struct tModelData *m);
void transmission(struct tModelData *m);
void update_game_data(void);
void Update_MDrive(void);
void update_reckon_base(s32 slot, s32 unused);
void UpdateModel(s32 slot);
void UpdateResurrectingCar(struct tModelData *m);
void UpdateTireAppearance(s16 droneIndex);
void upshift(struct tModelData *m);
void velocities(struct tModelData *m);
void whatslips(struct tModelData *m);

#endif /* GAME_PHYSICS_H */
