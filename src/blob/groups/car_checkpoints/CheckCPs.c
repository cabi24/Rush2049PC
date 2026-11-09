/* CheckCPs: the cars against the checkpoints, every frame (MAT3 checkpoint.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): the fields this reads and sets. */
typedef struct {
    u8 pad0[4];
    f32 modelTime;          /* +0x004 the model time of its position */
    f32 dr_pos[3];          /* +0x008 its position (dead reckoned) */
    u8 pad14[0x108 - 0x14];
    f32 mileage;            /* +0x108 the distance it has driven */
    u8 pad10C[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad10C_308[0x358 - 0x308];
    s8 crash_state;         /* +0x358 crashed (MAT3 +0x370) */
    u8 pad359[0x380 - 0x359]; void *input; u8 pad359_384[0x3B8 - 0x384];
} CarData;                  /* 0x3B8 */

/* The model record (MAT3 tModelData): the fields this reads and sets. */
typedef struct Model {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x398];
    streetyp *lasttp[4];
    u8 pad3B0[0x5C];
    const s16 *torquecurve;
    u8 pad410[0x238];
    surftyp *movingSurface;
    u8 pad64C[0x78];
    s16 moving_state;       /* +0x6C4 resurrect.moving_state: -1 unless being put back on the road */
    u8 pad6C6[0x7E4 - 0x6C6];
    s16 next_cp_index;      /* +0x7E4 the next checkpoint to pass */
    u8 pad7E6[3];
    s8 cpSign;              /* +0x7E9 the side of its plane the car was on last frame */
    u8 pad7EA[0x808 - 0x7EA];
} Model;                    /* 0x808 */

/* gPlayers[0].slot: the first player's car slot, the first byte of its input record. */
extern u8 gPlayers;

/* Adds the distance each car drove since the last frame to its mileage, and passes a car through its next
 * checkpoint (PassedCP) when it crossed the checkpoint's plane within its radius: a car driven here or by the
 * network's dead reckoning (link owner 0 or 6), not crashed nor being put back. The passing time is the frame's
 * time less the fraction of the frame since the crossing, from the distances to the plane before and after. Then,
 * but in practice: the cars' order (FindMaxpathIntervals, PlaceUnlockedCars), and with one human, the call for
 * taking the lead each time they take it (not again within 3 seconds), but in the attract mode's demo races. */
void CheckCPs(void)
{
    f32 frac;
    s16 i;
    f32 diff[3];
    s16 index;
    s32 side;
    f32 plane;
    CarData *gc;
    f32 *prev;
    Model *m;
    f32 move[3];
    f32 zvec[3];

    for (index = 0; index < gNumCars; index++) {
        gc = PC_CAR(index);
        prev = PC_CP_LAST_POS(index);
        move[0] = gc->dr_pos[0] - prev[0];
        move[1] = gc->dr_pos[1] - prev[1];
        move[2] = gc->dr_pos[2] - prev[2];
        gc->mileage += vector_magnitude(move);
        prev[0] = gc->dr_pos[0];
        prev[1] = gc->dr_pos[1];
        prev[2] = gc->dr_pos[2];
        if ((PC_LINK_OWNER(index) == 0 || PC_LINK_OWNER(index) == 6) && gc->crash_state == 0) {
            m = PC_MODEL(index);
            if (m->moving_state == -1) {
                diff[0] = gc->dr_pos[0] - gCheckpoints.points[m->next_cp_index].pos[0];
                diff[1] = gc->dr_pos[1] - gCheckpoints.points[m->next_cp_index].pos[1];
                diff[2] = gc->dr_pos[2] - gCheckpoints.points[m->next_cp_index].pos[2];
                plane = diff[0] * gCheckpoints.points[m->next_cp_index].normal[0] +
                        diff[1] * gCheckpoints.points[m->next_cp_index].normal[1] +
                        diff[2] * gCheckpoints.points[m->next_cp_index].normal[2];
                if (plane < 0.0f) {
                    side = -1;
                } else {
                    side = 1;
                }
                if (side != m->cpSign) {
                    m->cpSign = side;
                    if (diff[0] * diff[0] + diff[2] * diff[2] < gCheckpoints.points[m->next_cp_index].radius) {
                        frac = plane / (plane - PC_CP_PLANE(index));
                        frac = gFrameSeconds * frac;
                        PassedCP(m, gc->modelTime - frac);
                    }
                }
                PC_CP_PLANE(index) = plane;
            }
        }
    }
    if (gRace == kPractice) {
        return;
    }
    FindMaxpathIntervals();
    PlaceUnlockedCars();
    if (gFirstPlaceTime != 0.0f && gSeconds - gFirstPlaceTime > 3.0f) {
        gFirstPlaceTime = 0.0f;
    }
    if (num_humans == 1 && PC_CAR_PLACE(gPlayers) == 0 && !(gGameState & GS_DEMO)) {
        if (gInFirstPlace == 0 && gFirstPlaceTime == 0.0f) {
            gFirstPlaceTime = gSeconds;
            PlayGameSound(16, gPlayers, 1, 1); /* MAT3 0x8E: the lead */
        }
        gInFirstPlace = 1;
    } else {
        gInFirstPlace = 0;
    }
}
