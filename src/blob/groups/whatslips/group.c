/* whatslips/group.c: the clutch, and the engine's spin against it (MAT3 drivetra.c, whatslips()).
 *
 * The drivetrain (drivetrain.c) calls it each step after the gearbox: whether the clutch slips or holds, the torque
 * it passes to the wheels, and the engine's speed. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), the engine's and clutch's constants. */
typedef struct tCar {
    u8 pad0[0x60]; TireDesc *tires[4];             /* +0x60 each tire's constants */
    u8 pad70[0xA0 - 0x70]; f32 enginvmi;           /* +0xA0 the engine's inverse inertia */
    u8 padA4[4]; f32 clutchmaxt;                   /* +0xA8 the clutch's most torque */
} tCar;
/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its clutch scale. */
typedef struct tTune {u8 pad0[0x10]; f32 clutchmaxtk; /* +0x10 */} tTune;
#define rpmtordps (2.0*3.14159/60.0)   /* rpm to radians a second */

/* The clutch of model m this step. The engine idles at no less than 1000 rpm. The clutch takes up to 1.25 times the
 * descriptor's clutchmaxt as the pedal comes up from 0.8 (none above), times the tune's clutchmaxtk. In neutral or
 * with the clutch out of play the engine spins up freely under its own torque and nothing reaches the wheels.
 * Otherwise the clutch passes its full torque the way the engine is faster (it slips), and if the engine's speed
 * would cross the clutch's this step the two lock: the engine turns with the wheels (times the gears' ratio) and
 * its whole torque goes through. The driveline's effective inverse inertia combines the wheels' and the engine's
 * through the ratio squared; the wheels get the clutch's torque times the ratio. */
void whatslips(tModelData *m)
{
    f32 totratsq,curclmaxt,dv;
    int was_slipping;
    totratsq=m->totalratio*m->totalratio;
    if (m->engangvel<(f32)(1000*rpmtordps)) m->engangvel=(f32)(1000*rpmtordps);
    if ((f32).8<m->clutch) curclmaxt=0;
    else curclmaxt=((f32).8-m->clutch)*(f32)1.25*m->car->clutchmaxt;
    if (m->gear==0 || curclmaxt==0) {
        m->clutchtorque=0;
        m->dwtorque=0;
        m->clutchangvel=m->engangvel;
        m->engangvel+=m->engtorque*m->car->enginvmi*m->dt;
        m->efdwinvmi=m->dwinvmi;
        return;
    }
    curclmaxt*=m->tune->clutchmaxtk;
    m->clutchangvel=m->dwangvel*m->totalratio;
    was_slipping=0;
    if (m->engangvel>m->clutchangvel) was_slipping=1;
    if (was_slipping) m->clutchtorque=curclmaxt;
    else m->clutchtorque=-curclmaxt;
    dv=(m->engtorque-m->clutchtorque)*m->car->enginvmi*m->dt;
    if (((m->engangvel+dv)>m->clutchangvel)^was_slipping) {
        m->engangvel=m->clutchangvel;
        m->clutchtorque=m->engtorque;
    } else m->engangvel+=dv;
    m->efdwinvmi=(f32)1.0/((f32)1.0/m->dwinvmi+totratsq/m->car->enginvmi);
    m->dwtorque=m->clutchtorque*m->totalratio;
}
