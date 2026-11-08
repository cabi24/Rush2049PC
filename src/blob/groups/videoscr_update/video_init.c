/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Video-options screen setup (GC videoscr.c), for the updateVideoScreen group (updateVideoScreen):
 *  particle_render    (0x800B8650, 448 B) = GC calcMenuRect@videoscr.c (MAT3 0x8006519C)
 *  particle_collision (0x800B8820, 1012 B) = GC initScreen@videoscr.c (MAT3 0x8006425C) with GC initButtons@videoscr.c
 *                       (MAT3 0x80065338) written in place.
 * . Pattern from the audio pair (groups/audioscr_update/init_w36q.c; for audioscr).
 * Helpers vi_gfx_lock / vi_gfx_unlock / vi_SetFont (GC HAL_SetFont: no old-font value is kept, retail has no
 * `move s0,v0`) and vi_setButtonColor (GC ZOID_SetColor) are non-static and not on keep (stub rule).
 * DISCLOSED DEVICE: unused locals u0..u3 in particle_collision = the exact frame residual: retail frame 136, without
 * them 120 (4 slots, 16 B at the frame top; every sp-relative access is identical with and without them). The same
 * 16 B appear in drone_pathfind_main (GC initScreen@audioscr.c, also with initButtons written in place): most likely
 * retail's umerge inline area for initButtons, which the shadow unit cannot inline (umerge's knapsack is global).
 * N64 differences from GC: calcMenuRect uses font 11 and text tables 41/42 (GC 10, 42/43); validOption is the
 * group's getter video_validOption (optionValid[i] only); NewMultiBlit gets 2 entries for both defs; B1/arrow y is 55.2.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct OSMesgQueue OSMesgQueue;
typedef struct TexDef TexDef;
typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;
typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;   /* 0x40 */
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 color; u8 pad40[4]; } Ent;
typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct TextState { void *word0; char **single; void *word8; u16 *tab; char **taba; } TextState;
typedef struct { s32 x, w, h, cx; } MenuRect;

extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern MenuRect video_menuRect;                /* menuRect */
extern Button video_buttonInfo[12];              /* buttonInfo */
extern char *video_buttonNames[];                 /* buttonNames */
extern Ent ObjInstanceList[];
extern volatile u8 gNumModels;
extern f32 unitmatrix[3][3];               /* unitmatrix */
extern const struct pc_multiblit video_frontEndBlit[2], video_pauseBlit[2];   /* frontEndBlit, pauseBlit: two blits each, as
                                         * NewMultiBlit gets them (the second ran past a one-blit table) */
extern Blit *video_blit;                   /* blit */
extern s32 video_buttonsInited, video_buttonsJustInited;         /* buttonsInited, buttonsJustInited */
extern s8 gOptions[];                    /* gOptions */
extern s32 gVertCenter, gHorzCenter;         /* gVertCenter, gHorzCenter */
extern s32 gVideoMenuSelect;
extern s8 gTestFrequency, gConfirmFrequency, gYesFrequency;   /* gTestFrequency, gConfirmFrequency, gYesFrequency */
extern s32 exitVideoScreen;
extern s8 video_inited;                      /* inited */
extern u32 gGameState;

s32 osRecvMesg(OSMesgQueue *, void *, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32);
u32 HAL_GetTextWidth(void *, s16);
s16 TB_GetFontLineHeight(void);
s32 TB_Sprintf(char *, char *, ...);
s32 video_validOption(s32 i);                  /* validOption (updateVideoScreen getter) */
void *memcpy(void *, const void *, u32);
s32 MBOX_FindObject_WORK(char *, s8, s8, s32);
s16 MBOX_NewObject(s32, void *, s32, u32);       /* MBOX_NewObject wrapper */
TexDef *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, s32);
void ZOID_SetTexture(s16, TexDef *, s32);
void WPitchUV(f32, f32 [3][3]);
void ScaleUV(f32 [3][3], f32 [3][3], f32);
void SaveGlobals(void);
void createBG(void);
void ZeroCameras(void);
void ClearRushControls(void);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);

void vi_gfx_lock(void) { osRecvMesg(&text_lock_queue, 0, 1); }
void vi_gfx_unlock(void) { osSendMesg(&text_lock_queue, 0, 0); }
void vi_SetFont(s32 font)
{
    vi_gfx_lock();
    HAL_SetFontInternal(font);
    vi_gfx_unlock();
}

