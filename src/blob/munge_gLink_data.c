#include "game/game.h"


#define MAX_LINKS 6
#define LINK_ACTIVE 0x80
#define DRONE 1
#define HUMAN 2

typedef struct MODELDAT {
    /* 0x000 */ u8 pad0[0x7C6];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ s16 in_game;
    /* 0x7CA */ s16 we_control;
    /* 0x7CC */ s8 drone_type;
    /* 0x7CD */ u8 pad7CD[0x3B];
} MODELDAT; /* 0x808 */

typedef struct {
    /* 0x000 */ u8 pad0[0xEE];
    /* 0x0EE */ u8 unkEE;
    /* 0x0EF */ u8 padEF[0x110 - 0xEF]; struct pc_visual visuals[21]; u8 padEF_308[0x35B - 0x308];
    /* 0x35B */ s8 place;
    /* 0x35C */ u8 pad35C[0x380 - 0x35C]; void *input; u8 pad35C_384[0x3B8 - 0x384];
} CAR_DATA; /* 0x3B8 */

void munge_gLink_data(void) {
    s32 i;
    s32 index;
    s32 j;
    MODELDAT *m;
    CAR_DATA *gc;

    gNumActiveCars = 0;
    for (i = 0; i < gNumCars; i++) {
        m = PC_MODEL(i);
        gc = PC_CAR(i);
        m->we_control = (PC_LINK_OWNER(i) == 0);
        m->drone_type = 0;
        m->in_game = (PC_LINK_FLAGS(i) & LINK_ACTIVE) != 0;
        if (m->in_game) {
            PC_MODEL_SLOT(gNumActiveCars++) = i;
            gc->place = i;
            gc->unkEE = PC_LINK_PLACE(i);
            m->drone_type = (PC_LINK_OWNER(i) < MAX_LINKS) ? DRONE : HUMAN;
        }
    }
    for (i = gNumCars; i < PC_MODEL_SLOTS; i++) {
        m = PC_MODEL(i);
        m->we_control = (PC_LINK_OWNER(i) == 0);
        m->drone_type = 0;
        m->in_game = 0;
    }
    num_our_drones = 0;
    j = num_our_drones;
    num_drones = j;
    num_humans = j;
    for (i = 0; i < gNumActiveCars; i++) {
        index = PC_MODEL_SLOT(i);
        if (PC_MODEL_DRONE_TYPE(index) == HUMAN) {
            j = num_humans;
            humans[j] = index;
            num_humans = j + 1;
        } else {
            j = num_drones;
            PC_DRONE(j) = index;
            num_drones = j + 1;
            if (PC_MODEL_WE_CONTROL(index)) {
                PC_OUR_DRONE(num_our_drones++) = index;
            }
        }
    }
    gModelIteration = 0;
}
