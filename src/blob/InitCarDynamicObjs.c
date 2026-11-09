/* InitCarDynamicObjs.c: find a car's part definitions in its model file (MAT3 visuals.c, InitCarDynamicObjs()). */
#include "game/game.h"

#define MBOX_WARN 1             /* MBOX_FindObject_WORK: complain if it isn't there */
#define CAR_PARTS 3             /* the frame, the hood, the sheen (MAT3 has 9) */

VIEW(char *[CAR_PARTS], carObjNames);
#define carObjNames carObjNames_view

extern int sprintf(char *, const char *, ...);

/* Look up the parts of car type `car` ("<car name><part name>", car_part_name_format) in model file `model` and keep
 * their definitions in gObjList from slot * 3 (OBJLIST_CAR_PARTS). */
void InitCarDynamicObjs(s16 slot, s16 car, s8 model) {
    s32 part;
    char partName[20];
    s32 objIndex;

    objIndex = slot * CAR_PARTS;
    for (part = 0; part < CAR_PARTS; ++part, ++objIndex) {
        sprintf(partName, car_part_name_format, CarNames[car], carObjNames[part]);
        gObjList[objIndex] = MBOX_FindObject_WORK(partName, (s32)model, (s32)model, MBOX_WARN);
    }
}
