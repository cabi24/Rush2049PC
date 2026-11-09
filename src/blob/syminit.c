/* syminit.c: a car's simulation set up from scratch (MAT3 drivsym.c, syminit()).
 *
 * Init_MDrive calls it when a car starts a race or respawns. */
#include "game/game.h"

/* The model record (tModelData), the fields syminit sets itself. */
typedef struct tModelData {
    char pad0[0x408];
    f32 engangvel;              /* 0x408 the engine's speed, radians a second */
    char pad40C[0x720 - 0x40C];
    volatile f32 wheel;         /* 0x720 mainin: the controls the model thread reads */
    volatile f32 clutch;        /* 0x724 */
    volatile f32 brake;         /* 0x728 */
    volatile f32 throttle;      /* 0x72C */
    volatile s8 gear;           /* 0x730 */
    char pad731[0x7D0 - 0x731];
    s16 rpm;                    /* 0x7D0 the engine's rpm, for the sounds and the HUD */
} tModelData;

/* PC: the engine speed to s16, which a car's engine spinning past +-32767 rpm overruns (seen in a 12-car race,
 * docs/port/MORECARS.md 4); undefined in C. Through s32, whose low half the conversion keeps, as the N64's trunc.w.s
 * and sh do (docs/port/SANITIZE.md, particle_knot's V16). */
#define TRUNC_W (int)

/* Set model m up (initialize: the state, the car's constants, the road, the collision box), reset its controls
 * (MAT3 _InitModelControls: no steering, clutch or brake, throttle 1, first gear), find the road under it and place
 * its tires (road, positions), and give the sounds its rpm: engangvel in rpm (9.549305 = 60 / 2 pi) times 0.9
 * (MAT3 mcommunication). */
void syminit(tModelData *m)
{
    initialize(m);
    m->wheel = 0.0f;
    m->clutch = 0.0f;
    m->brake = 0.0f;
    m->throttle = 1.0f;
    m->gear = 1;
    road(m);
    positions(m);
    m->rpm = (s16) TRUNC_W (m->engangvel * 9.549305f * .9f);
}
