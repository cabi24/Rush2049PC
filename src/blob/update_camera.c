#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
typedef struct { s32 opaque[0x1B0 / 4]; } OSThread;
typedef struct Object {
    u8 other0[8];
    Vec3 pos;           /* 8 */
    u8 other20[24];
    Mat3 uvs44;         /* 44 */
    Mat3 uvs;           /* 80 */
    u8 other116[0x110 - 0x74]; struct pc_visual visuals[21]; u8 other116_308[0x35B - 0x308];
    s8 car;             /* 859 */
    s8 slot;            /* 860 */
    u8 view;            /* 861 */
} Object;
typedef struct Model {
    u8 other0[328];
    f32 body_force[2][3];   /* 328 */
    f32 center_force[2][3]; /* 352 */
    u8 other376[1356];
    s16 f1732;              /* 1732 */
    u8 other1734[322];
} Model;                    /* 0x808 */
typedef struct Camera {
    Mat3 uvs;           /* 0 */
    Vec3 pos;           /* 36 */
    u8 other48[48];
    Mat3 out_uvs;       /* 96 */
    Vec3 out_pos;       /* 132 */
    f32 spring;         /* 144 */
    u8 other148[4];
} Camera;               /* 152 */
VIEW(Camera[], gCamera);
#define gCamera gCamera_view
VIEW(Vec3[], cur_acc);
#define cur_acc cur_acc_view
VIEW(OSThread, model_thread);
#define model_thread model_thread_view

extern void osStopThread(OSThread *);
extern void osStartThread(OSThread *);

/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): the driver's eye smooths the car's peak forces once a frame, a filter
 * tuned to a frame of two retraces (PC_FILTER, pc_game_fps.h); the vertical one also takes the spring each frame, which
 * a shorter frame takes in proportion, so that y = y * e - spring settles where the N64's does. */
#define PC_DECAY_IN(u, e) (pc_fps60 && pc_frame_scale != 1.0f && (e) < 1.0f ? (u) * (1.0f - pc_fps_filter(e)) / (1.0f - (e)) : (u))
extern s32 CameraCollision(void *arg0, void *arg1);

#define CAM (gCamera[slot])
void update_camera(Object *object)
{
    s32 i;
    s32 j;
    Vec3 pos_in;
    Vec3 pos_out;
    Model *m;
    s32 slot;

    m = PC_MODEL(object->car);
    slot = object->slot;
    CAM.pos[0] = object->pos[0];
    CAM.pos[1] = object->pos[1];
    CAM.pos[2] = object->pos[2];
    CAM.pos[1] += 0.25f;
    switch (object->view) {
    case 0:
        update_car_object(object, CAM.pos, object->uvs);
        pos_in[0] = 0.0f;
        pos_in[1] = view1_suscomp_offset(m) + view0_eye_height;
        pos_in[2] = view0_eye_forward;
        CopyUV(object->uvs44, &CAM);
        WorldVector(pos_in, pos_out, &CAM);
        CAM.pos[0] = CAM.pos[0] + pos_out[0];
        CAM.pos[1] = CAM.pos[1] + pos_out[1];
        CAM.pos[2] = CAM.pos[2] + pos_out[2];
        break;
    case 1:
        osStopThread(&model_thread);
        for (j = 0; j != 4; j += 2) {
            if (m->center_force[0][j] > 100000.0f) m->center_force[0][j] = 100000.0f;
            if (m->center_force[1][j] < -100000.0f) m->center_force[1][j] = -100000.0f;
            if (m->body_force[0][j] > 100000.0f) m->body_force[0][j] = 100000.0f;
            if (m->body_force[1][j] < -100000.0f) m->body_force[1][j] = -100000.0f;
            pos_in[j] = m->center_force[0][j] * 0.00002f;
            pos_in[j] += m->center_force[1][j] * 0.00002f;
            m->center_force[0][j] = 0.0f;
            m->center_force[1][j] = 0.0f;
            pos_in[j] += m->body_force[0][j] * 0.00002f;
            pos_in[j] += m->body_force[1][j] * 0.00002f;
            m->body_force[0][j] = 0.0f;
            m->body_force[1][j] = 0.0f;
        }
        osStartThread(&model_thread);
        for (i = 0; i != 4; i += 2) {
            cur_acc[slot][i] = cur_acc[slot][i] * PC_FILTER(view1_accel_filter) + pos_in[i] * (1.0f - PC_FILTER(view1_accel_filter));
        }
        cur_acc[slot][1] = cur_acc[slot][1] * PC_FILTER(view1_spring_filter) - PC_DECAY_IN(CAM.spring, view1_spring_filter);
        pos_in[0] = view1_eye_x - cur_acc[slot][0];
        pos_in[1] = view1_eye_y - cur_acc[slot][1];
        pos_in[2] = view1_eye_z - cur_acc[slot][2];
        CopyUV(object->uvs, &CAM);
        WorldVector(pos_in, pos_out, &CAM);
        if (pos_out[1] < 0.0f) {
            CAM.pos[1] -= pos_out[1] * 0.7f;
        }
        update_car_object(object, CAM.pos, &CAM);
        CAM.pos[0] = CAM.pos[0] + pos_out[0];
        CAM.pos[1] = CAM.pos[1] + pos_out[1];
        CAM.pos[2] = CAM.pos[2] + pos_out[2];
        break;
    case 2:
    case 3:
        pos_in[0] = CAM.pos[0];
        pos_in[1] = CAM.pos[1];
        pos_in[2] = CAM.pos[2];
        View3Cam(object, CAM.pos);
        CopyUV(CAM.out_uvs, &CAM);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        if (m->f1732 < 0) {
            CameraCollision(CAM.pos, pos_in);
        }
        break;
    case 10:
        pos_in[0] = CAM.pos[0];
        pos_in[1] = CAM.pos[1];
        pos_in[2] = CAM.pos[2];
        UpdateCarObj(object, CAM.pos, &object->uvs, pos_in);
        CopyUV(CAM.out_uvs, &CAM);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        CameraCollision(CAM.pos, pos_in);
        break;
    case 8:
        pos_in[0] = CAM.pos[0];
        pos_in[1] = CAM.pos[1];
        pos_in[2] = CAM.pos[2];
        UpdateCarObj(object, CAM.pos, &object->uvs, pos_in);
        CopyUV(CAM.out_uvs, &CAM);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        break;
    case 4:
        DeathCam(object, CAM.pos, &object->uvs);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        CopyUV(CAM.out_uvs, &CAM);
        CameraCollision(CAM.pos, object->pos);
        break;
    case 5:
        circle_camera_around_car(object, CAM.pos, &object->uvs);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        CopyUV(CAM.out_uvs, &CAM);
        break;
    case 6:
        fix_camera_in_space(2, object, CAM.pos, &object->uvs);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        CopyUV(CAM.out_uvs, &CAM);
        break;
    case 7:
        steady_move_cam(2, object, CAM.pos, &object->uvs);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        CopyUV(CAM.out_uvs, &CAM);
        break;
    case 9:
        maxpath_cam(2, object, CAM.pos, &object->uvs);
        CAM.pos[0] = CAM.out_pos[0];
        CAM.pos[1] = CAM.out_pos[1];
        CAM.pos[2] = CAM.out_pos[2];
        CopyUV(CAM.out_uvs, &CAM);
        break;
    }
}
