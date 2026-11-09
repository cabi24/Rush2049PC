/* InitTrackDynamicObjs.c: find the definitions of the effects, weapons, wings and track objects (MAT3 visuals.c,
 * InitTrackDynamicObjs()). */
#include "game/game.h"
/* Look the race's shared object definitions up by name in every model file loaded (0 .. gNumModels - 1) and keep
 * them in gObjList: the hulk and the effects from OBJLIST_HULK, the weapons (OBJLIST_WEAPONS), the wings and their
 * flames (OBJLIST_WINGS), the mine when cones or cars carry mines, then the track's objects; then the targets' object
 * definitions (SetTargetDefs). A name that isn't there gives 0, silently. */
void InitTrackDynamicObjs(void)
{
    s32 i;

    for (i = OBJLIST_HULK; i < OBJLIST_WEAPONS; i++) {
        gObjList[i] = MBOX_FindObject_WORK(InitTrackDynamicObjs_ObjNames[i - OBJLIST_HULK], 0, (s8)(gNumModels[0] - 1), 0);
    }
    for (; i < OBJLIST_WINGS; i++) {
        gObjList[i] = MBOX_FindObject_WORK(WeaponNames[i - OBJLIST_WEAPONS], 0, (s8)(gNumModels[0] - 1), 0);
    }
    for (; i < OBJLIST_TRACK; i++) {
        gObjList[i] = MBOX_FindObject_WORK(WingNames[i - OBJLIST_WINGS], 0, (s8)(gNumModels[0] - 1), 0);
    }
    if (gMineCones || gCarMines) {
        gObjList[i] = MBOX_FindObject_WORK("MINEG1", 0, (s8)(gNumModels[0] - 1), 0);
        i++;
    }
    for (; i < 386; i++) {
        gObjList[i] = MBOX_FindObject_WORK(TrkNames[i - OBJLIST_TRACK], 0, (s8)(gNumModels[0] - 1), 0);
    }
    SetTargetDefs();
}
