#include "game/game.h"

typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Basis {Vec3 rows[3];} Basis;
typedef struct Parameters {float values[40];} Parameters;
enum n64p_Parameters { n64p_Parameters_ };
enum n64p_void { n64p_void_ };
typedef struct VehicleModel {
 enum n64p_Parameters parameters;u8 gap4[60];Vec3 velocity,angular;float wind_lateral,wind_vertical,drag;
 Vec3 tire_force[4],tire_velocity[4];u8 gap196[180];Basis tire_basis[4];
 u8 gap520[228];Basis transform,road_basis[4];u8 gap928[16];float steering;
 float wheel_torque[4];u8 gap964[12];float throttle;u8 gap980[36];s16 condition;
 u8 gap1018[54];u8 wheel_state[4][92];u8 gap1440[20];float slip;
 u8 gap1464[20];float suspension[4];u8 gap1500[48];int wheel_mode[4];
 u8 gap1564[44];enum n64p_void direction;u8 gap1612[378];s16 index;
 u8 gap1992[4];s8 mode;u8 gap1997[7];u32 flags;u8 tail[48];
} VehicleModel;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Parameters: (Parameters *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

extern void dotireforce(VehicleModel *,Vec3 *,Basis *,void *,float,Vec3 *,float,float,float,float,float,float,int,float);

void forces1(VehicleModel *vehicle)
{
 /* Original forces1 declarations from drivsym.c; the N64 game-over
    drag block was removed, leaving its source-owned local declarations. */
 int poortract;
 float airfact;
 float speed,drag_base,drag_scale;
 airfact=1.0f;
 poortract=0;
 calctireuv(&vehicle->velocity,&vehicle->angular,(Vec3 *)&N64P(vehicle->parameters)->values[28],vehicle->steering,&vehicle->transform,&vehicle->road_basis[0],&vehicle->tire_basis[0],&vehicle->tire_velocity[0]);
 calctireuv(&vehicle->velocity,&vehicle->angular,(Vec3 *)&N64P(vehicle->parameters)->values[31],vehicle->steering,&vehicle->transform,&vehicle->road_basis[1],&vehicle->tire_basis[1],&vehicle->tire_velocity[1]);
 calctireuv(&vehicle->velocity,&vehicle->angular,(Vec3 *)&N64P(vehicle->parameters)->values[34],0.0f,&vehicle->transform,&vehicle->road_basis[2],&vehicle->tire_basis[2],&vehicle->tire_velocity[2]);
 calctireuv(&vehicle->velocity,&vehicle->angular,(Vec3 *)&N64P(vehicle->parameters)->values[37],0.0f,&vehicle->transform,&vehicle->road_basis[3],&vehicle->tire_basis[3],&vehicle->tire_velocity[3]);
 dotireforce(vehicle,&vehicle->tire_velocity[0],&vehicle->tire_basis[0],vehicle->wheel_state[0],vehicle->wheel_torque[0],&vehicle->tire_force[0],vehicle->suspension[0],vehicle->suspension[1],N64P(vehicle->parameters)->values[6],N64P(vehicle->parameters)->values[10],N64P(vehicle->parameters)->values[12],N64P(vehicle->parameters)->values[16],poortract,airfact);
 dotireforce(vehicle,&vehicle->tire_velocity[1],&vehicle->tire_basis[1],vehicle->wheel_state[1],vehicle->wheel_torque[1],&vehicle->tire_force[1],vehicle->suspension[1],vehicle->suspension[0],N64P(vehicle->parameters)->values[7],N64P(vehicle->parameters)->values[10],N64P(vehicle->parameters)->values[13],N64P(vehicle->parameters)->values[17],poortract,airfact);
 {
 float tsc;
 tsc=(float)1.0-vehicle->throttle;
 if(vehicle->tire_force[0].z<0.0f)vehicle->tire_force[0].z*=tsc;
 if(vehicle->tire_force[1].z<0.0f)vehicle->tire_force[1].z*=tsc;
 }
 poortract=((vehicle->wheel_mode[2]==1 || vehicle->wheel_mode[2]==2) && (vehicle->wheel_mode[3]==1 || vehicle->wheel_mode[0]==2));
 dotireforce(vehicle,&vehicle->tire_velocity[2],&vehicle->tire_basis[2],vehicle->wheel_state[2],vehicle->wheel_torque[2],&vehicle->tire_force[2],vehicle->suspension[2],vehicle->suspension[3],N64P(vehicle->parameters)->values[8],N64P(vehicle->parameters)->values[10],N64P(vehicle->parameters)->values[14],N64P(vehicle->parameters)->values[18],poortract,airfact);
 dotireforce(vehicle,&vehicle->tire_velocity[3],&vehicle->tire_basis[3],vehicle->wheel_state[3],vehicle->wheel_torque[3],&vehicle->tire_force[3],vehicle->suspension[3],vehicle->suspension[2],N64P(vehicle->parameters)->values[9],N64P(vehicle->parameters)->values[10],N64P(vehicle->parameters)->values[15],N64P(vehicle->parameters)->values[19],poortract,airfact);
 if(vehicle->direction)MovingSurfaceForce(vehicle);
 if(!PC_CAR_DISABLED(vehicle->index) && vehicle->wheel_mode[0]==8 && vehicle->wheel_mode[1]==8 && vehicle->wheel_mode[2]==8 && vehicle->wheel_mode[3]==8) {
  vehicle->wind_lateral=((vehicle->transform.rows[0].x*gWind)*gWind)*80.0f;
  vehicle->wind_vertical=((vehicle->transform.rows[1].x*gWind)*gWind)*80.0f;
 } else {vehicle->wind_lateral=0.0f;vehicle->wind_vertical=0.0f;}
 if(!PC_CAR_DISABLED(vehicle->index)) speed=vehicle->velocity.z-(vehicle->transform.rows[2].x*gWind)*20.0f;
 else speed=vehicle->velocity.z;
 if(vehicle->flags&0x10) {drag_base=5000.0f;drag_scale=10.0f;}
 else {drag_base=30.0f;drag_scale=N64P(vehicle->parameters)->values[22];}
 if(speed>0.0f)vehicle->drag=-(N64P(vehicle->parameters)->values[23]+drag_base+(drag_scale*speed)*speed);
 else vehicle->drag=N64P(vehicle->parameters)->values[23]+drag_base+(drag_scale*speed)*speed;
 if(vehicle->condition) {
  vehicle->drag*=gWingDrag[gCarWings[vehicle->mode==2 ? vehicle->index+1 : 0][((u8 *)vehicle)[8]]];
 }
 if(vehicle->wheel_mode[3]==1 && vehicle->wheel_mode[2]==1) {
  vehicle->drag+=(rear_dirt_drag_factor*(gSpecialTires==2 ? 0.1f : 1.0f-vehicle->slip))*speed;
 } else if(vehicle->wheel_mode[3]==2 && vehicle->wheel_mode[2]==2) {
  vehicle->drag+=(rear_water_drag_factor*(gSpecialTires==2 ? 0.1f : 1.0f-vehicle->slip))*speed;
 } else {
  vehicle->drag+=(offroad_tires_drag_factor*(gSpecialTires==2 ? 0 : vehicle->slip))*speed;
 }
}
