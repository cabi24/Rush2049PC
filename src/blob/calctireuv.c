#include "game/game.h"

typedef struct Vec3 {f32 x,y,z;} Vec3;
typedef struct Basis {Vec3 first,second,third;} Basis;

extern f32 cosf(f32),sinf(f32);
void calctireuv(const Vec3 *origin,const Vec3 *first,const Vec3 *second,f32 angle,const Basis *transform,const Basis *source,Basis *output,Vec3 *position) {
 Vec3 point;
 f32 pad[4];
 point.x=second->y*first->z-first->y*second->z;
 point.y=second->z*first->x-first->z*second->x;
 point.z=second->x*first->y-first->x*second->y;
 point.x=origin->x+point.x;
 point.y=origin->y+point.y;
 point.z=origin->z+point.z;
 BodyVector(&source->second,&output->second,transform);
 output->first.x=cosf(angle);
 output->first.y=0.0f;
 output->first.z=-sinf(angle);
 output->third.x=output->first.y*output->second.z-output->second.y*output->first.z;
 output->third.y=output->first.z*output->second.x-output->second.z*output->first.x;
 output->third.z=output->first.x*output->second.y-output->second.x*output->first.y;
 output->first.x=output->second.y*output->third.z-output->third.y*output->second.z;
 output->first.y=output->second.z*output->third.x-output->third.z*output->second.x;
 output->first.z=output->second.x*output->third.y-output->third.x*output->second.y;
 BodyVector(&point,position,output);
}
