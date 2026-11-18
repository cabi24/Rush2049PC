/* sym/group.c: one step of a car's simulation (MAT3 drivsym.c, sym() and _ModelControls()).
 *
 * The model thread (UpdateModel) runs sym for each car it drives, at the model's rate (dt, MAT3 0.0025 s): the
 * controls into the model (ModelControls), the drivetrain, the ordinary step (regular: road, forces, moments,
 * integration), the crash test (checkok), the rpm, the engine bogging down, and the peak forces the HUD and sounds
 * read. The N64's ModelControls has the arcade's brake ramps as constants (0.7 at the end of the race, 0.33
 * coasting). */
#include "game/game.h"

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
    Tire *td;
    tCarData *gc = PC_CAR(m->slot);

    m->dt = m->fastin.dt;
    m->idt = 1 / m->dt;
    m->steerangle = m->steergain * m->mainin.wheel;
    m->clutch = m->mainin.clutch;
    if (gc->place_locked == 1) {
        m->throttle = 0.0f;
    } else {
        m->throttle = m->mainin.throttle;
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
        m->brake = m->mainin.brake;
    }
    if (m->brake > 0.99f) {
        m->brake = 1.0f;
    }
    if (!m->autotrans) {
        m->commandgear = m->gear = m->mainin.gear;
    } else {
        m->commandgear = m->mainin.gear;
    }
    if (m->fastin.modelIteration < 0) {
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
