/* init_view3: a car's camera to the rear chase view (MAT3 camera.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its camera and views. */
struct tCarData {
    u8 pad0[0x110]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308];
    s8 camera;      /* 0x35C its camera, the window showing it */
    s8 view;        /* 0x35D its camera's view */
    s8 saveview;    /* 0x35E the view to go back to after the crash view */
};

/* Sets car `gc`'s camera to view 2 (the high rear chase view, view3's), starting from its rest distance: 5 feet up
 * and 20 back in the demo, 3 and 16 otherwise. */
void init_view3(struct tCarData *gc) {
    s8 camera;

    gc->saveview = 2;
    camera = gc->camera;
    gc->view = gc->saveview;
    if (gGameState & GS_DEMO) {
        rear_y[camera] = 5.0f;
        rear_z[camera] = 20.0f;
    } else {
        rear_y[camera] = 3.0f;
        rear_z[camera] = 16.0f;
    }
    view3_switch[camera] = 1;
}