void video_calcMenuRect(void)
{
    s32 menuSize;
    s32 optionWidth;
    s32 valueWidth;
    s32 menuIdx;
    u8 str1[40];

    vi_SetFont(11);
    menuSize = 0;
    optionWidth = 0;
    valueWidth = 0;
    for (menuIdx = 0; menuIdx < 4; menuIdx++) {
        if (video_validOption(menuIdx) == 1) {
            menuSize++;
            optionWidth = HAL_GetTextWidth(gText.taba[gText.tab[41] + menuIdx], -1) > optionWidth ?
                HAL_GetTextWidth(gText.taba[gText.tab[41] + menuIdx], -1) : optionWidth;
            if (menuIdx == 2) {
                TB_Sprintf((char *)str1, "%s  %s", (gText.taba + gText.tab[42])[0],
                             (gText.taba + gText.tab[42])[1]);
                valueWidth = HAL_GetTextWidth(str1, -1) > valueWidth ? HAL_GetTextWidth(str1, -1) : valueWidth;
            } else {
                valueWidth = HAL_GetTextWidth("000", -1) > valueWidth ? HAL_GetTextWidth("000", -1) : valueWidth;
            }
        }
    }
    video_menuRect.w = optionWidth + valueWidth + 10;
    video_menuRect.h = menuSize * TB_GetFontLineHeight();
    video_menuRect.x = 160 - video_menuRect.w / 2;
    video_menuRect.cx = video_menuRect.x + optionWidth + 5;
}

void vi_setButtonColor(s16 objnum, Color *col) { ObjInstanceList[objnum].color = col->rgba; }

void video_initScreen(void)
{
    s32 u0, u1, u2, u3;

    SaveGlobals();
    video_calcMenuRect();
    if (gGameState & 0x7C03FFFE) {
        createBG();
        {
            Color buttonColor = { { 255, 255, 255, 255 } };
            TexDef *tex;
            u16 texIdx;
            s32 buttonIdx;

            for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
                if (video_buttonInfo[buttonIdx].handle == -1) {
                    memcpy(video_buttonInfo[buttonIdx].mat, unitmatrix, 36);
                    video_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                        MBOX_FindObject_WORK(video_buttonNames[video_buttonInfo[buttonIdx].type], 0, gNumModels - 1, 1),
                        &video_buttonInfo[buttonIdx].mat, -1,
                        (video_buttonInfo[buttonIdx].type == 0 || video_buttonInfo[buttonIdx].type == 1 ||
                         video_buttonInfo[buttonIdx].type == 2) ? 0x42000 : 0);
                    vi_setButtonColor(video_buttonInfo[buttonIdx].handle, &buttonColor);
                }
            }
            tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(video_buttonInfo[8].handle, tex, -1);
            tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(video_buttonInfo[9].handle, tex, -1);
            memcpy(video_buttonInfo[9].mat, unitmatrix, 36);
            WPitchUV(-1.5707964f, video_buttonInfo[9].mat);
            video_buttonInfo[9].pos[0] = -75.1f;
            video_buttonInfo[9].pos[1] = 55.2f;
            video_buttonInfo[9].pos[2] = 100.0f;
            ScaleUV(video_buttonInfo[9].mat, video_buttonInfo[9].mat, 0.5f);
            memcpy(video_buttonInfo[10].mat, unitmatrix, 36);
            WPitchUV(-1.5707964f, video_buttonInfo[10].mat);
            video_buttonInfo[10].pos[0] = -87.1f;
            video_buttonInfo[10].pos[1] = 55.2f;
            video_buttonInfo[10].pos[2] = 100.0f;
            tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(video_buttonInfo[11].handle, tex, -1);
            memcpy(video_buttonInfo[11].mat, unitmatrix, 36);
            WPitchUV(-1.5707964f, video_buttonInfo[11].mat);
            video_buttonInfo[11].pos[0] = 0.0f;
            video_buttonInfo[11].pos[1] = 115.0f;
            video_buttonInfo[11].pos[2] = 200.0f;
            video_buttonsInited = 1;
            video_buttonsJustInited = 1;
            ZeroCameras();
        }
        video_blit = NewMultiBlit(0, 0, &video_frontEndBlit, 2);
    } else {
        video_blit = NewMultiBlit(0, 0, &video_pauseBlit, 2);
    }
    gVertCenter = gOptions[20];
    gHorzCenter = gOptions[19];
    ClearRushControls();
    while (!video_validOption(gVideoMenuSelect)) {
        if (++gVideoMenuSelect >= 4) {
            gVideoMenuSelect = 0;
            break;
        }
    }
    gTestFrequency = 0;
    gConfirmFrequency = 0;
    gYesFrequency = 0;
    exitVideoScreen = 0;
    video_inited = 1;
}
