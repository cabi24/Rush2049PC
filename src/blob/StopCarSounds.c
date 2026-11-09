/* StopCarSounds: silences a car (MAT3 audio.c). */
#include "game/game.h"

/* The model record's fields this file reads (MAT3 tModelData, the N64's layout: docs/port/CARS.md). */
typedef struct MODELDAT {
    u8 pad0[0x7C6];
    s16 slot;           /* 0x7C6 */
    u8 pad7C8[4];
    s8 drone_type;      /* 0x7CC: 2 for a human's car */
} MODELDAT;

/* A human's car (tModelData.drone_type): its sounds are plain effects, a drone's are 3D emitters. */
#define DRONE_TYPE_HUMAN 2

/* Stops every sound of the car of model m, if it has started them (StartCarSounds): the two engine layers, the
 * random engine sound and the tire screech, and for a human's car its road noises, scrape, horn and bog sounds.
 * Its engine is then updated on the frames of its slot's parity again (pc_engine_phase). */
void StopCarSounds(MODELDAT *m) {
    s32 slot = m->slot;
    s32 i;

    if (PC_CAR_SOUNDS_ON(slot)) {
        PC_CAR_SOUNDS_ON(slot) = 0;
        PC_ENGINE_PHASE(slot) = (slot & 1) == 0;
        for (i = 0; i < 2; i++) {
            if (i) {}
            if (PC_ENGINE_SOUND_HANDLE(slot, i) != -1) {
                StopSound(PC_ENGINE_SOUND_REC(slot, i), m->drone_type != DRONE_TYPE_HUMAN);
            }
        }
        /* Record 3: the random engine sound (MAT3's rev; MAT3 also stops a hum, record 2, which the N64 never
         * starts). */
        if (PC_ENGINE_SOUND_HANDLE(slot, 3) != -1) {
            StopSound(PC_ENGINE_SOUND_REC(slot, 3), m->drone_type != DRONE_TYPE_HUMAN);
        }
        StopSound(PC_SCREECH(slot), m->drone_type != DRONE_TYPE_HUMAN);
        if (m->drone_type == DRONE_TYPE_HUMAN) {
            StopSound(&gRoadNoises[slot].dirt, 0);
            StopSound(&gRoadNoises[slot].water, 0);
            StopSound(&gRoadNoises[slot].pave, 0);
            AudStopEffect(gScrapes[slot]);
            gScrapes[slot] = -1;
            gScrapeState[slot] = 0;
            gScrapeTime[slot] = 0.0f;
            AudStopEffect(gHornSounds[slot]);
            gHornSounds[slot] = -1;
            AudStopEffect(gBogSounds[slot]);
            gBogSounds[slot] = -1;
        }
    }
}
