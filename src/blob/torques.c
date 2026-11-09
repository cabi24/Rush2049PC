/* torques.c: the moments on a car (MAT3 drivsym.c, torques()).
 *
 * A stage of the physics step (regular, sym), after the forces: the moment of each tire's and body corner's force
 * about the centre of mass, the wing's air control, and the centre moment (antispin, collisions). accelerations
 * turns the sum into an angular acceleration. */
#include "game/game.h"

typedef f32 Vec3[3];
/* The car record (tCarData), its input record (tPadState: the stick, analog[0] across and [1] up and down): a host
 * pointer on the PC (docs/port/CARS.md 2.1), NULL for a drone. */
typedef struct tCarData { u8 pad[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad_308[0x380 - 0x308]; tPadState *player; u8 rest[52]; } tCarData;
/* The car's physics descriptor (MAT3 _tCar), its tire positions. */
typedef struct tCar {
    u8 pad[0x60]; TireDesc *tires[4];   /* +0x60 each tire's constants */
    Vec3 tireR[4];                      /* +0x70 each tire's position from the centre of mass */
} tCar;
/* The model record (tModelData), the fields torques reads and writes. */
typedef struct tModelData {
    tCar *car;                  /* +0x000 the physics descriptor */
    Tune44 *tune;
    u8 pad8[0x1C - 0x08];
    Vec3 m;                     /* +0x01C the total moment, the result */
    u8 pad28[0x64 - 0x28];
    Vec3 tireForce[4];          /* +0x064 each tire's force */
    u8 pad94[0xC4 - 0x94];
    Vec3 bodyForce[4];          /* +0x0C4 each body corner's force */
    u8 padF4[0x13C - 0xF4];
    Vec3 centerMoment;          /* +0x13C */
    u8 pad148[0x3A0 - 0x148];
    streetyp *lasttp[4];
    u8 pad3B0[0x3F0 - 0x3B0];
    f32 magvel;                 /* +0x3F0 the speed, feet a second */
    u8 pad3F4[0x3F8 - 0x3F4];
    s16 wing;                   /* +0x3F8 the wing deployed */
    u8 pad3FA[0x40C - 0x3FA];
    const s16 *torquecurve;
    u8 pad410[0x5B8 - 0x410];
    f32 wingEffect;             /* +0x5B8 the wing's size (gWingSizes) */
    u8 pad5BC[0x5CC - 0x5BC];
    f32 suscomp[4];             /* +0x5CC each corner's suspension compression */
    u8 pad5DC[0x5EC - 0x5DC];
    f32 airdist[4];             /* +0x5EC each tire's height above the road */
    u8 pad5FC[0x648 - 0x5FC];
    surftyp *movingSurface;
    u8 pad64C[0x731 - 0x64C];
    s8 mainin_wing;             /* +0x731 mainin.wing: the player holds the wing button */
    u8 pad732[0x7C6 - 0x732];
    s16 slot;                   /* +0x7C6 */
    u8 pad7C8[0x7D4 - 0x7C8];
    s32 appearance;             /* +0x7D4 APPEAR_* */
} tModelData;

#define veccopy(a,r) {r[0]=a[0]; r[1]=a[1]; r[2]=a[2];}
#define crossprod(a,b,r) {r[0]=a[1]*b[2]-a[2]*b[1]; r[1]=a[2]*b[0]-a[0]*b[2]; r[2]=a[0]*b[1]-a[1]*b[0];}
#define vecadd(a,b,r) {r[0]=a[0]+b[0]; r[1]=a[1]+b[1]; r[2]=a[2]+b[2];}

/* Model m's total moment, in the car's frame: for each tire its force crossed with its arm (its position from the
 * centre of mass, lowered by the suspension's compression), for each body corner its force crossed with the tire's
 * arm at the centre's height. High in the air (all four tires above 5 ft, not a hulk, not a battle) the wing button
 * puts the wing out, and with it a human's stick pitches the car (its up and down) and rolls it (across, the other
 * way on a mirrored track), in proportion to the wing's size and the speed. Then the centre moment. */
void torques(tModelData *m) {
    f32 moment[3],arm[3],*rp,*fp;
    int i;
    f32 wing_effect,speed;
    tCarData *car;
    tPadState *player;
    int sign;
    m->m[0]=0; m->m[1]=0; m->m[2]=0;
    for(i=0,rp=m->car->tireR[0],fp=m->tireForce[0];i<4;++i,rp+=3,fp+=3) {
        veccopy(rp,arm);
        arm[1] += m->suscomp[i];
        crossprod(fp,arm,moment);
        vecadd(m->m,moment,m->m);
    }
    for(i=0,rp=m->car->tireR[0],fp=m->bodyForce[0];i<4;++i,rp+=3,fp+=3) {
        veccopy(rp,arm);
        arm[1]=0;
        crossprod(fp,arm,moment);
        vecadd(m->m,moment,m->m);
    }
    if (!(m->appearance & APPEAR_HULK) && m->airdist[0]>5.0f && m->airdist[1]>5.0f && m->airdist[2]>5.0f && m->airdist[3]>5.0f && gRace != kBattle /* kBattle */) {
        if(m->mainin_wing) m->wing=1; else m->wing=0;
        if(m->wing) {
            car=PC_CAR(m->slot);
            player=car->player;
            if(player) {
                wing_effect=m->wingEffect;
                speed=m->magvel;
                m->m[0] -= player->analog[1] * wing_effect * speed;
                if(gMirrorWaves) sign=-1; else sign=1;
                m->m[2] += car->player->analog[0] * wing_effect * speed * (f32)sign;
            }
        }
    } else m->wing=0;
    m->m[0] += m->centerMoment[0];
    m->m[1] += m->centerMoment[1];
    m->m[2] += m->centerMoment[2];
}
