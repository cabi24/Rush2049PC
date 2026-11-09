/* circle_camera_around_car: the finish view, circling the car (MAT3 camera.c). */
#include "game/game.h"

typedef f32 Mat3[3][3];
/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its slot and camera. */
typedef struct Object { u8 other[0x110 - 0x0]; struct pc_visual visuals[21]; u8 other_308[0x35B - 0x308]; s8 car,camera; } Object;

extern f32 sinf(f32);
extern f32 cosf(f32);

/* Milliseconds of `t` seconds. */
#define IRQTIME(t) ((u32)((t) * 1000.0f))

/* Car `object`'s finish view (5): the camera circles the car once every 6 seconds, 15 feet out and 10 up, and
 * points at it (`carpos`, its orientation into `matrix`). Ten seconds after the finish the circling stops (finish
 * camera 2: the angle frozen at that time). The finish cameras are the PC's pc_finish_cam (DECOUPLE.md 7). */
void circle_camera_around_car(Object *object, f32 *carpos, Mat3 *matrix) {
    f32 res[3], ang;
    s32 camera = object->camera;

    if (PC_FINISH_CAM(object->car) == 1 && PC_FINISH_CAM_TIME(object->car) + 10.0f < gModelTime) {
        PC_FINISH_CAM_TIME(object->car) = gModelTime;
        PC_FINISH_CAM(object->car) = 2;
    }
    if (PC_FINISH_CAM(object->car) == 2)
        ang = (f32)(IRQTIME(PC_FINISH_CAM_TIME(object->car)) % 6000) * (1.0f / 6000) * 6.2831855f;
    else
        ang = (f32)(IRQTIME(gModelTime) % 6000) * (1.0f / 6000) * 6.2831855f;

    res[0] = cosf(ang) * 15.0f;
    res[1] = 10.0f;
    res[2] = sinf(ang) * 15.0f;
    elastic_factor[camera] = 0.0f;
    point_at_car(object, carpos, matrix, res);
}
