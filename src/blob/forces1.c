/* forces1.c: the tire forces and the resistances on a car (MAT3 drivsym.c, forces1()).
 *
 * A stage of the physics step (regular, sym): each tire's frame and contact-patch velocity (calctireuv), its force
 * (dotireforce), a moving surface's push, the wind and the drag. forces2 then sums them. */
#include "game/game.h"

typedef struct Vec3 {float x,y,z;} Vec3;
/* An orientation matrix by rows: the frame's unit vectors. */
typedef struct Basis {Vec3 rows[3];} Basis;
/* The car's physics descriptor (MAT3 _tCar, 0xB4 bytes). */
typedef struct tCar {
 float i[3],invi[3];            /* +0x00 the moments of inertia and their inverses */
 float springrate[4];           /* +0x18 each corner's spring rate */
 float farspringrate,rarspringrate;   /* +0x28 the front and rear anti-roll bars */
 float cdamping[4],rdamping[4]; /* +0x30 each corner's compression and rebound damping */
 float steerratio,swtpg;        /* +0x50 */
 float srefpcybo2;              /* +0x58 the air drag coefficient */
 float rollresist;              /* +0x5C the rolling resistance */
 u32 tires[4];                  /* +0x60 each tire's constants (window addresses) */
 Vec3 tireR[4];                 /* +0x70 each tire's position from the centre of mass */
} tCar;
enum n64p_tCar { n64p_tCar_ };
enum n64p_void { n64p_void_ };
/* The model record (tModelData), the fields forces1 reads and writes. */
typedef struct tModelData {
 enum n64p_tCar car;u8 pad4[4];u8 body_type;u8 pad9[0x40 - 0x9];   /* +0x008 the car, 0..12 */
 Vec3 v,w;                      /* +0x040 the velocity and the angular velocity, in the car's frame */
 float d[3];                    /* +0x058 the drag and wind force: [0] sideways, [1] up, [2] along */
 Vec3 tireForce[4],tireV[4];    /* +0x064 each tire's force; +0x094 its contact patch's velocity */
 u8 padC4[0x178 - 0xC4];Basis tireuvs[4];   /* +0x178 each tire's frame */
 u8 pad208[0x2EC - 0x208];Basis uvs,roaduvs[4];   /* +0x2EC the orientation; +0x310 the road's under each tire */
 u8 pad3A0[0x3B0 - 0x3A0];float steerangle;      /* +0x3B0 the front wheels' angle, radians */
 float torque[4];               /* +0x3B4 each wheel's drive torque */
 u8 pad3C4[0x3D0 - 0x3C4];float throttle;        /* +0x3D0 0..1 */
 u8 pad3D4[0x3F8 - 0x3D4];s16 wing;              /* +0x3F8 the wing deployed */
 u8 pad3FA[0x430 - 0x3FA];u8 tires[4][92];       /* +0x430 the tires (anon_tradius_springK_5c) */
 u8 pad5A0[0x5B4 - 0x5A0];float offroadtires;    /* +0x5B4 the tires' off-road factor (CalcOffroadTires) */
 u8 pad5B8[0x5CC - 0x5B8];float suscomp[4];      /* +0x5CC each corner's suspension compression */
 u8 pad5DC[0x60C - 0x5DC];int roadcode[4];       /* +0x60C each tire's road code */
 u8 pad61C[0x648 - 0x61C];enum n64p_void movingSurface;   /* +0x648 the moving surface under it, a window address */
 u8 pad64C[0x7C6 - 0x64C];s16 slot;              /* +0x7C6 */
 u8 pad7C8[0x7CC - 0x7C8];s8 drone_type;         /* +0x7CC DRONE_TYPE_* */
 u8 pad7CD[0x7D4 - 0x7CD];u32 appearance;u8 tail[0x808 - 0x7D8];   /* +0x7D4 APPEAR_* */
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tCar: (tCar *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

extern void dotireforce(tModelData *,Vec3 *,Basis *,void *,float,Vec3 *,float,float,float,float,float,float,int,float);

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
 float airfact;
 float speed,drag_base,drag_scale;
 airfact=1.0f;
 poortract=0;
 calctireuv(&m->v,&m->w,&N64P(m->car)->tireR[0],m->steerangle,&m->uvs,&m->roaduvs[0],&m->tireuvs[0],&m->tireV[0]);
 calctireuv(&m->v,&m->w,&N64P(m->car)->tireR[1],m->steerangle,&m->uvs,&m->roaduvs[1],&m->tireuvs[1],&m->tireV[1]);
 calctireuv(&m->v,&m->w,&N64P(m->car)->tireR[2],0.0f,&m->uvs,&m->roaduvs[2],&m->tireuvs[2],&m->tireV[2]);
 calctireuv(&m->v,&m->w,&N64P(m->car)->tireR[3],0.0f,&m->uvs,&m->roaduvs[3],&m->tireuvs[3],&m->tireV[3]);
 dotireforce(m,&m->tireV[0],&m->tireuvs[0],m->tires[0],m->torque[0],&m->tireForce[0],m->suscomp[0],m->suscomp[1],N64P(m->car)->springrate[0],N64P(m->car)->farspringrate,N64P(m->car)->cdamping[0],N64P(m->car)->rdamping[0],poortract,airfact);
 dotireforce(m,&m->tireV[1],&m->tireuvs[1],m->tires[1],m->torque[1],&m->tireForce[1],m->suscomp[1],m->suscomp[0],N64P(m->car)->springrate[1],N64P(m->car)->farspringrate,N64P(m->car)->cdamping[1],N64P(m->car)->rdamping[1],poortract,airfact);
 {
 float tsc;
 tsc=(float)1.0-m->throttle;
 if(m->tireForce[0].z<0.0f)m->tireForce[0].z*=tsc;
 if(m->tireForce[1].z<0.0f)m->tireForce[1].z*=tsc;
 }
 /* Original game: the second test reads tire 0 where tire 3 is meant (MAT3 has it too) */
 poortract=((m->roadcode[2]==ROADCODE_DIRT || m->roadcode[2]==ROADCODE_WATER) && (m->roadcode[3]==ROADCODE_DIRT || m->roadcode[0]==ROADCODE_WATER));
 dotireforce(m,&m->tireV[2],&m->tireuvs[2],m->tires[2],m->torque[2],&m->tireForce[2],m->suscomp[2],m->suscomp[3],N64P(m->car)->springrate[2],N64P(m->car)->farspringrate,N64P(m->car)->cdamping[2],N64P(m->car)->rdamping[2],poortract,airfact);
 dotireforce(m,&m->tireV[3],&m->tireuvs[3],m->tires[3],m->torque[3],&m->tireForce[3],m->suscomp[3],m->suscomp[2],N64P(m->car)->springrate[3],N64P(m->car)->farspringrate,N64P(m->car)->cdamping[3],N64P(m->car)->rdamping[3],poortract,airfact);
 if(m->movingSurface)MovingSurfaceForce(m);
 /* PC_CAR_DISABLED is the car record's in_tunnel (MAT3): no wind in a tunnel */
 if(!PC_CAR_DISABLED(m->slot) && m->roadcode[0]==ROADCODE_AIRBORNE && m->roadcode[1]==ROADCODE_AIRBORNE && m->roadcode[2]==ROADCODE_AIRBORNE && m->roadcode[3]==ROADCODE_AIRBORNE) {
  m->d[0]=((m->uvs.rows[0].x*gWind)*gWind)*80.0f;
  m->d[1]=((m->uvs.rows[1].x*gWind)*gWind)*80.0f;
 } else {m->d[0]=0.0f;m->d[1]=0.0f;}
 if(!PC_CAR_DISABLED(m->slot)) speed=m->v.z-(m->uvs.rows[2].x*gWind)*20.0f;
 else speed=m->v.z;
 if(m->appearance&APPEAR_HULK) {drag_base=5000.0f;drag_scale=10.0f;}
 else {drag_base=30.0f;drag_scale=N64P(m->car)->srefpcybo2;}
 if(speed>0.0f)m->d[2]=-(N64P(m->car)->rollresist+drag_base+(drag_scale*speed)*speed);
 else m->d[2]=N64P(m->car)->rollresist+drag_base+(drag_scale*speed)*speed;
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
