#include "game/game.h"

typedef f32 Mat3[3][3];
typedef struct Object { u8 other[0x110 - 0x0]; struct pc_visual visuals[21]; u8 other_308[0x35B - 0x308]; s8 car,slot; } Object;

typedef struct { u8 other[96];Mat3 matrix;f32 position[3];u8 tail[8]; } Slot152;

VIEW(volatile s32, gLastFrameCount);
#define gLastFrameCount gLastFrameCount_view
VIEW(Slot152[], gCamera);
#define gCamera gCamera_view

/* game.frame_rate 60: the elasticity is per frame of two retraces, e; a frame of one gets e^(1/2), so two of them
 * follow the car as one of two retraces does (docs/port/FRAMERATE.md 7). The N64's own scaling stays for 3-5. */
#define PC_CAM_ELASTIC(e) (pc_fps60 && gLastFrameCount == 1 ? __builtin_sqrtf(e) : (e))
void point_at_car(Object *object,f32 *carpos,Mat3 *matrix,f32 *res)
{
    s32 i;
    f32 rpos[3];
    s32 slot=object->slot;
    if(matrix) update_car_object(object,carpos,matrix);
    if(PC_MODEL_MOVING_STATE(object->car)>=0) {
        elasticity[slot]=0.0f;
    } else {
        elasticity[slot]=PC_CAM_ELASTIC(elastic_factor[slot]*0.6f);
        if(gLastFrameCount==3) elasticity[slot]*=0.75f;
        else if(gLastFrameCount==4) elasticity[slot]*=0.6f;
        else if(gLastFrameCount==5) elasticity[slot]*=0.5f;
    }
    for(i=0;i<3;i++) {
        gCamera[slot].position[i]=(gCamera[slot].position[i]*elasticity[slot]+(carpos[i]+res[i])*(1.0f-elasticity[slot]));
        rpos[i]=carpos[i]-gCamera[slot].position[i];
    }
    LookInDir(rpos,gCamera[slot].matrix);
}
