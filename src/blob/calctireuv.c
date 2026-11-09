/* calctireuv.c: a tire's orientation and velocity (MAT3 tires.c, calctireuv()).
 *
 * A stage of the tire forces (forces1): before dotireforce, each tire gets its own frame, the road's normal as its
 * up axis turned by the steering angle, and the velocity of its contact patch in that frame. */
#include "game/game.h"

typedef struct Vec3 {f32 x,y,z;} Vec3;
/* An orientation matrix by rows: the frame's unit vectors (uvs[0], [1], [2] elsewhere). */
typedef struct Basis {Vec3 side,up,forward;} Basis;

extern f32 cosf(f32),sinf(f32);

/* The tire's frame and its velocity. v is the car's velocity and w its angular velocity, both in the car's frame;
 * r the tire's position from the car's centre (the tire arm). steer is the tire's steering angle in radians,
 * caruvs the car's orientation and roaduvs the road's under the tire (both world frames). Out: tireuvs, the tire's
 * frame in the car's (up the road's normal, side the steered axle, forward their cross product), and tirev, the
 * contact patch's velocity v + w x r in the tire's frame. */
void calctireuv(const Vec3 *v,const Vec3 *w,const Vec3 *r,f32 steer,const Basis *caruvs,const Basis *roaduvs,Basis *tireuvs,Vec3 *tirev) {
 Vec3 cartirev;
 f32 pad[4];   /* unused: the frame's layout */
 cartirev.x=r->y*w->z-w->y*r->z;
 cartirev.y=r->z*w->x-w->z*r->x;
 cartirev.z=r->x*w->y-w->x*r->y;
 cartirev.x=v->x+cartirev.x;
 cartirev.y=v->y+cartirev.y;
 cartirev.z=v->z+cartirev.z;
 BodyVector(&roaduvs->up,&tireuvs->up,caruvs);
 tireuvs->side.x=cosf(steer);
 tireuvs->side.y=0.0f;
 tireuvs->side.z=-sinf(steer);
 tireuvs->forward.x=tireuvs->side.y*tireuvs->up.z-tireuvs->up.y*tireuvs->side.z;
 tireuvs->forward.y=tireuvs->side.z*tireuvs->up.x-tireuvs->up.z*tireuvs->side.x;
 tireuvs->forward.z=tireuvs->side.x*tireuvs->up.y-tireuvs->up.x*tireuvs->side.y;
 tireuvs->side.x=tireuvs->up.y*tireuvs->forward.z-tireuvs->forward.y*tireuvs->up.z;
 tireuvs->side.y=tireuvs->up.z*tireuvs->forward.x-tireuvs->forward.z*tireuvs->up.x;
 tireuvs->side.z=tireuvs->up.x*tireuvs->forward.y-tireuvs->forward.x*tireuvs->up.y;
 BodyVector(&cartirev,tirev,tireuvs);
}
