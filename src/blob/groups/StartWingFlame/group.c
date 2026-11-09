/* group.c (StartWingFlame): start one of a car's wing-flame visuals (MAT3 visuals.c, StartWingFlame()). */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields StartWingFlame reads. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 body_type;                /* +0x008 the car, 0-12 */
    u8 pad009[0x3A0 - 0x9];
    streetyp *lasttp[4];
    u8 pad3B0[0x40C - 0x3B0];
    const s16 *torquecurve;
    u8 pad410[0x648 - 0x410];
    surftyp *movingSurface;
    u8 pad64C[0x7CA - 0x64C];
    s16 we_control;              /* +0x7CA the computer drives it (a drone) */
    u8 pad7CC[0x808 - 0x7CC];
} tModelData;

/* A scene node's matrix: its rotation and its position relative to its parent. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* Start the flame of car `slot`'s wing on side `side` (0 left, 1 right; VIS_WING_FLAMES + side, AnimateWingFlame):
 * the wing type's flame object (OBJLIST_WINGS + 2), a child of the wing's, at wing_flame_height above it (MAT3:
 * 0.55), hidden until it fires. No flame where there is no wing (see StartWing). */
void StartWingFlame(s16 slot, s16 side) {
    s8 (*wings)[13] = (s8 (*)[13]) &gCarWings;   /* s8[][13]: the wing by row (player + 1) and body */
    tModelData *m;
    s32 index;
    s16 objnum;
    if (gRace == kGhost || gRace == kBattle || (m = PC_MODEL(slot), m->we_control != 0)) {
        PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].func = 0;
        return;
    }
    PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].func = AnimateWingFlame;
    PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].slot = slot;
    PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].data = side;
    index = wings[slot + 1][m->body_type] * 4 + side + OBJLIST_WINGS + 2;
    PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].objnum = InitCarObj(gObjList[index], 15, PC_CAR_VISUALS(slot)[side + VIS_WINGS].objnum, 0, slot, -1, 1);
    objnum = PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].objnum;
    ZOID_FindMatrix(objnum)->pos[1] = wing_flame_height;
    MBOX_HideObject(PC_CAR_VISUALS(slot)[side + VIS_WING_FLAMES].objnum, 1, 15);
}
