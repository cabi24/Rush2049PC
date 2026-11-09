/* CarSetup: the cars' visuals once their models are ready (MAT3 game.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the fields this reads. */
struct tModelData {
    u8 pad0[8];
    u8 body_type;       /* 0x008 the car's body (the roster's car) */
    u8 pad9[0x7DD - 9];
    s8 data_valid;      /* 0x7DD the model thread has filled it in */
};

/* carsetup_flag is the PC's pc_carsetup_flags, a flag per car slot (pc_game_types.h, docs/port/DECOUPLE.md 8). */
#define carsetup_flag PC_CARSETUP_FLAG(0)

/* For each car whose model is ready: its visuals set up the first time (SetupCar, carsetup_flag) and updated. */
void CarSetup(void) {
    s16 slot;
    s16 i;
    s8 *done;
    struct tModelData *m;

    i = 0;
    if ((s8) gNumActiveCars > 0) {
        do {
            slot = PC_MODEL_SLOT(i);
            done = (s32 *) ((u8 *) &carsetup_flag + slot);
            m = PC_MODEL(slot);
            if (m->data_valid != 0) {
                if (*done == 0) {
                    SetupCar(slot, m->body_type, 1);
                }
                *done = 1;
                UpdateVisuals(slot);
            }
            i += 1;
        } while (i < (s8) gNumActiveCars);
    }
}
