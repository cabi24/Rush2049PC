/* point_at_car.c: a chase camera follows and looks at its car (MAT3 camera.c, point_at_car()). */
#include "game/game.h"

typedef f32 Mat3[3][3];
/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields used here. */
typedef struct tCarData { u8 other[0x110 - 0x0]; struct pc_visual visuals[21]; u8 other_308[0x35B - 0x308]; s8 slot,camera; } tCarData;


VIEW(volatile s32, gLastFrameCount);
#define gLastFrameCount gLastFrameCount_view

/* game.frame_rate 60: the elasticity is per frame of two retraces, e; a frame of one gets e^(1/2), so two of them
 * follow the car as one of two retraces does (docs/port/FRAMERATE.md 7). The N64's own scaling stays for 3-5. */
#define PC_CAM_ELASTIC(e) (pc_fps60 && gLastFrameCount == 1 ? __builtin_sqrtf(e) : (e))

/* Move the camera that shows car `car` toward the car's position pos offset by res, and turn it to look at the
 * car. With uvs given the car's object is first placed at pos with that orientation (update_car_object). The camera
 * keeps `elasticity` of its old position each frame (elastic_factor times 0.6, less on frames of 3-5 retraces so
 * it follows as fast in time; none while the car is respawning, when it jumps). */
void point_at_car(tCarData *car,f32 *pos,Mat3 *uvs,f32 *res)
{
    s32 i;
    f32 rpos[3];
    s32 camera=car->camera;
    if(uvs) update_car_object(car,pos,uvs);
    if(PC_MODEL_MOVING_STATE(car->slot)>=0) {
        elasticity[camera]=0.0f;
    } else {
        elasticity[camera]=PC_CAM_ELASTIC(elastic_factor[camera]*0.6f);
        if(gLastFrameCount==3) elasticity[camera]*=0.75f;
        else if(gLastFrameCount==4) elasticity[camera]*=0.6f;
        else if(gLastFrameCount==5) elasticity[camera]*=0.5f;
    }
    for(i=0;i<3;i++) {
        gCamera[camera].fcam.pos[i]=(gCamera[camera].fcam.pos[i]*elasticity[camera]+(pos[i]+res[i])*(1.0f-elasticity[camera]));
        rpos[i]=pos[i]-gCamera[camera].fcam.pos[i];
    }
    LookInDir(rpos,gCamera[camera].fcam.uvs);
}
