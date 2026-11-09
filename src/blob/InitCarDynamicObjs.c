#include "game/game.h"


#define MBOX_WARN 1
#define CAR_PARTS 3

VIEW(char *[CAR_PARTS], carObjNames);
#define carObjNames carObjNames_view

extern int sprintf(char *, const char *, ...);

void InitCarDynamicObjs(s16 slot, s16 car, s8 model) {
    s32 i, j;
    char partName[20];
    s32 k;

    k = slot * CAR_PARTS;
    for (j = 0; j < CAR_PARTS; ++j, ++k) {
        sprintf(partName, car_part_name_format, CarNames[car], carObjNames[j]);
        gObjList[k] = MBOX_FindObject_WORK(partName, (s32)model, (s32)model, MBOX_WARN);
    }
}
