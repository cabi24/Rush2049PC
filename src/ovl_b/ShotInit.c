/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Image B: initialize four pools, resolve model/texture names, reset effects.
 * Strict standalone match: 140 words / 560 bytes, stock IDO 5.3.
 * The indexed texture loops preserve the known-global array provenance in UGEN,
 * so as1 can schedule the fifth outgoing argument before the name load. The
 * prior pointer-cursor form loses that no-alias fact and differs at six words.
 * No padding, unused locals, extra reads, or ABI shaping is required.
 * Existing lookup interfaces and PlayerEffect layout follow the existing
 * runtime_b_initializer_20261006; whole-function arcade ancestry
 * is unestablished. The inherited volatile count view is noncausal here.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef struct NameEntry NameEntry;
extern void InitStatLL(void *, void *, s32, s32, u8);
extern s32 MBOX_FindObject_WORK(char *, s8, s8, s8);
extern NameEntry *MBOX_FindTexture_WORK(char *, s16 *, s8, s8, s32);
extern void InitStreaks(void);
/* Same qualified view as both accepted lookup implementations; noncausal here. */
extern volatile u8 gNumModels;
extern s16 wallTidx;
/* PC: the list headers have the StatLL host layout (40 bytes) and the element arrays their elements' host layout,
 * shots.c's Shot (104 bytes on the N64, 120 here), HitPoly (8, 16), ShotObj (60, 72) and ShotCase (72, 80), which
 * are linked through their first pointer. These declarations give them that storage (gen_data), and the lists the
 * host element size (docs/port/POINTERS.md 9). */
typedef struct { u8 dbl; s32 numElements, elementSize; void *list, *head, *store; } PcStatLL;
typedef struct { void *next; s8 slot, slotDamaged, type, flags; float lifeTime, incTime, radius, vel[3], pos[3], old[3];
    float uvs[3][3]; void *streak, *obj, *obj2; } PcShot;
typedef struct { void *next, *poly; } PcHitPoly;
typedef struct { void *next; s32 mbIdx; float mat[12]; void *shotInfo; } PcShotObj;
typedef struct { void *next; s32 mbIdx; float mat[12]; float time, vel[3]; } PcShotCase;
extern PcStatLL gShotList[1], gHitPolyList[1], gShotObjList[1], gShotCaseList[1];
extern PcShot gShots[24];
extern PcHitPoly gHitPolys[24];
extern PcShotObj gShotObjs[24];
extern PcShotCase gShotCase[30];
#define ELEMENT_SIZE(n64, type) sizeof(type)
extern char *WprObjNames[10], *WfxObjNames[7];
extern char *MineTexNames[5], *RipTexNames[5];
extern s32 gWprObjList[10], gWfxObjList[7];
extern s16 gMineTextures[5], gRipTextures[5];
typedef struct PlayerEffect { s32 handle; float transform[12]; } PlayerEffect;
extern PlayerEffect gCarShield[4];
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
