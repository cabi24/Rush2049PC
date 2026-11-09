/* maxpath_cam: a camera that flies along the drone path ahead of the car (MAT3 camera.c). */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its speed, its drone path and its camera. */
typedef struct Object { u8 other0[248]; s16 mph; u8 other250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 other250_308[0x33C - 0x308]; s16 path; u8 other830[30]; s8 camera; } Object;

extern f32 sqrtf(f32);

/* Car `object`'s drone path camera (view 9). `update` 0 starts it at point (s32) position[0] of the car's drone
 * path; else the camera moves along the path from there at 100 feet a second plus the car's speed, 10 feet up, and
 * points at the car (`position`, its orientation into `matrix`: point_at_car). */
void maxpath_cam(s16 update,Object *object,f32 *position,Mat3 *matrix)
{
    s8 camera=object->camera;
    s32 next;
    Vec3 result,delta;
    s16 previous,path;
    f32 amount,length;
    if(!update) {
        maxpath_cam_start_point[camera]=maxpath_cam_last_point[camera]=(s32)position[0];
        maxpath_cam_start_path[camera]=object->path;
        maxpath_cam_start_time[camera]=gModelTime;
        maxpath_cam_last_dist[camera]=0.0f;
    } else {
        if(maxpath_cam_start_time[camera]>gModelTime) maxpath_cam_start_time[camera]=0.0f;
        path=maxpath_cam_start_path[camera];
        /* how much further along the path than the last point it reached */
        amount=(gModelTime-maxpath_cam_start_time[camera])*(100.0f+(object->mph>>2)*1.4666667f)-maxpath_cam_last_dist[camera];
        for(previous=maxpath_cam_last_point[camera];;) {
            next=NextPathPt(previous,path);
            delta[0]=gDronePaths[path].pts[next].pos[0]-gDronePaths[path].pts[previous].pos[0];
            delta[1]=gDronePaths[path].pts[next].pos[1]-gDronePaths[path].pts[previous].pos[1];
            delta[2]=gDronePaths[path].pts[next].pos[2]-gDronePaths[path].pts[previous].pos[2];
            length=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
            if(length<=amount) {
                maxpath_cam_last_dist[camera]+=length;
                previous=next;
                maxpath_cam_last_point[camera]=next;
                amount-=length;
            } else {
                result[0]=(amount/length)*delta[0]+gDronePaths[path].pts[previous].pos[0];
                result[1]=(amount/length)*delta[1]+gDronePaths[path].pts[previous].pos[1];
                result[2]=(amount/length)*delta[2]+gDronePaths[path].pts[previous].pos[2];
                break;
            }
        }
        result[1]+=10.0f;
        result[0]-=position[0];
        result[1]-=position[1];
        result[2]-=position[2];
        elastic_factor[camera]=0.0f;
        point_at_car(object,position,matrix,result);
    }
}
