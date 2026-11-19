/* StartWeapon: a car's weapon appears on it (MAT3 weapons.c). */
#include "game/game.h"

extern f32 weaponOffsets[8][13][3];

/* Puts the weapon on car `slot` if it has none: its weapon visual (visual 20) animates it (AnimateWeapon), the
 * weapon's object (object definition 217) on the car's frame at the car body's offset. */
void StartWeapon(s16 slot)
{
    tVisual *v;

    v = &pc_cars[slot].visuals[20];
    if (v->objnum < 0) {
        v->index = 0;
        v->func = AnimateWeapon;
        v->timeStamp = gSeconds;
        v->slot = slot;
        v->objnum = gCarParts[slot].weap =
            InitCarObj(gObjList[217], 15, gCarParts[slot].frame, 0x2000, slot, -1, 1);
        ZOID_UpdateObject(v->objnum, weaponOffsets[1][pc_models[slot].body_type], 0);
    }
}
