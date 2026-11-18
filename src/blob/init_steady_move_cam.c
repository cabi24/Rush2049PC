/* init_steady_move_cam: the steady camera's first place (MAT3 camera.c). */
#include "game/game.h"

/* Starts car `o`'s steady camera (steady_move_cam, mode 0) 80 feet ahead of the car, 20 to its side and 5 up. */
void init_steady_move_cam(tCarData *o) {
    float pos[3];
    float out[9];
    pos[0] = o->dr_pos[0] + o->dr_uvs[2][0] * 80.0f + o->dr_uvs[0][0] * 20.0f;
    pos[1] = o->dr_pos[1] + 5.0f;
    pos[2] = o->dr_pos[2] + o->dr_uvs[2][2] * 80.0f + o->dr_uvs[0][2] * 20.0f;
    steady_move_cam(0, o, pos, out);
}
