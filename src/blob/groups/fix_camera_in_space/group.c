/* fix_camera_in_space: a camera fixed at a point of the track, watching the car (MAT3 camera.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its camera. */
struct tCarData { u8 pad0[0x110]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308]; s8 camera; };

extern void point_at_car(void*,void*,intptr_t,f32*);

/* Car `car`'s track camera (view 6). `mode` 0 fixes the camera at `pos`, 1 there too but zooming in on the car as
 * it goes away; else (2) it points from where it was fixed at the car, now at `pos` (its orientation into `uvs`:
 * point_at_car), a zooming one kept 20 feet off and closing in by fixed_cam_zoom_rate a foot past 50. */
void fix_camera_in_space(s16 mode, struct tCarData *car, f32 *pos, intptr_t uvs) {
    f32 vector[3];
    f32 dist;
    f32 scale;
    s8 camera;
    f32 *fixed;

    camera = car->camera;
    if (mode == 0) {
        fixed_cam_mode[camera] = 0;
        fixed = fixed_cam_pos[camera];
        fixed[0] = (f32) pos[0];
        fixed[1] = (f32) pos[1];
        fixed[2] = (f32) pos[2];
        return;
    }
    if (mode == 1) {
        fixed_cam_mode[camera] = 1;
        fixed = fixed_cam_pos[camera];
        fixed[0] = (f32) pos[0];
        fixed[1] = (f32) pos[1];
        fixed[2] = (f32) pos[2];
        return;
    }
    fixed = fixed_cam_pos[camera];
    vector[0] = fixed[0] - pos[0];
    vector[1] = fixed[1] - pos[1];
    vector[2] = fixed[2] - pos[2];
    elastic_factor[camera] = 0.0f;
    if (fixed_cam_mode[camera] != 0) {
        dist = vector_magnitude(vector);
        if (dist != 0.0f) {
            scale = (((dist - 50.0f) * fixed_cam_zoom_rate) + 20.0f) / dist;
            vector[0] *= scale;
            vector[1] *= scale;
            vector[2] *= scale;
        }
    }
    point_at_car(car, pos, uvs, vector);
}
