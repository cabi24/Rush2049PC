#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
typedef struct Object { u8 other0[248]; s16 speed; u8 other250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 other250_308[0x33C - 0x308]; s16 track; u8 other830[30]; s8 slot; } Object;
typedef struct PathPoint { s16 x,y,z,pad; } PathPoint;
typedef struct { u16 count,other2; PathPoint *points; } Path;

VIEW(Path[], gDronePaths);
#define gDronePaths gDronePaths_view
extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

extern void point_at_car(Object *,f32 *,Mat3 *,f32 *);
void maxpath_cam(s16 update,Object *object,f32 *position,Mat3 *matrix)
{
    s8 slot=object->slot;
    s32 next;
    Vec3 result,delta;
    s16 previous,track;
    f32 amount,length;
    if(!update) {
        maxpath_cam_start_point[slot]=maxpath_cam_last_point[slot]=(s32)position[0];
        maxpath_cam_start_path[slot]=object->track;
        maxpath_cam_start_time[slot]=gModelTime;
        maxpath_cam_last_dist[slot]=0.0f;
    } else {
        if(maxpath_cam_start_time[slot]>gModelTime) maxpath_cam_start_time[slot]=0.0f;
        track=maxpath_cam_start_path[slot];
        amount=(gModelTime-maxpath_cam_start_time[slot])*(100.0f+(object->speed>>2)*1.4666667f)-maxpath_cam_last_dist[slot];
        for(previous=maxpath_cam_last_point[slot];;) {
            next=NextPathPt(previous,track);
            delta[0]=gDronePaths[track].points[next].x-gDronePaths[track].points[previous].x;
            delta[1]=gDronePaths[track].points[next].y-gDronePaths[track].points[previous].y;
            delta[2]=gDronePaths[track].points[next].z-gDronePaths[track].points[previous].z;
            length=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
            if(length<=amount) {
                maxpath_cam_last_dist[slot]+=length;
                previous=next;
                maxpath_cam_last_point[slot]=next;
                amount-=length;
            } else {
                result[0]=(amount/length)*delta[0]+gDronePaths[track].points[previous].x;
                result[1]=(amount/length)*delta[1]+gDronePaths[track].points[previous].y;
                result[2]=(amount/length)*delta[2]+gDronePaths[track].points[previous].z;
                break;
            }
        }
        result[1]+=10.0f;
        result[0]-=position[0];
        result[1]-=position[1];
        result[2]-=position[2];
        elastic_factor[slot]=0.0f;
        point_at_car(object,position,matrix,result);
    }
}
