/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * circle_camera_around_car - circling camera for one car slot. Arcade ancestor:
 * game/camera.c circle_camera_around_car(): the camera offset rotates about
 * the car once per 6 s (IRQTIME % 6000 ms), res = {cos*15, 10, sin*15}, then
 * the slot elastic factor is cleared and the follow update (point_at_car,
 * N64 UpdateCarObj) places the camera. N64 additions: per-car state byte
 * D_80110680[car]; state 1 latches the clock (D_80110668[car]) after 10 s and
 * switches to state 2, which freezes the angle at the latched time.
 * IRQTIME is the float clock gModelTime in ms converted to u32 (the
 * cvt/unsigned sequences). Own literals (1/6000, 2*pi, twice) are own .rodata.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef f32 Mat3[3][3];
typedef struct { u8 other[0x110 - 0x0]; struct pc_visual visuals[21]; u8 other_308[0x35B - 0x308]; s8 car,slot; } Object;
extern f32 elastic_factor[];
extern f32 gModelTime;
/* On the PC the finish cameras are PC data, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 7). */
extern f32 sinf(f32);
extern f32 cosf(f32);
extern void point_at_car(Object *object, f32 *carpos, Mat3 *matrix, f32 *res);

#define IRQTIME(t) ((u32)((t) * 1000.0f))

void circle_camera_around_car(Object *object, f32 *carpos, Mat3 *matrix) {
    f32 res[3], ang;
    s32 slot = object->slot;

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
    elastic_factor[slot] = 0.0f;
    point_at_car(object, carpos, matrix, res);
}
