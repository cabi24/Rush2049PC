/* torques.c: the moments on a car (MAT3 drivsym.c, torques()).
 *
 * A stage of the physics step (regular, sym), after the forces: the moment of each tire's and body corner's force
 * about the centre of mass, the wing's air control, and the centre moment (antispin, collisions). accelerations
 * turns the sum into an angular acceleration. */
#include "game/game.h"

typedef f32 Vec3[3];

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
        if(m->mainin.wing) m->wing=1; else m->wing=0;
        if(m->wing) {
            car=&pc_cars[m->slot];
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
