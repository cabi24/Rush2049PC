/* InitCarObj.c: make one of a car's scene objects (MAT3 visuals.c, InitCarObj()). */
#include "game/game.h"

s32 ZOID_AddObject(s32 id, void *owner, s16 parent, s32 flags);

/* Add a scene object of definition `objnum` under `parent` (-1 none) and return its handle. `camlink` is the mask
 * of the windows that show it (15 all four; the object is hidden in the others). Its matrix: a new one in a race
 * (`for_game`); on the car screen the car's preset matrix for part `car_obj_num` of the screen player slot / 13
 * (gCarMatrices, 12 a player), or the identity for a part that has none (-1), which then never moves (flag 0x80).
 * MAT3 returns MBOX_NewObjectCam's result; the N64 left ZOID_AddObject's in its result register, which every caller
 * reads (docs/port/PROTOTYPES.md). */
s32 InitCarObj(s32 objnum, s16 camlink, s16 parent, s32 flags, s16 slot, s16 car_obj_num, s32 for_game)
{
    void *matrix;

    if (for_game != 0) {
        matrix = ZOID_NewMatrix();
    } else if (car_obj_num == -1) {
        matrix = &unitmatrix;
        flags |= 0x80;
    } else {
        matrix = gCarMatrices[slot / 13 * 12 + car_obj_num];
    }
    return ZOID_AddObject(objnum, matrix, parent, ((camlink << 8) ^ 0xF00) | flags);
}
