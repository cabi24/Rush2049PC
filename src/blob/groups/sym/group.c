/* sym/group.c: one step of a car's simulation (MAT3 drivsym.c, sym() and _ModelControls()).
 *
 * The model thread (UpdateModel) runs sym for each car it drives, at the model's rate (dt, MAT3 0.0025 s): the
 * controls into the model (ModelControls), the drivetrain, the ordinary step (regular: road, forces, moments,
 * integration), the crash test (checkok), the rpm, the engine bogging down, and the peak forces the HUD and sounds
 * read. The N64's ModelControls has the arcade's brake ramps as constants (0.7 at the end of the race, 0.33
 * coasting). */
#include "game/game.h"

/* A tire in the model record (MAT3 anon_tradius_springK_5c), its spin. */
typedef struct tiredes {
    /* 0x00 */ u8 pad00[0x48];
    /* 0x48 */ f32 angvel;          /* the wheel's spin, radians a second */
    /* 0x4C */ u8 pad4C[0x10];
} tiredes;

/* The model record (tModelData), the fields sym and ModelControls read and write. */
typedef struct tModelData {
    /* 0x000 */ _tCar *car;
    /* 0x004 */ Tune44 *tune;
    /* 0x008 */ u8 pad008[2];
    /* 0x00A */ s8 autotrans;
    /* 0x00B */ u8 pad00B[0xC4 - 0xB];
    /* 0x0C4 */ f32 bodyForce[4][3];
    /* 0x0F4 */ u8 pad0F4[0x124 - 0xF4];
    /* 0x124 */ f32 centerForce[3];
    /* 0x130 */ f32 centerForceLast[3];
    /* 0x13C */ f32 centerMoment[3];
    /* 0x148 */ f32 peak_body_force[2][3];     /* the largest and smallest body force on each axis */
    /* 0x160 */ f32 peak_center_force[2][3];   /* the same of the centre force */
    /* 0x178 */ u8 pad178[0x3A0 - 0x178];
    /* 0x3A0 */ streetyp *lasttp[4];
    /* 0x3B0 */ f32 steerangle;
    /* 0x3B4 */ f32 torque[4];
    /* 0x3C4 */ f32 steergain;
    /* 0x3C8 */ f32 trail;
    /* 0x3CC */ f32 clutch;
    /* 0x3D0 */ f32 throttle;
    /* 0x3D4 */ f32 brake;
    /* 0x3D8 */ f32 brakeroll;
    /* 0x3DC */ f32 brakegain[4];
    /* 0x3EC */ u8 pad3EC[8];
    /* 0x3F4 */ s16 gear;
    /* 0x3F6 */ s16 commandgear;
    /* 0x3F8 */ u8 pad3F8[0x408 - 0x3F8];
    /* 0x408 */ f32 engangvel;
    /* 0x40C */ const s16 *torquecurve;
    /* 0x410 */ u8 pad410[0x430 - 0x410];
    /* 0x430 */ tiredes tires[4];
    /* 0x5A0 */ u8 pad5A0[0x634 - 0x5A0];
    /* 0x634 */ f32 dt;
    /* 0x638 */ f32 idt;
    /* 0x63C */ u8 pad63C[4];
    /* 0x640 */ s8 crashflag;
    /* 0x641 */ u8 pad641[0x648 - 0x641];
    /* 0x648 */ surftyp *movingSurface;
    /* 0x64C */ u8 pad64C[0x65C - 0x64C];
    /* 0x65C */ s16 bogState;               /* 1 the engine bogs down, 3 it recovers */
    /* 0x65E */ u8 pad65E[0x710 - 0x65E];
    /* 0x710 */ s32 modelIteration;         /* fastin: the steps run, negative during the countdown */
    /* 0x714 */ u8 pad714[4];
    /* 0x718 */ f32 fastin_dt;              /* fastin.dt: the step the model thread asks for */
    /* 0x71C */ u8 pad71C[4];
    /* 0x720 */ f32 mainin_wheel;           /* mainin: the controls as the game thread left them */
    /* 0x724 */ f32 mainin_clutch;
    /* 0x728 */ f32 mainin_brake;
    /* 0x72C */ f32 mainin_throttle;
    /* 0x730 */ s8 mainin_gear;
    /* 0x731 */ u8 pad731[0x7C6 - 0x731];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ u8 pad7C8[4];
    /* 0x7CC */ s8 drone_type;              /* DRONE_TYPE_* */
    /* 0x7CD */ u8 pad7CD[3];
    /* 0x7D0 */ s16 rpm;
} tModelData;

/* The car record (tCarData), the fields ModelControls reads. */
typedef struct tCarData {
    /* 0x000 */ u8 pad000[0xEF];
    /* 0x0EF */ s8 place_locked;            /* the car has finished */
    /* 0x0F0 */ u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad308[0x359 - 0x308];
    /* 0x359 */ s8 dead;
    /* 0x35A */ u8 pad35A[0x380 - 0x35A]; void *player; u8 pad384[0x3B8 - 0x384];
} tCarData;

