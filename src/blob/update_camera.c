/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * update_camera (0x800EB028, 1,640 B): per-object camera view update = arcade game/camera.c setcamview()'s
 * `switch (view)` (N64 view byte 861, 0..10; slot camera gCamera[slot] (152 B), model D_8014A250[car]).
 * Camera position starts at the object position + 0.25 up. 0 worm's eye (update_car_object, view1_suscomp
 * offset view1_suscomp_offset + 1.2, 4.0 forward, ftransvec = WorldVector), 1 driver's eye (collision-force clamp
 * and smoothing between osStopThread/osStartThread on model_thread, x/z only: `j != 4; j += 2`; cur_acc =
 * cur_acc[slot]; driver offset view1_eye_x..98; acc elasticity view1_accel_filter / view1_spring_filter), 2/3 View3Cam
 * (View3Cam), 8/10 UpdateCarObj, 4 DeathCam, 5 circle (circle_camera_around_car),
 * 6 fixed (fix_camera_in_space), 7 steady_move_cam, 9 maxpath_cam; each copies the
 * camera's out matrix/pos back (CopyUV = fmatcopy) and some re-ground the camera (CameraCollision).
 * STATE: MATCH (from the draft; GC camera.c update_camera locals i, j, pos_in, pos_out, m, camera).
 * Levers: no Camera pointer local -- the slot camera is the expression gCamera[slot] (macro CAM; GC keeps
 * an int `camera` index, not a pointer), which frees the home the pointer took and lets uopt promote pos_out[1]
 * into $f0 as retail (175 -> 20 rows, ops 0); `CAM.pos[i] = CAM.pos[i] + pos_out[i]` instead of `+=` gives
 * retail's commutative operand order (pos_out first) in cases 0/1. No devices.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
typedef struct { s32 opaque[0x1B0 / 4]; } OSThread;
typedef struct {
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
typedef struct {
    u8 other0[328];
    f32 body_force[2][3];   /* 328 */
    f32 center_force[2][3]; /* 352 */
    u8 other376[1356];
    s16 f1732;              /* 1732 */
    u8 other1734[322];
} Model;                    /* 0x808 */
typedef struct {
    Mat3 uvs;           /* 0 */
    Vec3 pos;           /* 36 */
    u8 other48[48];
    Mat3 out_uvs;       /* 96 */
    Vec3 out_pos;       /* 132 */
    f32 spring;         /* 144 */
    u8 other148[4];
} Camera;               /* 152 */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Camera gCamera[];
extern Vec3 cur_acc[];
extern OSThread model_thread;
extern f32 view0_eye_height, view0_eye_forward, view1_eye_x, view1_eye_y, view1_eye_z, view1_accel_filter, view1_spring_filter;
extern void osStopThread(OSThread *);
extern void osStartThread(OSThread *);
extern void CopyUV(void *src, void *dst);
extern void WorldVector(f32 *in, f32 *out, void *uvs);
extern f32 view1_suscomp_offset(Model *m);
extern void update_car_object(Object *object, f32 *pos, void *uvs);
extern void View3Cam(Object *object, f32 *pos);
extern void UpdateCarObj(Object *object, f32 *pos, Mat3 *uvs, f32 *old);
extern void DeathCam(Object *object, f32 *pos, Mat3 *uvs);
extern void circle_camera_around_car(Object *object, f32 *pos, Mat3 *uvs);
extern void fix_camera_in_space(s16 mode, Object *object, f32 *pos, Mat3 *uvs);
extern void steady_move_cam(s16 mode, Object *object, f32 *pos, Mat3 *uvs);
extern void maxpath_cam(s16 mode, Object *object, f32 *pos, Mat3 *uvs);
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
