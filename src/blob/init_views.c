/* init_views: the cameras' views at a race's start (MAT3 camera.c). */
#include "game/game.h"

VIEW(tCamera[], gCamera);
#define gCamera gCamera_view

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its camera and view. */
typedef struct { u8 p[0x110 - 0x0]; struct pc_visual visuals[21]; u8 p_308[0x35C - 0x308]; u8 camera; u8 view; u8 q[0x380 - 0x35E]; void *input; u8 q_384[0x3B8 - 0x384]; } B952;

/* The finish cameras' state is the PC's pc_finish_cam, one per car slot (docs/port/DECOUPLE.md 7): the clear resets
 * every slot it has. */
#define FINISH_CAM_SLOTS PC_MAX_LINKS

/* Gives each window's car its camera, the window's: the view3 chase view's distances (init_view3), then view 7,
 * with the steady camera set up (init_steady_move_cam), and no demo view switch due. Turns every finish camera off. */
void init_views(void) {
    s32 i;
    B952 *gc;
    for (i = 0; i < gNumWindows; i++) {
        gc = PC_CAR(gCamera[i].slot);
        gc->camera = i;
        init_view3(gc);
        gc->view = 7;
        init_steady_move_cam(gc);
        camera_nextSwitch[i] = 0.0f;
    }
    for (i = 0; i < FINISH_CAM_SLOTS; i++) {
        PC_FINISH_CAM(i) = 0;
    }
}
