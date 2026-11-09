/* dead_reckon_all.c: take each car's dead-reckoning snapshot (MAT3 rekon.c, dead_reckon_all()).
 *
 * The model thread calls it after a step: the game thread then draws the cars from these snapshots, extrapolated
 * to the frame's time (game_reckon_all), rather than from the simulation's own state. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4): the fields used here. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad008[0x398];
    streetyp *lasttp[4];
    u8 pad3B0[0x5C];
    const s16 *torquecurve;
    u8 pad410[0x238];
    surftyp *movingSurface;
    u8 pad64C[0x17A];
    u16 slot;                   /* 0x7C6 */
    s16 in_game;                /* 0x7C8 in the race */
    u8 pad7CA[0x7E8 - 0x7CA];
    u8 laps;                    /* 0x7E8 */
} tModelData;

/* For each car in the race that isn't dead (dead below CAR_DEAD), copy its position, velocity, acceleration,
 * orientation, suspension and speed into its reckon snapshot (dead_reckon_posuv). */
void dead_reckon_all(void) {
    tModelData *m;
    s32 i;
    for (i = 0; i != PC_RACE_CARS; i++) {
        m = PC_MODEL(i);
        if ((m->in_game != 0) && ((s8) PC_CAR_DEAD(i) < CAR_DEAD)) {
            dead_reckon_posuv(m);
        }
    }
}
