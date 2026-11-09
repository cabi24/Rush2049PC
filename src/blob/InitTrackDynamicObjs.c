#include "game/game.h"

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

void InitTrackDynamicObjs(void)
{
    s32 i;

    for (i = 157; i < 216; i++) {
        gObjList[i] = MBOX_FindObject_WORK(InitTrackDynamicObjs_ObjNames[i - 157], 0, (s8)(gNumModels - 1), 0);
    }
    for (; i < 224; i++) {
        gObjList[i] = MBOX_FindObject_WORK(WeaponNames[i - 216], 0, (s8)(gNumModels - 1), 0);
    }
    for (; i < 236; i++) {
        gObjList[i] = MBOX_FindObject_WORK(WingNames[i - 224], 0, (s8)(gNumModels - 1), 0);
    }
    if (gMineCones || gCarMines) {
        gObjList[i] = MBOX_FindObject_WORK("MINEG1", 0, (s8)(gNumModels - 1), 0);
        i++;
    }
    for (; i < 386; i++) {
        gObjList[i] = MBOX_FindObject_WORK(TrkNames[i - 236], 0, (s8)(gNumModels - 1), 0);
    }
    SetTargetDefs();
}
