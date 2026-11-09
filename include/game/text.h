/* include/game/text.h: fonts and text */
#ifndef GAME_TEXT_H
#define GAME_TEXT_H

#include "game/types.h"

/* Data */
extern u64 gspF3DEX2_NoN_fifoTextStart[EXTENT(626)];
extern u64 gspF3DEX2_NoN_fifoDataStart[EXTENT(132)];
extern Gfx rdp_init_dl[EXTENT(17)];
extern Gfx rsp_init_dl[EXTENT(11)];
extern u64 rsp_dram_stack[EXTENT(128)];
extern u64 rsp_yield_buffer[EXTENT(384)];
extern u64 rdp_fifo_buffer[EXTENT(4096)];
extern u64 rdp_fifo_buffer_end[EXTENT(512)];
extern u32 gfx_tasks_pending;
extern s8 gFinished;
extern void *gFontHeap;
extern f32 gDepth;
extern f32 gTrans;
extern s8 gBufferInited;
extern MenuDepthPoint menu_text_depth_curve[EXTENT(14)];
extern s8 text_lock_inited;
extern void *pause_gTitlePane;
extern s8 gInited_TBText;
extern ScreenPos gJust;
extern TextColor gFlashColor1;
extern TextColor gFlashColor2;
extern volatile f32 gColorCycleTime;
extern Color8 gFontColors[EXTENT(22)];
extern s8 text_wave_inited;
extern char pause_title_format[EXTENT(20)];
extern char pause_title_name_format[EXTENT(8)];
extern s16 gLang;
extern u8 gTextWave;
extern s32 gTextWaveInst;
extern s32 gFontDatWaveInst;
extern s32 gFontTexWaveInst;
extern struct Bank *gFontHeader;
extern struct anon_kerns_value_c *gFontLetters;
extern struct tTexture *gFontTextures[EXTENT(16)];
extern s16 gCharMap[EXTENT(256)];
extern s32 font_image_format;
extern s32 font_texel_size;
extern PcText4 gFontColor;
extern s8 gLeading;
extern s8 gSpacing;
extern u32 gFontAttribute;
extern s16 gFontXpos;
extern s16 gFontYpos;
extern s8 gFontIndex;
extern s32 gBufLen;
extern u8 gBuffer[EXTENT(800)];
extern Gfx *last_frame_dl_end;
extern s32 last_frame_text_dl_end;
extern TextTables gText;
extern f32 gColorCycle;

/* Functions */
void AnimateObjSwapGeneric(struct Obj *obj, s16 mode);
void ClearMemoryHeap(struct Heap *hint);
void DrawText(const u8 *text, u16 length);
void EndGraphics(void);
void font_get_char_list(u8 *out);
void HAL_DrawText(void *text, s16 numChars);
void HAL_FinishText(s8 end);
s32 HAL_GetCharWidth(u16 id);
u8 HAL_GetFontAscent(void);
u8 HAL_GetFontDescent(void);
s8 HAL_GetFontLeading(void);
s8 HAL_GetFontSpacing(void);
s32 HAL_GetTextWidth(u8 *str, s16 maxlen);
void HAL_InitText(void);
void HAL_PurgeFonts(void);
s32 HAL_SetFont(s32 fontID);
s8 HAL_SetFontFixedWidth(s8 value);
s32 HAL_SetFontInternal(s32 selection);
void HAL_SetFontLeading(s16 a);
void HAL_SetFontSpacing(s16 a);
void HAL_SetMenuTextDepth(f32 value);
void HAL_SetTextColor(u8 a, u8 b, u8 c, u8 d);
void HAL_SetTextDepth(f32 value);
void HAL_SetTextPos(s16 arg0, s16 arg1);
void HAL_SetTextTranslucency(f32 x);
void HAL_StartText(void);
void InitTextWave(void);
void LoadTextWave(void);
void pause_TitlePane(void);
void process_scheduler_message(void);
void SetDefaults(u8 *rgba, f32 *amount, s16 *x, s16 *y, s8 *bank, s8 *mode, s8 *variant);
void SetupFontData(s32 force);
void StartGraphics(void);
s16 TB_CenterText(void *arg0, s16 arg1);
void TB_DrawEllipseText(s16 arg0, s16 arg1, s16 arg2, void *arg3);
void TB_DrawText(s16 x, s16 y, void *text);
void TB_DrawTextC(s16 arg0, s16 arg1, s32 arg2, void *arg3);
void TB_DrawTextScrollBox(s16 x, s16 y, s16 width, s16 height, s16 page, s16 line, void *text);
void TB_EllipseText(void *dst, void *src, s16 width);
f32 TB_GetFlashTextCycle(void);
s16 TB_GetFontHeight(void);
s16 TB_GetFontLineHeight(void);
s16 TB_GetTextNumLines(s16 width, u8 *text);
void TB_Printf(s16 arg0, s16 arg1, const char *arg2, ...);
s16 TB_RightText(void *arg0, s16 arg1);
void TB_SetJust(u32 first, u32 second);
void TB_SetTextColor(s32 mode);
void TB_Sprintf(char *arg0, const char *arg1, ...);
u8 *TB_Strcat(u8 *destination, u8 *source);
u8 *TB_Strcpy(u8 *destination, u8 *source);
s32 TB_Strlen(u8 *string);
void TB_TextOncePerFrame(void);
void text_lock(void);
void text_unlock(void);
u32 TextLen(const u8 *text, s16 numChars);

#endif /* GAME_TEXT_H */