/* Model m's controls for this step (MAT3 _ModelControls): the step and its inverse, the steering angle (the wheel
 * times the steering gain), the clutch and the throttle from mainin. A car that has finished gets no throttle; at the
 * end of the race (or finished) it brakes at 0.7 with the clutch pressed, coasting (gCoastFlag) at 0.33; else the
 * player's brake, full above 0.99. The gear is the player's for a manual box, the box's own otherwise (autoshift),
 * though commandgear always follows the player. During the countdown: neutral, clutch pressed, full brake. Each
 * wheel's brake torque is its brakegain times the brake, against its spin, fading in from 2 to 10 rad/s (none
 * slower, so a stopped car doesn't rock). */
void ModelControls(tModelData *m) {
    f32 rolloff;
    s32 i;
    tiredes *td;
    tCarData *gc = PC_CAR(m->slot);

    m->dt = m->fastin_dt;
    m->idt = 1 / m->dt;
    m->steerangle = m->steergain * m->mainin_wheel;
    m->clutch = m->mainin_clutch;
    if (gc->place_locked == 1) {
        m->throttle = 0.0f;
    } else {
        m->throttle = m->mainin_throttle;
    }
    if (gEndGame || gc->place_locked) {
        m->brake = 0.7f;
        m->throttle = 0.0f;
        m->clutch = 1.0f;
    } else if (gCoastFlag) {
        m->brake = 0.33f;
        m->throttle = 0.0f;
        m->clutch = 1.0f;
    } else {
        m->brake = m->mainin_brake;
    }
    if (m->brake > 0.99f) {
        m->brake = 1.0f;
    }
    if (!m->autotrans) {
        m->commandgear = m->gear = m->mainin_gear;
    } else {
        m->commandgear = m->mainin_gear;
    }
    if (m->modelIteration < 0) {
        m->commandgear = 0;
        m->gear = 0;
        m->clutch = 1.0f;
        m->brake = 1.0f;
    }
    for (i = 0, td = &m->tires[0]; i < 4; ++i, ++td) {
        if (td->angvel > 0.0f) {
            if (td->angvel < 10.0f) {
                if (td->angvel < 2.0f) {
                    rolloff = 0.0f;
                } else {
                    rolloff = td->angvel * 0.05f;
                }
            } else {
                rolloff = 1.0f;
            }
        } else {
            if (td->angvel > -10.0f) {
                if (td->angvel > -2.0f) {
                    rolloff = 0.0f;
                } else {
                    rolloff = td->angvel * 0.05f;
                }
            } else {
                rolloff = -1.0f;
            }
        }
        m->torque[i] = -m->brakegain[i] * m->brake * rolloff;
    }
}

/* One step of model m: its controls (none for a crashed car or one out of the race), the drivetrain, the ordinary
 * step, the crash test; the rpm (engangvel in rpm, 9.549305 = 60 / 2 pi, times 0.9: MAT3 mcommunication); a manual
 * car below 600 rpm on more than half throttle bogs down (bogState 1, for the sound) and recovers once it doesn't
 * (3); for a human the peaks of the centre and body forces on each axis; then this step's centre force is kept
 * (centerForceLast) and the centre force and moment are cleared for the next. */
void sym(tModelData *m) {
    s32 absvel, i, j;   /* absvel: unused */

    ModelControls(m);
    if (m->crashflag || PC_CAR_DEAD(m->slot)) {
        m->steerangle = 0.0f;
        m->clutch = 0.0f;
        m->brake = 0.0f;
        m->throttle = 0.0f;
    }
    drivetrain(m);
    regular(m);
    checkok(m);
    m->rpm = (s16) TRUNC_W (m->engangvel * 9.549305f * 0.9f);
    if ((m->autotrans == 0) && (m->throttle > 0.5f) && (((m->rpm >= 0) ? m->rpm : -m->rpm) < 600)) {
        if (m->bogState == 0) {
            m->bogState = 1;
        }
    } else {
        if (m->bogState) {
            m->bogState = 3;
        }
    }
    if (m->drone_type == DRONE_TYPE_HUMAN) {
        for (i = 0; i < 3; i++) {
            if (m->centerForce[i] > m->peak_center_force[0][i]) {
                m->peak_center_force[0][i] = m->centerForce[i];
            }
            if (m->centerForce[i] < m->peak_center_force[1][i]) {
                m->peak_center_force[1][i] = m->centerForce[i];
            }
            for (j = 0; j < 4; j++) {
                if (m->bodyForce[j][i] > m->peak_body_force[0][i]) {
                    m->peak_body_force[0][i] = m->bodyForce[j][i];
                }
                if (m->bodyForce[j][i] < m->peak_body_force[1][i]) {
                    m->peak_body_force[1][i] = m->bodyForce[j][i];
                }
            }
        }
    }
    m->centerForceLast[0] = m->centerForce[0];
    m->centerForceLast[1] = m->centerForce[1];
    m->centerForceLast[2] = m->centerForce[2];
    m->centerForce[0] = m->centerForce[1] = m->centerForce[2] = 0.0f;
    m->centerMoment[0] = 0.0f;
    m->centerMoment[1] = 0.0f;
    m->centerMoment[2] = 0.0f;
}
