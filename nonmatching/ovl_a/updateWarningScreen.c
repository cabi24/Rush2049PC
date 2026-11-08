/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* updateWarningScreen@warningscr.c (image A), straight port of the Dreamcast warningscr.c text (closest
 * to the N64 body: MAT3 keeps only the tail) with the N64 additions read from the retail words: 4 paks,
 * PS_GetBlocks(0x810), the gActiveEdges 0xC00 gPakScr toggle and the `& 3` start test. Statics as in DC. */
typedef signed char s8; typedef unsigned char u8;
typedef short s16; typedef unsigned short u16; typedef int s32; typedef unsigned int u32; typedef float f32;
typedef struct Blit Blit;
/* PC: WarningBlit is an array, which gcc wants complete: the MultiBlit of the other image-A screens. */
typedef struct MultiBlit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    u32 zdepth, alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;
typedef struct Controller { s8 attached; u8 rest[15]; } Controller;

extern s8 pak_recognize_error_seen;
extern struct PlayerFiles { u32 unknown0, unknown4; void *head, *tail; } gPlayerFiles;   /* the profiles on the paks */
extern Controller gControllerData[4];  /* gControllerData */
extern u32 gActiveEdges;
extern u8 gBlocksNeeded;
extern MultiBlit warningscr_blits[1];   /* WarningBlit: one, as NewMultiBlit gets it. By the gap it ran over the strings after
                                   * it, which other screens' blits name, and turned them into words */

extern s8 PS_PakInserted(s32);
extern s8 PS_PakDamaged(s32);
extern s8 PS_PakRumble(s32);
extern u32 PS_GetFreeBlocks(s32);
extern u32 PS_GetBlocks(s32);
extern u32 PS_GetFreeFiles(s32);
extern Blit *NewMultiBlit(s16, s16, MultiBlit *, s16);
extern void RemoveBlit(Blit *);
extern void SetControllerActive(s32, s32);
extern void SetControllerStickEdges(s32, s32);
extern void PlayUDSound(u32);
extern void PlayStartABSound(u32);
extern void setGameState(s32);

/* PC: both runtime images are linked in, and image B has a function at 0x8038A400 too (findDamageSlot, worlddam.c),
 * which the loader's tables name for both. Image A's is updateWarningScreen, which updateFrontEnd calls in
 * state 0x40000000 and port/data's function table gives image A's data (docs/port/SYNTH.md). Its two file statics
 * are read by drawWarningScreen (warningscr_animText) by their addresses, so they are the image A arena's bytes. */
#define numPaksFound num_paks_found
extern s8 gPakScr;
extern u8 numPaksFound;

void updateWarningScreen(void)
{
    static Blit *sBlit = 0;       /* D_803B83C8 */
    s32 numPaks;
    s32 i;

    if (!sBlit) {
        gPakScr = 0;
        pak_recognize_error_seen = 0;
    }
    if (!gPlayerFiles.head) {
        for (i = 0; i < 4; i++) {
            if (gControllerData[i].attached) break;
        }
        if (i < 4) {
            numPaks = 0;
            for (i = 0; i < 4; i++) {
                if (PS_PakInserted(i) && !PS_PakDamaged(i) && !PS_PakRumble(i)) {
                    numPaks++;
                    if (PS_GetFreeBlocks(i) >= PS_GetBlocks(0x810) && PS_GetFreeFiles(i) >= 1) goto done;
                }
            }
            numPaksFound = numPaks;
            if (numPaksFound > 0 || !pak_recognize_error_seen) {
                if (!sBlit) {
                    sBlit = NewMultiBlit(0, 0, warningscr_blits, 1);
                    SetControllerActive(-1, 1);
                    SetControllerStickEdges(-1, 1);
                }
                if (numPaksFound > 0) {
                    if (gActiveEdges & 0xc00) {
                        PlayUDSound(gActiveEdges);
                        gPakScr = !gPakScr;
                    }
                } else {
                    gPakScr = 0;
                }
                if (!(3 & gActiveEdges)) return;
                PlayStartABSound(gActiveEdges);
            }
        }
    }
done:
    if (sBlit) {
        RemoveBlit(sBlit);
        sBlit = 0;
    }
    gBlocksNeeded = PS_GetBlocks(0x810);
    setGameState(gPakScr ? 0x4000000 : 4);
}
