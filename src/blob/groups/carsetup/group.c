/* carsetup/group.c: a car's constants from its parts (MAT3 initiali.c, carsetup()).
 *
 * initialize calls it when a car starts or respawns: the physics descriptor and tune record for its body and
 * handling choice, its weight, collision box, tires, brakes and steering, and the drivetrain's state cleared. */
#include "game/game.h"

typedef struct Vec3 {float x,y,z;} Vec3;
/* A tire in the model record (MAT3 anon_tradius_springK_5c, tModelData.tires[i]). */
typedef struct Tire {
 float tradius,springK,rubdamp,cstiff,cfmax,invmi,zforce,afmax,k1,k2,k3,l2,l3,m1,m2,m3,m4,patchy;
 float angvel,sliptorque,sideforce,traction;   /* +0x48 the wheel's spin, radians a second */
 float slipflag_word;                          /* +0x58 slipflag, an s8, and padding */
} Tire;

enum n64p_tCar { n64p_tCar_ };
enum n64p_Tune44 { n64p_Tune44_ };
enum n64p_u16 { n64p_u16_ };
enum n64p_void { n64p_void_ };
/* The car's physics descriptor (MAT3 _tCar, gCarModels[body]). */
typedef struct tCar {u8 pad0[0x50];float steerratio,swtpg;u8 pad58[8];enum n64p_void tires[4];   /* +0x60 the tire
 * constants (window addresses) */ Vec3 tireR[4];   /* +0x70 each tire's position from the centre of mass */} tCar;
