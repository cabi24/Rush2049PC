/* StartWeapon: a car's weapon appears on it (MAT3 weapons.c). */
#include "game/game.h"

/* A visual (MAT3 tVisual; the PC's 0x20-byte struct pc_visual: next and func are host pointers). */
typedef struct tVisual tVisual;
struct tVisual {
    tVisual *next;      /* 0x00 */
    s16 index;          /* 0x04 */
    s16 objnum;         /* 0x06 */
    s16 slot;           /* 0x08 */
    u32 data;           /* 0x0C */
    f32 timeStamp;      /* 0x10 */
    void (*func)(tVisual *, s16); /* 0x14 */
};

/* A car's parts (gCarParts): its frame's and its weapon's scene objects. */
typedef struct tCarParts {
    s32 frame;                      /* 0x00 */
    u8 unknown04[0x14];
    s32 weap;                       /* 0x18 */
    u8 unknown1c[0x24];
} tCarParts;                        /* 0x40 */

VIEW(tCarParts[], gCarParts);
#define gCarParts gCarParts_view
extern f32 weaponOffsets[8][13][3];

/* Puts the weapon on car `slot` if it has none: its weapon visual (visual 20) animates it (AnimateWeapon), the
 * weapon's object (object definition 217) on the car's frame at the car body's offset. */
void StartWeapon(s16 slot)
{
    tVisual *v;

    v = &PC_CAR_VISUALS(slot)[20];
    if (v->objnum < 0) {
        v->index = 0;
        v->func = AnimateWeapon;
        v->timeStamp = gSeconds;
        v->slot = slot;
        v->objnum = gCarParts[slot].weap =
            InitCarObj(gObjList[217], 15, gCarParts[slot].frame, 0x2000, slot, -1, 1);
        ZOID_UpdateObject(v->objnum, weaponOffsets[1][PC_MODEL_BODY(slot)], 0);
    }
}
