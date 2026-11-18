/* forces1.c: the tire forces and the resistances on a car (MAT3 drivsym.c, forces1()).
 *
 * A stage of the physics step (regular, sym): each tire's frame and contact-patch velocity (calctireuv), its force
 * (dotireforce), a moving surface's push, the wind and the drag. forces2 then sums them. */
#include "game/game.h"

typedef struct Vec3 {float x,y,z;} Vec3;
/* An orientation matrix by rows: the frame's unit vectors. */
typedef struct Basis {Vec3 rows[3];} Basis;

/* The forces on model m's tires and body for this step, in the car's frame:
 * - each tire's frame and contact-patch velocity (the front two steered), then its force from the suspension and
 *   the road (dotireforce: the front pair, then the rear pair, with poor traction when the rear tires are on dirt or
 *   water); the throttle eases the front tires' braking force (1 - throttle of it);
 * - a moving surface under the car (MovingSurfaceForce);
 * - the wind (gWind, the cheat), sideways and up, while all four tires are in the air outside a tunnel;
 * - the drag along the car, against its motion: the rolling resistance, a constant (30, or 5000 for a hulk) and the
 *   air drag (srefpcybo2, or 10 for a hulk) times the square of the speed through the air; times the wing's drag
 *   with the wing out; and a drag in proportion to the speed on dirt, on water or for off-road tires on the road. */
void forces1(tModelData *m)
{
 int poortract;
 float speed,drag_base,drag_scale;
 poortract=0;
 /* each tire's arm, the descriptor's tireR[i] (three floats), is calctireuv's vector r */
 calctireuv(&m->v,&m->w,(Vec3 *)m->car->tireR[0],m->steerangle,&m->uvs,&m->roaduvs[0],&m->tireuvs[0],&m->tireV[0]);
 calctireuv(&m->v,&m->w,(Vec3 *)m->car->tireR[1],m->steerangle,&m->uvs,&m->roaduvs[1],&m->tireuvs[1],&m->tireV[1]);
 calctireuv(&m->v,&m->w,(Vec3 *)m->car->tireR[2],0.0f,&m->uvs,&m->roaduvs[2],&m->tireuvs[2],&m->tireV[2]);
 calctireuv(&m->v,&m->w,(Vec3 *)m->car->tireR[3],0.0f,&m->uvs,&m->roaduvs[3],&m->tireuvs[3],&m->tireV[3]);
 dotireforce(m,&m->tireV[0],&m->tireuvs[0],&m->tires[0],m->torque[0],&m->tireForce[0],m->suscomp[0],m->suscomp[1],m->car->springrate[0],m->car->farspringrate,m->car->cdamping[0],m->car->rdamping[0],poortract);
 dotireforce(m,&m->tireV[1],&m->tireuvs[1],&m->tires[1],m->torque[1],&m->tireForce[1],m->suscomp[1],m->suscomp[0],m->car->springrate[1],m->car->farspringrate,m->car->cdamping[1],m->car->rdamping[1],poortract);
 {
 float tsc;
 tsc=(float)1.0-m->throttle;
 if(m->tireForce[0][2]<0.0f)m->tireForce[0][2]*=tsc;
 if(m->tireForce[1][2]<0.0f)m->tireForce[1][2]*=tsc;
 }
 /* Original game: the second test reads tire 0 where tire 3 is meant (MAT3 has it too) */
 poortract=((m->roadcode[2]==ROADCODE_DIRT || m->roadcode[2]==ROADCODE_WATER) && (m->roadcode[3]==ROADCODE_DIRT || m->roadcode[0]==ROADCODE_WATER));
 dotireforce(m,&m->tireV[2],&m->tireuvs[2],&m->tires[2],m->torque[2],&m->tireForce[2],m->suscomp[2],m->suscomp[3],m->car->springrate[2],m->car->farspringrate,m->car->cdamping[2],m->car->rdamping[2],poortract);
 dotireforce(m,&m->tireV[3],&m->tireuvs[3],&m->tires[3],m->torque[3],&m->tireForce[3],m->suscomp[3],m->suscomp[2],m->car->springrate[3],m->car->farspringrate,m->car->cdamping[3],m->car->rdamping[3],poortract);
 if(m->movingSurface)MovingSurfaceForce(m);
 /* PC_CAR_IN_TUNNEL is the car record's in_tunnel (MAT3): no wind in a tunnel */
 if(!PC_CAR_IN_TUNNEL(m->slot) && m->roadcode[0]==ROADCODE_AIRBORNE && m->roadcode[1]==ROADCODE_AIRBORNE && m->roadcode[2]==ROADCODE_AIRBORNE && m->roadcode[3]==ROADCODE_AIRBORNE) {
  m->d[0]=((m->uvs[0][0]*gWind)*gWind)*80.0f;
  m->d[1]=((m->uvs[1][0]*gWind)*gWind)*80.0f;
 } else {m->d[0]=0.0f;m->d[1]=0.0f;}
 if(!PC_CAR_IN_TUNNEL(m->slot)) speed=m->v[2]-(m->uvs[2][0]*gWind)*20.0f;
 else speed=m->v[2];
 if(m->appearance&APPEAR_HULK) {drag_base=5000.0f;drag_scale=10.0f;}
 else {drag_base=30.0f;drag_scale=m->car->srefpcybo2;}
 if(speed>0.0f)m->d[2]=-(m->car->rollresist+drag_base+(drag_scale*speed)*speed);
 else m->d[2]=m->car->rollresist+drag_base+(drag_scale*speed)*speed;
 if(m->wing) {
  m->d[2]*=gWingDrag[gCarWings[m->drone_type==DRONE_TYPE_HUMAN ? m->slot+1 : 0][m->body_type]];
 }
 if(m->roadcode[3]==ROADCODE_DIRT && m->roadcode[2]==ROADCODE_DIRT) {
  m->d[2]+=(rear_dirt_drag_factor*(gSpecialTires==2 ? 0.1f : 1.0f-m->offroadtires))*speed;
 } else if(m->roadcode[3]==ROADCODE_WATER && m->roadcode[2]==ROADCODE_WATER) {
  m->d[2]+=(rear_water_drag_factor*(gSpecialTires==2 ? 0.1f : 1.0f-m->offroadtires))*speed;
 } else {
  m->d[2]+=(offroad_tires_drag_factor*(gSpecialTires==2 ? 0 : m->offroadtires))*speed;
 }
}
