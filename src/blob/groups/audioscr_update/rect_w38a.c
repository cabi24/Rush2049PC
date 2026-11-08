/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * drone_collision_avoid (0x800D6E7C, 852 B) = MAT3/DC calcMenuRect@audioscr.c: widest option / value text of the
 * audio menu (HAL_GetTextWidth, TB_Strcpy, song names gSongNames, the
 * upper-cased song strings), then menuRect (audio_menuRect) w/h/x/cx with TB_GetFontLineHeight.
 * Internal (sole caller drone_pathfind_main = initScreen@audioscr.c, audio_options_menu group).
 * Base: rect_best / rect/v/a0.c (5 words: the menuIdx IV home at sp+28, retail sp+32).
 * levers: (1) validOption is the group's own audio_validOption (`v = 1; return v;`, the Dreamcast decomp's
 * validOption shape with its named local; already gave the 0x800D690C stub this body), called and inlined:
 *   its local is the missing 4-byte step of the inline area (MAT3's `return 1;` copy left the IV home at sp+28);
 *   (2) HAL_SetFont in the MAT3
 *   two-level shape (cmr_font_set = lock / _HAL_SetFont (HAL_SetFontInternal) / unlock) called directly, no extra
 *   void wrapper level.  -> EQUAL.
 * DISCLOSED: one unused local `u0` (second declaration). Frame arithmetic: retail frame 144; without u0 the frame is
 *   136 (one 4-byte local slot, frame rounded to 8); with it 144 and every sp offset is retail's.
 * Helpers cmr_* are non-static inlined helpers (no retail stubs): HAL_text.c's _GetSemaphore/_ReleaseSemaphore and
 *   HAL_SetFont. cmr_gfx_unlock is DEFINED AFTER cmr_font_set (prototype before), the
 *   highscorescr_draw/replay_save_prompt precedent: as1 breaks the `move s0,v0` / osSendMesg-argument tie by source line.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16; typedef int s32; typedef unsigned int u32; typedef float f32;
typedef struct TexDef TexDef; typedef struct Blit Blit; typedef struct MultiBlit MultiBlit;
typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;
typedef struct Ent {
    /* 0x00 */ s32 w0;
    /* 0x04 */ s32 w4;
    /* 0x08 */ void *owner;
    /* 0x0C */ f32 scaleA;
    /* 0x10 */ f32 scaleB;
    /* 0x14 */ u16 id;          /* 0xFFFF = free */
    /* 0x16 */ s16 link[3];     /* -1 = none */
    /* 0x1C */ s32 w28[8];
    u32 color;
    /* 0x40 */ s32 w64;
} Ent; /* size 0x44 */


typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; } AudioCfg;
extern AudioCfg gOptions;
extern Button audio_buttonInfo[12]; extern Ent ObjInstanceList[];
extern char *audio_buttonNames[]; extern u8 gNumModels; extern f32 unitmatrix[3][3];
extern char audio_init_texture_name_1[],audio_init_texture_name_2[],audio_init_texture_name_3[];
extern const MultiBlit audio_frontEndBlit,audio_pauseBlit;
extern Blit *audio_blit;
extern s32 audio_buttonsInited,audio_buttonsJustInited,gAudioMenuSelect,exitAudioScreen,audio_gSong;
extern s8 gGlobalsSet,audio_inited; extern u32 gGameState;
void *memcpy(void *,const void *,u32);
s32 MBOX_FindObject_WORK(char *,s8,s8,s32);
s16 ZOID_AddObject(s32,void *,s16,s32);
TexDef *MBOX_FindTexture_WORK(char *,u16 *,s8,s8,s32);
void ZOID_SetTexture(s16,TexDef *,s32);
void WPitchUV(f32,f32 [3][3]); void ScaleUV(f32 [3][3],f32 [3][3],f32);
void createBG(void); void ZeroCameras(void); void ClearRushControls(void);
Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);
void audio_calcMenuRect(void);void AudKillSong(void);void AudSetMusicVol(f32,f32);void AudPlaySong(s32,s32);void StartGameMusic(void);
s16 MBOX_NewObject(s32,void *,s32,u32);void SaveGlobals(void);
typedef struct OSMesgQueue OSMesgQueue;
s32 osSendMesg(OSMesgQueue *, void *, s32);

typedef struct TextState { void *word0; char **single; void *word8; u16 *tab; char **taba; } TextState;
typedef struct AudioMenuRect { s32 x,w,h,cx; } AudioMenuRect;
extern TextState gText;extern AudioMenuRect audio_menuRect;
extern char *gSongNames[];
extern OSMesgQueue text_lock_queue;
s32 osRecvMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32);u32 HAL_GetTextWidth(void *,s16);s16 TB_GetFontLineHeight(void);void *TB_Strcpy(void *,void *);
s32 audio_validOption(s32 i);   /* validOption (audio_options_menu member) */
void cmr_gfx_lock(void) { osRecvMesg(&text_lock_queue, 0, 1); }
void cmr_gfx_unlock(void);
s32 cmr_font_set(s32 font)
{
    s32 old;
    cmr_gfx_lock();
    old = HAL_SetFontInternal(font);
    cmr_gfx_unlock();
    return old;
}
void cmr_gfx_unlock(void) { osSendMesg(&text_lock_queue, 0, 0); }
void audio_calcMenuRect(void)
{
    s32 menuIdx,u0,menuCount,optionWidth,valueWidth;
    u8 str[40];
    u8 *charPtr;
    s32 songIdx;
    cmr_font_set(10);
    optionWidth=0;
    valueWidth=0;
    menuCount=0;
    for(menuIdx=0;menuIdx<4;menuIdx++) {
        optionWidth = HAL_GetTextWidth(gText.taba[gText.tab[39]+menuIdx],-1)>optionWidth ? HAL_GetTextWidth(gText.taba[gText.tab[39]+menuIdx],-1):optionWidth;
        if(audio_validOption(menuIdx)) {
            switch(menuIdx) {
            case 0: case 1:
                valueWidth=HAL_GetTextWidth("10",-1)>valueWidth ? HAL_GetTextWidth("10",-1):valueWidth;
                break;
            case 2:
                for(songIdx=0;songIdx<14;songIdx++) {
                    if(songIdx<12) TB_Strcpy(str,gSongNames[songIdx]);
                    else if(songIdx==12) TB_Strcpy(str,gText.single[34]);
                    else if(songIdx==13) TB_Strcpy(str,gText.taba[gText.tab[16]]);
                    for(charPtr=str;*charPtr != 0;charPtr++)
                        *charPtr=(*charPtr>='a' && *charPtr<='z') ? *charPtr^0x20:*charPtr;
                    valueWidth=HAL_GetTextWidth(str,-1)>valueWidth ? HAL_GetTextWidth(str,-1):valueWidth;
                }
                break;
            case 3:
                valueWidth=HAL_GetTextWidth(gText.taba[gText.tab[40]],-1)>valueWidth ? HAL_GetTextWidth(gText.taba[gText.tab[40]],-1):valueWidth;
                valueWidth=HAL_GetTextWidth(gText.taba[gText.tab[40]+1],-1)>valueWidth ? HAL_GetTextWidth(gText.taba[gText.tab[40]+1],-1):valueWidth;
                break;
            }
            menuCount++;
        }
    }
    audio_menuRect.w=optionWidth+valueWidth+10;
    audio_menuRect.h=menuCount*TB_GetFontLineHeight();
    audio_menuRect.x=160-audio_menuRect.w/2;
    audio_menuRect.cx=audio_menuRect.x+optionWidth+5;
}
