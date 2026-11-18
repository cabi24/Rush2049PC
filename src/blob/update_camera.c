/* update_camera: a window's camera follows its car in its view (MAT3 camera.c). */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];

/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): the driver's eye smooths the car's peak forces once a frame, a filter
 * tuned to a frame of two retraces (PC_FILTER, pc_game_fps.h); the vertical one also takes the spring each frame, which
 * a shorter frame takes in proportion, so that y = y * e - spring settles where the N64's does. */
#define PC_DECAY_IN(u, e) (pc_fps60 && pc_frame_scale != 1.0f && (e) < 1.0f ? (u) * (1.0f - pc_fps_filter(e)) / (1.0f - (e)) : (u))

#define CAM (gCamera[slot])

/* Moves the camera of car `object` (its window's) for the car's view: 0 the driver's eye (with the suspension's
 * travel, view1_suscomp_offset); 1 the chase view, which lags the car's peak forces (taken from the model with its
 * thread stopped, and cleared) and its spring; 2 and 3 view3's high chase (View3Cam) and 10 and 8 the demo's car
 * camera (UpdateCarObj), the camera kept out of walls (CameraCollision) but in 8 and in 3 while being put back;
 * 4 the crash (DeathCam); 5 the finish's circling (circle_camera_around_car); 6 a camera fixed by the track
 * (fix_camera_in_space); 7 the steady camera (steady_move_cam); 9 the camera over the drone path (maxpath_cam).
 * The views from 2 on build the camera in fcam and copy it to mat. */
void update_camera(tCarData *object)
{
    s32 i;
    s32 j;
    Vec3 pos_in;
    Vec3 pos_out;
    tModelData *m;
    s32 slot;

    m = &pc_models[object->slot];
    slot = object->camera;
    CAM.mat.pos[0] = object->dr_pos[0];
    CAM.mat.pos[1] = object->dr_pos[1];
    CAM.mat.pos[2] = object->dr_pos[2];
    CAM.mat.pos[1] += 0.25f;
    switch ((u8)object->view) {   /* the view, 0-4; this file always read it unsigned */
    case 0:
        update_car_object(object, CAM.mat.pos, object->dr_car_uvs);
        pos_in[0] = 0.0f;
        pos_in[1] = view1_suscomp_offset(m) + view0_eye_height;
        pos_in[2] = view0_eye_forward;
        CopyUV(object->dr_uvs, &CAM);
        WorldVector(pos_in, pos_out, &CAM);
        CAM.mat.pos[0] = CAM.mat.pos[0] + pos_out[0];
        CAM.mat.pos[1] = CAM.mat.pos[1] + pos_out[1];
        CAM.mat.pos[2] = CAM.mat.pos[2] + pos_out[2];
        break;
    case 1:
        thread_stop(&model_thread);
        for (j = 0; j != 4; j += 2) {
            if (m->peak_center_force[0][j] > 100000.0f) m->peak_center_force[0][j] = 100000.0f;
            if (m->peak_center_force[1][j] < -100000.0f) m->peak_center_force[1][j] = -100000.0f;
            if (m->peak_body_force[0][j] > 100000.0f) m->peak_body_force[0][j] = 100000.0f;
            if (m->peak_body_force[1][j] < -100000.0f) m->peak_body_force[1][j] = -100000.0f;
            pos_in[j] = m->peak_center_force[0][j] * 0.00002f;
            pos_in[j] += m->peak_center_force[1][j] * 0.00002f;
            m->peak_center_force[0][j] = 0.0f;
            m->peak_center_force[1][j] = 0.0f;
            pos_in[j] += m->peak_body_force[0][j] * 0.00002f;
            pos_in[j] += m->peak_body_force[1][j] * 0.00002f;
            m->peak_body_force[0][j] = 0.0f;
            m->peak_body_force[1][j] = 0.0f;
        }
        thread_start(&model_thread);
        for (i = 0; i != 4; i += 2) {
            cur_acc[slot][i] = cur_acc[slot][i] * PC_FILTER(view1_accel_filter) + pos_in[i] * (1.0f - PC_FILTER(view1_accel_filter));
        }
        cur_acc[slot][1] = cur_acc[slot][1] * PC_FILTER(view1_spring_filter) - PC_DECAY_IN(CAM.view2spring, view1_spring_filter);
        pos_in[0] = view1_eye_x - cur_acc[slot][0];
        pos_in[1] = view1_eye_y - cur_acc[slot][1];
        pos_in[2] = view1_eye_z - cur_acc[slot][2];
        CopyUV(object->dr_car_uvs, &CAM);
        WorldVector(pos_in, pos_out, &CAM);
        if (pos_out[1] < 0.0f) {
            CAM.mat.pos[1] -= pos_out[1] * 0.7f;
        }
        update_car_object(object, CAM.mat.pos, &CAM);
        CAM.mat.pos[0] = CAM.mat.pos[0] + pos_out[0];
        CAM.mat.pos[1] = CAM.mat.pos[1] + pos_out[1];
        CAM.mat.pos[2] = CAM.mat.pos[2] + pos_out[2];
        break;
    case 2:
    case 3:
        pos_in[0] = CAM.mat.pos[0];
        pos_in[1] = CAM.mat.pos[1];
        pos_in[2] = CAM.mat.pos[2];
        View3Cam(object, CAM.mat.pos);
        CopyUV(CAM.fcam.uvs, &CAM);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        if (m->resurrect.moving_state < 0) {
            CameraCollision(CAM.mat.pos, pos_in);
        }
        break;
    case 10:
        pos_in[0] = CAM.mat.pos[0];
        pos_in[1] = CAM.mat.pos[1];
        pos_in[2] = CAM.mat.pos[2];
        UpdateCarObj(object, CAM.mat.pos, &object->dr_car_uvs, pos_in);
        CopyUV(CAM.fcam.uvs, &CAM);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        CameraCollision(CAM.mat.pos, pos_in);
        break;
    case 8:
        pos_in[0] = CAM.mat.pos[0];
        pos_in[1] = CAM.mat.pos[1];
        pos_in[2] = CAM.mat.pos[2];
        UpdateCarObj(object, CAM.mat.pos, &object->dr_car_uvs, pos_in);
        CopyUV(CAM.fcam.uvs, &CAM);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        break;
    case 4:
        DeathCam(object, CAM.mat.pos, &object->dr_car_uvs);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        CopyUV(CAM.fcam.uvs, &CAM);
        CameraCollision(CAM.mat.pos, object->dr_pos);
        break;
    case 5:
        circle_camera_around_car(object, CAM.mat.pos, &object->dr_car_uvs);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        CopyUV(CAM.fcam.uvs, &CAM);
        break;
    case 6:
        fix_camera_in_space(2, object, CAM.mat.pos, &object->dr_car_uvs);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        CopyUV(CAM.fcam.uvs, &CAM);
        break;
    case 7:
        steady_move_cam(2, object, CAM.mat.pos, &object->dr_car_uvs);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        CopyUV(CAM.fcam.uvs, &CAM);
        break;
    case 9:
        maxpath_cam(2, object, CAM.mat.pos, &object->dr_car_uvs);
        CAM.mat.pos[0] = CAM.fcam.pos[0];
        CAM.mat.pos[1] = CAM.fcam.pos[1];
        CAM.mat.pos[2] = CAM.fcam.pos[2];
        CopyUV(CAM.fcam.uvs, &CAM);
        break;
    }
}