/* The model record (tModelData), the fields carsetup reads and writes. */
typedef struct tModelData {
 enum n64p_tCar car;            /* +0x000 the physics descriptor (a window address) */
 enum n64p_Tune44 tune;         /* +0x004 the tune record (a window address) */
 u8 body_type;                  /* +0x008 the car, 0..12 */
 u8 pad9[2];
 s8 handling;                   /* +0x00B */
 u8 padC[1];
 s8 tire_type;                  /* +0x00D */
 s8 frame;                      /* +0x00E */
 u8 padF[0x48 - 0xF];
 float v2;                      /* +0x048 v[2], the forward speed */
 u8 pad4C[0x58 - 0x4C];
 Vec3 d;                        /* +0x058 the drag (forces1) */
 Vec3 tireForce[4];             /* +0x064 */
 Vec3 tireV[4];                 /* +0x094 */
 u8 padC4[0xF4 - 0xC4];
 Vec3 bodyR[4];                 /* +0x0F4 the body's corners, car frame */
 u8 pad124[0x13C - 0x124];
 Vec3 centerMoment;             /* +0x13C */
 u8 pad148[0x208 - 0x148];
 Vec3 g;                        /* +0x208 gravity, car frame */
 u8 pad214[0x2D4 - 0x214];
 Vec3 grw;                      /* +0x2D4 gravity, world frame */
 u8 pad2E0[0x3C4 - 0x2E0];
 float steergain,trail;         /* +0x3C4 (steer_constants) */
 u8 pad3CC[0x3D4 - 0x3CC];
 float brake;                   /* +0x3D4 */
 u8 pad3D8[0x3DC - 0x3D8];
 float brakegain[4];            /* +0x3DC each tire's braking force at full brake */
 u8 pad3EC[0x3F6 - 0x3EC];
 s16 commandgear,wing;          /* +0x3F6, +0x3F8 */
 u8 pad3FA[0x404 - 0x3FA];
 float engtorque;               /* +0x404 */
 u8 pad408[0x40C - 0x408];
 enum n64p_u16 torquecurve;     /* +0x40C the torque curve (a window address) */
 float dwtorque,dwinvmi,efdwinvmi,dwangvel,clutchtorque,clutchangvel,transratio,totalratio;   /* +0x410 the
 * drivetrain (whatslips, transmission) */
 Tire tires[4];                 /* +0x430 */
 u8 pad5A0[0x5A8 - 0x5A0];
 u8 magicdiff,nothrusttorque;   /* +0x5A8 the differential's two kinds of help */
 u8 pad5AA[2];
 float drift;                   /* +0x5AC (CalcDrift) */
 float sidefk;                  /* +0x5B0 (CalcTireSideForce) */
 float offroadtires;            /* +0x5B4 (CalcOffroadTires) */
 float wingEffect;              /* +0x5B8 the wing's size */
 float weight;                  /* +0x5BC mass times gravity */
 float mass;                    /* +0x5C0 (CalcMass) */
 float collisionmass;           /* +0x5C4 the mass in car-to-car collisions */
 float wheelbase;               /* +0x5C8 */
 float suscomp[4];              /* +0x5CC */
 u8 pad5DC[0x638 - 0x5DC];
 float idt;                     /* +0x638 1 / dt */
 float crashthreshold;          /* +0x63C the impact that wrecks the car */
 u8 pad640[0x654 - 0x640];
 float colrad;                  /* +0x654 the collision sphere's radius */
 u8 pad658[0x730 - 0x658];
 u8 mainin_gear;                /* +0x730 mainin.gear */
 u8 pad731[0x7C6 - 0x731];
 s16 slot;                      /* +0x7C6 */
 u8 pad7C8[0x7CC - 0x7C8];
 s8 drone_type;                 /* +0x7CC DRONE_TYPE_* */
 u8 tail7CD[0x808 - 0x7CD];
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tCar: (tCar *)PC_N64P(x), enum n64p_Tune44: (Tune44 *)PC_N64P(x), enum n64p_u16: (u16 *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

/* The tables as the code indexes them: their own storage, through other types (include/game/view.h). */
VIEW(s8[], gCarWings);
#define gCarWings gCarWings_view

VIEW(u16[10 * 12], stdtorquecurve);
#define stdtorquecurve stdtorquecurve_view
VIEW(float[2][6], frontTire);
#define frontTire frontTire_view
/* A PC datum: frontTire's address (the descriptors' tire constants), declared for the data loader in
 * port/data/game_decls.c. */
float (*const pc_tire_records)[6] = frontTire;

float sqrtf(float);

/* Fill model m from its parts: the descriptor gCarModels[body] and the tune gTune[handling] (as window addresses);
 * the wing's size (a human's own wings, row slot + 1 of gCarWings, the drones' row 0); the mass (CalcMass), weight
 * and the durability's share of the collision mass (1000 per step) and crash threshold
 * (carsetup_crash_threshold_scale); the differential's flags (gCarDifferential bit 0 magic, bit 1 no thrust torque);
 * the torque curve; the tire factors; the wheelbase (front tire to rear along the car); the body box from collsize
 * and its bounding sphere; each tire's constants from the descriptor (copy_tire_info, tire_constants with the other
 * axle's distance) and its spin matching the speed; the brakes (weight times the tire's friction times
 * gCarBraking); the driveline's inertia, half a rear tire's; the steering (steer_constants); gravity; then the gear,
 * wing, drag, forces and the drivetrain's state cleared. */
void carsetup(tModelData *m)
{
 tCar *car;
 Collision16 *size;
 float durability,max_length,drift;
 int row,body,i;
 if(m->drone_type==DRONE_TYPE_HUMAN)row=m->slot+1;else row=0;
 body=m->body_type;
 car=gCarModels[body];
 m->tune=N64W(&gTune[m->handling]);
 m->car=N64W(car);
 m->wingEffect=gWingSizes[gCarWings[row*13+body]];
 m->mass=CalcMass(m);
 durability=carDurability[m->frame]-carsetup_durability_base;
 body=m->body_type;
 m->collisionmass=gCollisionMass[body]+durability*1000.0f;
 m->crashthreshold=gCrashThreshold[body]+durability*carsetup_crash_threshold_scale;
 m->weight=m->mass*gGravity;
 m->magicdiff=gCarDifferential[body]&1;
 m->nothrusttorque=gCarDifferential[body]&2;
 m->torquecurve=N64W(stdtorquecurve);
 m->offroadtires=CalcOffroadTires(m);
 drift=CalcDrift(m);m->drift=drift;
 m->sidefk=CalcTireSideForce(m);
 m->wheelbase=car->tireR[0].z-car->tireR[2].z;
 size=&collsize[m->body_type];
 m->bodyR[0].x=m->bodyR[2].x=size->side;
 m->bodyR[1].x=m->bodyR[3].x=-size->side;
 m->bodyR[0].y=m->bodyR[1].y=size->height;
 m->bodyR[2].y=m->bodyR[3].y=size->height;
 m->bodyR[0].z=m->bodyR[1].z=size->front;
 m->bodyR[2].z=m->bodyR[3].z=-size->rear;
 max_length=(size->rear<size->front)?size->front:size->rear;
 m->colrad=sqrtf(max_length*max_length+size->side*size->side+size->height*size->height);
 for(i=0;i<4;i++) {
  copy_tire_info(N64P(car->tires[i]),&m->tires[i]);
  if(i<2)tire_constants(m,&m->tires[i],-car->tireR[i+2].z);
  else tire_constants(m,&m->tires[i],car->tireR[i-2].z);
  m->tires[i].angvel=m->v2/m->tires[i].tradius;
  m->suscomp[i]=0.0f;
  m->tireV[i].x=0.0f;m->tireV[i].y=0.0f;m->tireV[i].z=0.0f;
 }
 m->brakegain[0]=m->weight*m->tires[0].cfmax*gCarBraking[m->body_type];
 m->brakegain[1]=m->weight*m->tires[1].cfmax*gCarBraking[m->body_type];
 m->brakegain[2]=m->weight*m->tires[2].cfmax*gCarBraking[m->body_type];
 m->brakegain[3]=m->weight*m->tires[3].cfmax*gCarBraking[m->body_type];
 m->dwinvmi=m->tires[2].invmi/2;
 steer_constants(m);
 m->grw.z=0.0f;m->grw.x=0.0f;m->grw.y=-m->weight;
 m->mainin_gear=0;m->commandgear=0;m->wing=0;
 m->idt=0.0f;
 m->d.x=0.0f;m->d.y=0.0f;m->d.z=0.0f;
 m->g.x=0.0f;m->g.y=0.0f;m->g.z=0.0f;
 m->centerMoment.x=0.0f;m->centerMoment.y=0.0f;m->centerMoment.z=0.0f;
 m->tireForce[1].x=0.0f;m->tireForce[1].y=0.0f;m->tireForce[1].z=0.0f;
 m->tireForce[2].x=0.0f;m->tireForce[2].y=0.0f;m->tireForce[2].z=0.0f;
 m->tireForce[3].x=0.0f;m->tireForce[3].y=0.0f;m->tireForce[3].z=0.0f;
 m->tireForce[0].x=0.0f;m->tireForce[0].y=0.0f;m->tireForce[0].z=0.0f;
 m->brake=0.0f;m->engtorque=0.0f;
 m->clutchtorque=0.0f;m->clutchangvel=0.0f;m->transratio=0.0f;m->totalratio=0.0f;
 m->dwtorque=0.0f;m->efdwinvmi=0.0f;m->dwangvel=0.0f;
}
