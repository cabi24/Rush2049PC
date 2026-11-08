/* include/game/camera.h: the cameras */
#ifndef GAME_CAMERA_H
#define GAME_CAMERA_H

#include "game/types.h"

/* Data */
extern f32 view0_eye_height;
extern f32 view0_eye_forward;
extern f32 view1_eye_x;
extern f32 view1_eye_y;
extern f32 view1_eye_z;
extern f32 view1_accel_filter;
extern f32 view1_spring_filter;
extern f32 lastSwitch[EXTENT(4)];
extern f32 camera_nextSwitch[EXTENT(3)];
extern f32 FSCLS_from_one[EXTENT(1)];
extern struct Respawn *trackSpots[EXTENT(6)];
extern f32 steady_leg_duration[EXTENT(272)];
extern f32 fixed_cam_zoom_rate;
extern f32 view3_speed_stretch;
extern f32 view3_elastic_range;
extern f32 view3_elastic_max;
extern f32 view3_elastic_min;
extern f32 view3_yf_rate_range;
extern f32 view3_yf_rate_base;
extern f32 view3_zvec_min_len;
extern f32 view3_zvec_elastic_min_len;
extern f32 view3_yvec_min_len;
extern f32 view3_yvec_elastic_min_len;
extern f32 view3_xvec_min_len;
extern f32 view1_suscomp_bias;
extern f32 fixed_cam_pos[EXTENT(4)][3];
extern s8 fixed_cam_mode[EXTENT(4)];
extern Vector steady_start_pos[EXTENT(4)];
extern s16 steady_stage[EXTENT(4)];
extern f32 steady_leg_time[EXTENT(2)];
extern s16 maxpath_cam_start_point[EXTENT(4)];
extern s16 maxpath_cam_start_path[EXTENT(4)];
extern s16 maxpath_cam_last_point[EXTENT(4)];
extern f32 maxpath_cam_start_time[EXTENT(4)];
extern f32 maxpath_cam_last_dist[EXTENT(4)];
extern CameraView gCamera[EXTENT(1)];
extern s32 gCamera_n64;
extern s16 gNumWindows;
extern s8 gUpsideDownTrack;
extern s8 view3_switch[EXTENT(4)];
extern s8 gTireCnt[EXTENT(4)];
extern f32 gSusAvg[EXTENT(4)];
extern Vector cur_acc[EXTENT(4)];
extern f32 rear_y[EXTENT(4)];
extern f32 rear_z[EXTENT(4)];
extern f32 elasticity[EXTENT(4)];
extern f32 elastic_factor[EXTENT(4)];
extern f32 last_zvec[EXTENT(4)][3];
extern f32 last_yvec[EXTENT(4)][3];
extern f32 zf[EXTENT(4)];
extern f32 yf[EXTENT(4)];
extern s32 gContLevels_pad2;
extern s32 gContEdges_pad2;

/* Functions */
void CheckCameraView(void);
void circle_camera_around_car(struct Object *object, f32 *carpos, f32 (*matrix)[3][3]);
void DeathCam(struct Cr *car, f32 *pos, f32 *uvs);
void fix_camera_in_space(s16 arg0, void *arg1, void *arg2, intptr_t arg3);
void init_camera_on_track(struct Object *object);
void init_steady_move_cam(struct Obj *o);
void init_view3(void *arg0);
void init_views(void);
void InitCameras(void);
void LookInDir(f32 *dir, f32 (*m)[3]);
void maxpath_cam(s16 update, struct Object *object, f32 *position, f32 (*matrix)[3][3]);
void SetCameraView(s16 arg0);
void steady_move_cam(s16 mode, struct Cr *car, f32 *pos, intptr_t uvs);
void update_camera(struct Object *object);
void update_car_object(void *car, void *vel, void *mat);
void UpdateCameras(void);
void UpdateCarObj(struct Cr *car, void *arg1, void *arg2, intptr_t arg3);
void UpdateDroneObjects_stub(void);
void UpdateRearCamera(struct Cr *car, f32 ab, f32 AB, f32 *camoff);
f32 view1_suscomp_offset(void *arg0);
void View3Cam(void *arg0, void *arg1);
void ZeroCamera(s16 arg0);
void ZeroCameras(void);

#endif /* GAME_CAMERA_H */
