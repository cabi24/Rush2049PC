/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned char u8;typedef signed char s8;typedef short s16;typedef unsigned short u16;
typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Tire92 {float values[23];} Tire92;
typedef struct Tune44 {float values[11];} Tune44;
typedef struct Collision16 {float front,rear,side,height;} Collision16;
enum n64p_Car { n64p_Car_ };
enum n64p_Tune44 { n64p_Tune44_ };
enum n64p_u16 { n64p_u16_ };
enum n64p_void { n64p_void_ };
typedef struct Car {u8 prefix[80];float mass,engine_inertia;u8 gap88[8];enum n64p_void tire_info[4];u8 gap112[8];Vec3 tire_positions[4];} Car;
typedef struct Vehicle2056 {
 enum n64p_Car car;
 enum n64p_Tune44 tune;
 u8 car_type;
 u8 gap9[2];
 s8 tune_type;
 u8 gap12[1];
 s8 traction_type;
 s8 mass_type;
 u8 gap15[57];
 float velocity;
 u8 gap76[12];
 Vec3 time;
 Vec3 center_moment;
 Vec3 tire_force[3];
 Vec3 tire_velocity[4];
 u8 gap196[48];
 Vec3 body_corner[4];
 u8 gap292[24];
 Vec3 delta;
 u8 gap328[192];
 Vec3 angular;
 u8 gap532[192];
 Vec3 gravity;
 u8 gap736[228];
 float steer[2];
 u8 gap972[8];
 float brake;
 u8 gap984[4];
 float brake_gain[4];
 u8 gap1004[10];
 s16 gear[2];
 u8 gap1018[10];
 float engine_torque;
 u8 gap1032[4];
 enum n64p_u16 torque_curve;
 float clutch;
 float drive_inertia;
 float trans_ratio;
 float total_ratio;
 float drive_values[4];
 Tire92 tires[4];
 u8 gap1440[8];
 u8 flags[2];
 u8 gap1450[2];
 float drag;
 float rolling_resistance;
 float torque_scale;
 float parameter1464;
 float mass;
 float mass_scale;
 float spring;
 float wheelbase;
 float compression[4];
 u8 gap1500[92];
 float inverse_dt;
 float damping;
 u8 gap1600[20];
 float collision_radius;
 u8 gap1624[216];
 u8 ignition;
 u8 gap1841[149];
 s16 active_index;
 u8 gap1992[4];
 s8 mode;
 u8 tail1997[59];
} Vehicle2056;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Car: (struct Car *)PC_N64P(x), enum n64p_Tune44: (Tune44 *)PC_N64P(x), enum n64p_u16: (u16 *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
extern Car *gCarModels[];
extern Tune44 gTune[];
/* PC: a table of 13-entry rows, indexed mode * 13 + type (mode up to 4); declared with its first row's size, an
 * optimizer may take every index for one below 13 (docs/port/SANITIZE.md, OPTIMIZED.md 2.7) */
extern s8 gCarWings[],gCarDifferential[];
extern float gWingSizes[],carDurability[],gTireSideForce[],gCollisionMass[],gCrashThreshold[],gCarBraking[];
extern float carsetup_durability_base,carsetup_crash_threshold_scale,gGravity;
extern u16 stdtorquecurve[10 * 12];   /* the torque curve, short[10][12] (enginetorque) */
/* After it, the two tire records (front and rear, six floats each: copy_tire_info copies them), which only the
 * physics descriptors' u32 tire_info fields reach. Declared and referenced here, so that the data loader swaps them
 * as floats; the torque curve's extent ran over them as u16s before (docs/port/BRINGUP.md 3). */
extern float D_8011F814[2][6];
float (*const pc_tire_records)[6] = D_8011F814;
extern Collision16 collsize[];
extern float CalcMass(Vehicle2056 *),CalcOffroadTires(Vehicle2056 *),CalcDrift(Vehicle2056 *);
extern void copy_tire_info(void *,Tire92 *),tire_constants(Vehicle2056 *,Tire92 *,float),steer_constants(Vehicle2056 *);
float sqrtf(float);
#pragma intrinsic (sqrtf)
float CalcTireSideForce(Vehicle2056 *m);
void carsetup(Vehicle2056 *m)
{
 Car *car;
 Collision16 *size;
 float mass_delta,max_length,resistance;
 int mode,type,i;
 if(m->mode==2)mode=m->active_index+1;else mode=0;
 type=m->car_type;
 car=gCarModels[type];
 m->tune=N64W(&gTune[m->tune_type]);
 m->car=N64W(car);
 m->parameter1464=gWingSizes[gCarWings[mode*13+type]];
 m->mass_scale=CalcMass(m);
 mass_delta=carDurability[m->mass_type]-carsetup_durability_base;
 type=m->car_type;
 m->spring=gCollisionMass[type]+mass_delta*1000.0f;
 m->damping=gCrashThreshold[type]+mass_delta*carsetup_crash_threshold_scale;
 m->mass=m->mass_scale*gGravity;
 m->flags[0]=gCarDifferential[type]&1;
 m->flags[1]=gCarDifferential[type]&2;
 m->torque_curve=N64W(stdtorquecurve);
 m->torque_scale=CalcOffroadTires(m);
 resistance=CalcDrift(m);m->drag=resistance;
 m->rolling_resistance=CalcTireSideForce(m);
 m->wheelbase=car->tire_positions[0].x-car->tire_positions[2].x;
 size=&collsize[m->car_type];
 m->body_corner[0].x=m->body_corner[2].x=size->side;
 m->body_corner[1].x=m->body_corner[3].x=-size->side;
 m->body_corner[0].y=m->body_corner[1].y=size->height;
 m->body_corner[2].y=m->body_corner[3].y=size->height;
 m->body_corner[0].z=m->body_corner[1].z=size->front;
 m->body_corner[2].z=m->body_corner[3].z=-size->rear;
 max_length=(size->rear<size->front)?size->front:size->rear;
 m->collision_radius=sqrtf(max_length*max_length+size->side*size->side+size->height*size->height);
 for(i=0;i<4;i++) {
  copy_tire_info(N64P(car->tire_info[i]),&m->tires[i]);
  if(i<2)tire_constants(m,&m->tires[i],-car->tire_positions[i+2].x);
  else tire_constants(m,&m->tires[i],car->tire_positions[i-2].x);
  m->tires[i].values[18]=m->velocity/m->tires[i].values[0];
  m->compression[i]=0.0f;
  m->tire_velocity[i].x=0.0f;m->tire_velocity[i].y=0.0f;m->tire_velocity[i].z=0.0f;
 }
 m->brake_gain[0]=m->mass*m->tires[0].values[4]*gCarBraking[m->car_type];
 m->brake_gain[1]=m->mass*m->tires[1].values[4]*gCarBraking[m->car_type];
 m->brake_gain[2]=m->mass*m->tires[2].values[4]*gCarBraking[m->car_type];
 m->brake_gain[3]=m->mass*m->tires[3].values[4]*gCarBraking[m->car_type];
 m->drive_inertia=m->tires[2].values[5]/2;
 steer_constants(m);
 m->gravity.z=0.0f;m->gravity.x=0.0f;m->gravity.y=-m->mass;
 m->ignition=0;m->gear[0]=0;m->gear[1]=0;
 m->inverse_dt=0.0f;
 m->time.x=0.0f;m->time.y=0.0f;m->time.z=0.0f;
 m->angular.x=0.0f;m->angular.y=0.0f;m->angular.z=0.0f;
 m->delta.x=0.0f;m->delta.y=0.0f;m->delta.z=0.0f;
 m->tire_force[0].x=0.0f;m->tire_force[0].y=0.0f;m->tire_force[0].z=0.0f;
 m->tire_force[1].x=0.0f;m->tire_force[1].y=0.0f;m->tire_force[1].z=0.0f;
 m->tire_force[2].x=0.0f;m->tire_force[2].y=0.0f;m->tire_force[2].z=0.0f;
 m->center_moment.x=0.0f;m->center_moment.y=0.0f;m->center_moment.z=0.0f;
 m->brake=0.0f;m->engine_torque=0.0f;
 m->drive_values[0]=0.0f;m->drive_values[1]=0.0f;m->drive_values[2]=0.0f;m->drive_values[3]=0.0f;
 m->clutch=0.0f;m->trans_ratio=0.0f;m->total_ratio=0.0f;
}
