#include "game/game.h"

typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Vehicle2056 {
 u8 prefix[244];Vec3 corners[4];u8 gap292[1328];float radius;
 u8 gap1624[316];Vec3 position;float basis[9];u8 gap1988[4];s16 active;
 u8 gap1994[32];s8 enabled;u8 tail[29];
} Vehicle2056;

extern void setFBCollisionForce(Vehicle2056 *,Vehicle2056 *,Vehicle2056 *,Vec3 *,Vec3 *,float);
void collision(Vehicle2056 *vehicle)
{
 Vehicle2056 *other;
 Vec3 delta,world,local;
 Vec3 *point;
 float *value;
 float length;
 int offset;
 if(gNoCarCollisions && gRace!=1)return;
 if(!vehicle->enabled)return;
 if(gRace==2)return;
 for(other=vehicle+1;other<PC_MODELS_END;other++) {
  if(!other->active || !other->enabled)continue;
  delta.x=vehicle->position.x-other->position.x;
  delta.y=vehicle->position.y-other->position.y;
  delta.z=vehicle->position.z-other->position.z;
  length=0.0f;
  for(value=(float *)&delta;value<(float *)(&delta+1);value++)length+=*value**value;
  if(length>(vehicle->radius+other->radius)*(vehicle->radius+other->radius))continue;
  for(offset=0,point=vehicle->corners;offset<(int)sizeof(vehicle->corners);offset+=(int)sizeof(Vec3),point++) {
   WorldVector(point,&world,vehicle->basis);
   world.x=vehicle->position.x+world.x;world.y=vehicle->position.y+world.y;world.z=vehicle->position.z+world.z;
   world.x-=other->position.x;world.y-=other->position.y;world.z-=other->position.z;
   BodyVector(&world,&local,other->basis);
   if(PointInBody(other,&local,5.0f)) {
    setFBCollisionForce(vehicle,vehicle,other,&delta,&local,5.0f);return;
   }
  }
  for(offset=0,point=other->corners;offset!=(int)sizeof(other->corners);offset+=(int)sizeof(Vec3),point++) {
   WorldVector(point,&world,other->basis);
   world.x=other->position.x+world.x;world.y=other->position.y+world.y;world.z=other->position.z+world.z;
   world.x-=vehicle->position.x;world.y-=vehicle->position.y;world.z-=vehicle->position.z;
   BodyVector(&world,&local,vehicle->basis);
   if(PointInBody(vehicle,&local,5.0f)) {
    setFBCollisionForce(vehicle,other,vehicle,&delta,&local,5.0f);return;
   }
  }
 }
}
