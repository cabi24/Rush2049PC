/* group.c (StartCar): start one of a car's wing visuals (MAT3 visuals.c, StartWing()). */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields StartWing reads. */
typedef struct tModelData {
    u8 pad000[8];
    u8 body_type;                /* +0x008 the car, 0-12 */
    u8 pad009[0x7CA - 9];
    s16 we_control;              /* +0x7CA the computer drives it (a drone) */
    u8 pad7CC[0x808 - 0x7CC];
} tModelData;

/* gCarParts[slot]: the car's scene objects (MAT3 CarParts, the N64's 0x40 bytes: CreateCar.c). */
typedef struct CarParts {
    s32 frame;
    u8 pad04[0x40 - 4];
} CarParts;

VIEW(s8, gCarWings);
#define gCarWings gCarWings_view

VIEW(s32, wingOffsets);
#define wingOffsets wingOffsets_view

VIEW(s32, gCarParts);
#define gCarParts gCarParts_view

/* Start car `slot`'s wing on side `side` (0 left, 1 right; VIS_WINGS + side, AnimateWing): an object of the wing
 * the car's owner chose (gCarWings, a row a player, row 0 the drones'; OBJLIST_WINGS, 4 a wing type), under the
 * frame at the body's wing height (wingOffsets), hidden until the wing opens, and the smoke it trails
 * (InitWingSmoke). No wing in a ghost race or a battle (gRace 2 kGhost, 6 kBattle), nor for a drone. */
void StartWing(s16 slot, s16 side) {
    f32 (*wing_offsets)[3] = (f32 (*)[3]) &wingOffsets;   /* f32[13][3]: a row per body type */
    s8 (*wings)[13] = (s8 (*)[13]) &gCarWings;            /* s8[][13]: the wing by row (player + 1) and body */
    tModelData *m;
    f32 offset[3];
    s32 index;
    s32 body_type;

    if (gRace == 2 || gRace == 6 || (m = PC_MODEL(slot), m->we_control != 0)) {
        PC_CAR_VISUALS(slot)[side + VIS_WINGS].func = 0;
        return;
    }
    PC_CAR_VISUALS(slot)[side + VIS_WINGS].func = AnimateWing;
    PC_CAR_VISUALS(slot)[side + VIS_WINGS].slot = slot;
    PC_CAR_VISUALS(slot)[side + VIS_WINGS].data = side;
    body_type = m->body_type;
    offset[0] = 0.0f;
    offset[2] = 0.0f;
    offset[1] = wing_offsets[body_type][1];
    index = wings[slot + 1][body_type] * 4 + side + OBJLIST_WINGS;
    PC_CAR_VISUALS(slot)[side + VIS_WINGS].objnum = InitCarObj(gObjList[index], 15, ((CarParts *) &gCarParts)[slot].frame, 0, slot, -1, 1);
    ZOID_UpdateObject(PC_CAR_VISUALS(slot)[side + VIS_WINGS].objnum, offset, NULL);
    MBOX_HideObject(PC_CAR_VISUALS(slot)[side + VIS_WINGS].objnum, 1, 15);
    InitWingSmoke(slot, side);
}
