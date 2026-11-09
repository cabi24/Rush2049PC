#include "game/game.h"

typedef struct { u8 p0[148]; u8 idx; u8 p1[3]; } A152;
typedef struct { u8 p[0x110 - 0x0]; struct pc_visual visuals[21]; u8 p_308[0x35C - 0x308]; u8 f860; u8 f861; u8 q[0x380 - 0x35E]; void *input; u8 q_384[0x3B8 - 0x384]; } B952;

VIEW(A152[], gCamera);
#define gCamera gCamera_view

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* This file defines D_80110680, the finish cameras' state, on the N64; on the PC it is pc_finish_cam, with no N64
 * address (pc_game_types.h, docs/port/DECOUPLE.md 7), and the clear resets every slot it has. */
#define FINISH_CAM_SLOTS PC_MAX_LINKS

void init_views(void) {
    s32 i;
    B952 *b;
    for (i = 0; i < gNumWindows; i++) {
        b = PC_CAR(gCamera[i].idx);
        b->f860 = i;
        init_view3(b);
        b->f861 = 7;
        init_steady_move_cam(b);
        camera_nextSwitch[i] = 0.0f;
    }
    for (i = 0; i < FINISH_CAM_SLOTS; i++) {
        PC_FINISH_CAM(i) = 0;
    }
}
