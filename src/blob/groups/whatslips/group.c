/* whatslips/group.c: the clutch, and the engine's spin against it (MAT3 drivetra.c, whatslips()).
 *
 * The drivetrain (drivetrain.c) calls it each step after the gearbox: whether the clutch slips or holds, the torque
 * it passes to the wheels, and the engine's speed. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), the engine's and clutch's constants. */
typedef struct tCar {u8 pad0[0xA0]; f32 enginvmi; /* +0xA0 the engine's inverse inertia */ u8 padA4[4]; f32 clutchmaxt; /* +0xA8 the clutch's most torque */} tCar;
/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its clutch scale. */
typedef struct tTune {u8 pad0[0x10]; f32 clutchmaxtk; /* +0x10 */} tTune;
enum n64p_tCar { n64p_tCar_ };
enum n64p_tTune { n64p_tTune_ };
/* The model record (tModelData), the fields whatslips reads and writes. */
typedef struct tModelData {
    enum n64p_tCar car; enum n64p_tTune tune;       /* +0x000, +0x004 window addresses */
    u8 pad8[0x3CC - 0x8]; f32 clutch;              /* +0x3CC 1 pressed, 0 released (SetClutch) */
    u8 pad3D0[0x3F4 - 0x3D0]; s16 gear; u8 pad3F6[0x404 - 0x3F6];   /* +0x3F4 */
    f32 engtorque,engangvel; u8 pad40C[4];          /* +0x404 the engine's torque and speed (radians a second) */
    f32 dwtorque,dwinvmi,efdwinvmi,dwangvel,clutchtorque,clutchangvel;   /* +0x410 the driven wheels' torque, their
                                                     * inverse inertia, the driveline's effective one, their speed;
                                                     * the clutch's torque and speed */
    u8 pad428[4]; f32 totalratio;                   /* +0x42C the gear and differential ratio */
    u8 pad430[0x634 - 0x430]; f32 dt; u8 tail[0x808 - 0x638];   /* +0x634 the step, seconds */
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tCar: (tCar *)PC_N64P(x), enum n64p_tTune: (tTune *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
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
    else curclmaxt=((f32).8-m->clutch)*(f32)1.25*N64P(m->car)->clutchmaxt;
    if (m->gear==0 || curclmaxt==0) {
        m->clutchtorque=0;
        m->dwtorque=0;
        m->clutchangvel=m->engangvel;
        m->engangvel+=m->engtorque*N64P(m->car)->enginvmi*m->dt;
        m->efdwinvmi=m->dwinvmi;
        return;
    }
    curclmaxt*=N64P(m->tune)->clutchmaxtk;
    m->clutchangvel=m->dwangvel*m->totalratio;
    was_slipping=0;
    if (m->engangvel>m->clutchangvel) was_slipping=1;
    if (was_slipping) m->clutchtorque=curclmaxt;
    else m->clutchtorque=-curclmaxt;
    dv=(m->engtorque-m->clutchtorque)*N64P(m->car)->enginvmi*m->dt;
    if (((m->engangvel+dv)>m->clutchangvel)^was_slipping) {
        m->engangvel=m->clutchangvel;
        m->clutchtorque=m->engtorque;
    } else m->engangvel+=dv;
    m->efdwinvmi=(f32)1.0/((f32)1.0/m->dwinvmi+totratsq/N64P(m->car)->enginvmi);
    m->dwtorque=m->clutchtorque*m->totalratio;
}
