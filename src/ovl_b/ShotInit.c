/* ShotInit: the battle's weapons at a battle's start (MAT3 shots.c). */
#include "game/game.h"

typedef struct NameEntry NameEntry;

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(s16, wallTidx);
#define wallTidx wallTidx_view
/* PC: the list headers have the StatLL host layout (40 bytes) and the element arrays their elements' host layout,
 * shots.c's Shot (104 bytes on the N64, 120 here), HitPoly (8, 16), ShotObj (60, 72) and ShotCase (72, 80), which
 * are linked through their first pointer. These declarations give them that storage (gen_data), and the lists the
 * host element size (docs/port/POINTERS.md 9). */
typedef struct { u8 dbl; s32 numElements, elementSize; void *list, *head, *store; } PcStatLL;

VIEW(PcStatLL[1], gShotList);
#define gShotList gShotList_view
VIEW(PcStatLL[1], gHitPolyList);
#define gHitPolyList gHitPolyList_view
VIEW(PcStatLL[1], gShotObjList);
#define gShotObjList gShotObjList_view
VIEW(PcStatLL[1], gShotCaseList);
#define gShotCaseList gShotCaseList_view
VIEW(PcShot[24], gShots);
#define gShots gShots_view
VIEW(PcHitPoly[24], gHitPolys);
#define gHitPolys gHitPolys_view
VIEW(PcShotObj[24], gShotObjs);
#define gShotObjs gShotObjs_view
VIEW(PcShotCase[30], gShotCase);
#define gShotCase gShotCase_view
#define ELEMENT_SIZE(n64, type) sizeof(type)
VIEW(char *[10], WprObjNames);
#define WprObjNames WprObjNames_view
VIEW(char *[7], WfxObjNames);
#define WfxObjNames WfxObjNames_view
VIEW(char *[5], MineTexNames);
#define MineTexNames MineTexNames_view
VIEW(char *[5], RipTexNames);
#define RipTexNames RipTexNames_view
VIEW(s32[10], gWprObjList);
#define gWprObjList gWprObjList_view
VIEW(s32[7], gWfxObjList);
#define gWfxObjList gWfxObjList_view
VIEW(s16[5], gMineTextures);
#define gMineTextures gMineTextures_view
VIEW(s16[5], gRipTextures);
#define gRipTextures gRipTextures_view
typedef struct PlayerEffect { s32 handle; float transform[12]; } PlayerEffect;
VIEW(PlayerEffect[4], gCarShield);
#define gCarShield gCarShield_view
/* Sets up the shots' lists (24 shots, hit polygons and shot objects, 30 cases), finds the weapons' models
 * (WprObjNames, WfxObjNames) and the mines' and ripples' textures, turns every shield off and starts the streaks. */
void ShotInit(void)
{
    s32 *model;
    char **name;
    s32 i;
    wallTidx = 0;
    InitStatLL(gShotList, gShots, ELEMENT_SIZE(104, PcShot), 24, 0);
    InitStatLL(gHitPolyList, gHitPolys, ELEMENT_SIZE(8, PcHitPoly), 24, 0);
    InitStatLL(gShotObjList, gShotObjs, ELEMENT_SIZE(60, PcShotObj), 24, 0);
    InitStatLL(gShotCaseList, gShotCase, ELEMENT_SIZE(72, PcShotCase), 30, 0);
    for (name = WprObjNames, model = gWprObjList; model < gWprObjList + 10; model++, name++) {
        *model = MBOX_FindObject_WORK(*name, 0, gNumModels - 1, 0);
    }
    for (name = WfxObjNames, model = gWfxObjList; model < gWfxObjList + 7; model++, name++) {
        *model = MBOX_FindObject_WORK(*name, 0, gNumModels - 1, 0);
    }
    for (i = 0; i < 5; i++) {
        MBOX_FindTexture_WORK(MineTexNames[i], &gMineTextures[i], 0, gNumModels - 1, 1);
    }
    for (i = 0; i < 5; i++) {
        MBOX_FindTexture_WORK(RipTexNames[i], &gRipTextures[i], 0, gNumModels - 1, 1);
    }
    for (i = 0; i < 4; i++) {
        gCarShield[i].handle = -1;
    }
    InitStreaks();
}
