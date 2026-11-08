/* include/game/physics.h: the car simulation: collision, the road, the drivetrain, tyres, respawning */
#ifndef GAME_PHYSICS_H
#define GAME_PHYSICS_H

#include "game/types.h"

/* Data */
extern s32 sTrackSongs[EXTENT(19)];
extern f32 carTransGearRatio_biased;
extern f32 gTune_weight_scale;
extern f32 rear_dirt_drag_factor;
extern f32 rear_water_drag_factor;
extern f32 offroad_tires_drag_factor;
extern f32 gWingDrag[EXTENT(3)];
extern f32 gWingLift[EXTENT(3)];
extern f32 gWingAngVDrag;
extern f32 gWingAngVDrag_1;
extern s32 D_801163F8[EXTENT(36)];
extern f32 D_80117360[EXTENT(4)];
extern s16 *demo_path_points[EXTENT(12)];
extern s16 num_demo_ppoints[EXTENT(12)];
extern f32 D_8011F814[EXTENT(2)][6];
extern Row3 D_80120E74[EXTENT(12)];
extern u32 sSmoke[EXTENT(4)];
extern u32 sSmoke2[EXTENT(4)];
extern u32 sSkid[EXTENT(4)];
extern tSkidData sSkidData[EXTENT(4)];
extern f32 D_801210E8[EXTENT(12)][3];
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
extern u8 gPaks_rumble_request[EXTENT(2963)];
extern s16 gNumPlayers;
/* gPlayers: declared by its files (src/blob/groups/game_sounds/InitOptions.c: defines it) */
extern s8 first_camera_slot;
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
#endif /* GAME_PHYSICS_H */
