/* ShotInit: the battle's weapons at a battle's start (MAT3 shots.c). */
#include "game/game.h"

typedef struct NameEntry NameEntry;

/* PC: the element arrays have their elements' host layout, shots.c's Shot (104 bytes on the N64, 120 here), HitPoly
 * (8, 16), ShotObj (60, 72) and ShotCase (72, 80), which are linked through their first pointer, and the lists the
 * host element size (docs/port/POINTERS.md 9). */

#define ELEMENT_SIZE(n64, type) sizeof(type)
/* Sets up the shots' lists (24 shots, hit polygons and shot objects, 30 cases), finds the weapons' models
 * (WprObjNames, WfxObjNames) and the mines' and ripples' textures, turns every shield off and starts the streaks. */
void ShotInit(void)
{
    s32 *model;
    char **name;
    s32 i;
    wallTidx = 0;
    InitStatLL(&gShotList, gShots, ELEMENT_SIZE(104, PcShot), 24, 0);
    InitStatLL(&gHitPolyList, gHitPolys, ELEMENT_SIZE(8, PcHitPoly), 24, 0);
    InitStatLL(&gShotObjList, gShotObjs, ELEMENT_SIZE(60, PcShotObj), 24, 0);
    InitStatLL(&gShotCaseList, gShotCase, ELEMENT_SIZE(72, PcShotCase), 30, 0);
    for (name = WprObjNames, model = gWprObjList; model < gWprObjList + 10; model++, name++) {
        *model = MBOX_FindObject_WORK(*name, 0, gNumModels[0] - 1, 0);
    }
    for (name = WfxObjNames, model = gWfxObjList; model < gWfxObjList + 7; model++, name++) {
        *model = MBOX_FindObject_WORK(*name, 0, gNumModels[0] - 1, 0);
    }
    for (i = 0; i < 5; i++) {
        MBOX_FindTexture_WORK(MineTexNames[i], &gMineTextures[i], 0, gNumModels[0] - 1, 1);
    }
    for (i = 0; i < 5; i++) {
        MBOX_FindTexture_WORK(RipTexNames[i], &gRipTextures[i], 0, gNumModels[0] - 1, 1);
    }
    for (i = 0; i < 4; i++) {
        gCarShield[i].dlIdx = -1;
    }
    InitStreaks();
}
