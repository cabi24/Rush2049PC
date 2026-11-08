/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitTrackDynamicObjs (historical label; 0x800B338C, 504 bytes): resolves the shared model/texture name
 * tables into ids for the current language and then instantiates the level objects.
 *   gObjList[157..215]  <- InitTrackDynamicObjs_ObjNames[59]   ("HULKL1_LOD1", "GEOMEXPSPHERE01", ...)
 *   gObjList[216..223]  <- WeaponNames[8]    weapon models ("WEP_CANNG1", ...)
 *   gObjList[224..235]  <- WingNames[12]   wing parts ("WINGSWING1L", ...)
 *   one more "MINEG1" entry when gMineCones or gCarMines is set (the following indices shift by one)
 *   the rest up to [385]  <- TrkNames[i - 236] (track props, "CONE1G1", ...)
 * then calls SetTargetDefs (level-object instantiation, locked in level_objects).
 * One running index i across all loops (retail's s2 = 4*i); MBOX_FindObject_WORK(name, 0, lang - 1, 0)
 * returns the id. Own .rodata: the "MINEG1" literal at 0x8012325C (verified by the scorer).
 * Single kept function in the whole-program unit; no shaping quirks.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int s32;

extern u8 gNumModels;
extern s8 gMineCones;
extern s8 gCarMines;
extern u16 gObjList[];
extern char *InitTrackDynamicObjs_ObjNames[];
extern char *WeaponNames[];
extern char *WingNames[];
extern char *TrkNames[];
extern s32 MBOX_FindObject_WORK(char *, s8, s8, s8);
extern void SetTargetDefs(void);

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
