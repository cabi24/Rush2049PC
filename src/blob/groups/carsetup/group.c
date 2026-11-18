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

/* The car's physics descriptor (MAT3 _tCar, gCarModels[body]). */
typedef struct tCar {
 u8 pad0[0x50];float steerratio,swtpg;u8 pad58[8];   /* +0x50 the steering */
 TireDesc *tires[4];            /* +0x60 each tire's constants (frontTire's records) */
 Vec3 tireR[4];                 /* +0x70 each tire's position from the centre of mass */
} tCar;

/* A PC datum that names the car tables only pointers reach: the six physics descriptors (gCarModels points at
 * them) and the two tire records (the descriptors' tires[] point at those). The data loader places and fills the data
 * some file names (docs/port/DATA.md 3), each by its declaration in include/game; nothing else names these two, and
 * nothing reads this. */
const void *const pc_car_setup_data[2] = {car_descriptors, frontTire};

float sqrtf(float);

/* Fill model m from its parts: the descriptor gCarModels[body] and the tune gTune[handling];
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
 m->tune=&gTune[m->handling];
 m->car=car;
 m->wingEffect=gWingSizes[gCarWings[row][body]];
 m->mass=CalcMass(m);
 durability=carDurability[m->frame]-carsetup_durability_base;
 body=m->body_type;
 m->collisionmass=gCollisionMass[body]+durability*1000.0f;
 m->crashthreshold=gCrashThreshold[body]+durability*carsetup_crash_threshold_scale;
 m->weight=m->mass*gGravity;
 m->magicdiff=gCarDifferential[body]&1;
 m->nothrusttorque=gCarDifferential[body]&2;
 m->torquecurve=stdtorquecurve;
 m->offroadtires=CalcOffroadTires(m);
 drift=CalcDrift(m);m->drift=drift;
 m->sidefk=CalcTireSideForce(m);
 m->wheelbase=car->tireR[0].z-car->tireR[2].z;
 size=&collsize[m->body_type];
 m->bodyR[0][0]=m->bodyR[2][0]=size->side;
 m->bodyR[1][0]=m->bodyR[3][0]=-size->side;
 m->bodyR[0][1]=m->bodyR[1][1]=size->height;
 m->bodyR[2][1]=m->bodyR[3][1]=size->height;
 m->bodyR[0][2]=m->bodyR[1][2]=size->front;
 m->bodyR[2][2]=m->bodyR[3][2]=-size->rear;
 max_length=(size->rear<size->front)?size->front:size->rear;
 m->colrad=sqrtf(max_length*max_length+size->side*size->side+size->height*size->height);
 for(i=0;i<4;i++) {
  copy_tire_info(car->tires[i],&m->tires[i]);
  if(i<2)tire_constants(m,&m->tires[i],-car->tireR[i+2].z);
  else tire_constants(m,&m->tires[i],car->tireR[i-2].z);
  m->tires[i].angvel=m->v[2]/m->tires[i].tradius;
  m->suscomp[i]=0.0f;
  m->tireV[i][0]=0.0f;m->tireV[i][1]=0.0f;m->tireV[i][2]=0.0f;
 }
 m->brakegain[0]=m->weight*m->tires[0].cfmax*gCarBraking[m->body_type];
 m->brakegain[1]=m->weight*m->tires[1].cfmax*gCarBraking[m->body_type];
 m->brakegain[2]=m->weight*m->tires[2].cfmax*gCarBraking[m->body_type];
 m->brakegain[3]=m->weight*m->tires[3].cfmax*gCarBraking[m->body_type];
 m->dwinvmi=m->tires[2].invmi/2;
 steer_constants(m);
 m->grw[2]=0.0f;m->grw[0]=0.0f;m->grw[1]=-m->weight;
 m->mainin.gear=0;m->commandgear=0;m->wing=0;
 m->idt=0.0f;
 m->d[0]=0.0f;m->d[1]=0.0f;m->d[2]=0.0f;
 m->g[0]=0.0f;m->g[1]=0.0f;m->g[2]=0.0f;
 m->centerMoment[0]=0.0f;m->centerMoment[1]=0.0f;m->centerMoment[2]=0.0f;
 m->tireForce[1][0]=0.0f;m->tireForce[1][1]=0.0f;m->tireForce[1][2]=0.0f;
 m->tireForce[2][0]=0.0f;m->tireForce[2][1]=0.0f;m->tireForce[2][2]=0.0f;
 m->tireForce[3][0]=0.0f;m->tireForce[3][1]=0.0f;m->tireForce[3][2]=0.0f;
 m->tireForce[0][0]=0.0f;m->tireForce[0][1]=0.0f;m->tireForce[0][2]=0.0f;
 m->brake=0.0f;m->engtorque=0.0f;
 m->clutchtorque=0.0f;m->clutchangvel=0.0f;m->transratio=0.0f;m->totalratio=0.0f;
 m->dwtorque=0.0f;m->efdwinvmi=0.0f;m->dwangvel=0.0f;
}
