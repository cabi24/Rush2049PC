/* include/game/text.h: fonts and text.
 *
 * Two layers (docs/code/ui.md): HAL_text (MAT3 HAL_text.c), the fonts (one data wave and one texture wave each) and
 * the drawing of a string with the blitter, buffered as commands in gBuffer between HAL_StartText and
 * HAL_FinishText; and TB_text (MAT3 TB_text.c), what the screens call: justification, the colour table, the
 * flashing colour, word-wrapped boxes and the string helpers, which also handle the double-byte strings (first byte
 * 0xFF, then two bytes a character, ended by two zeros). The text lock (text_lock, text_unlock) keeps the game thread
 * and the frame's end from using the buffer at once. */
#ifndef GAME_TEXT_H
#define GAME_TEXT_H

#include "game/types.h"

/* TB_SetJust: how TB_DrawText and the boxes place a string against x and y (MAT3 enum kTB_JustBottom). */
enum { kTB_JustLeft = 0, kTB_JustCenter = 1, kTB_JustRight = 2, kTB_JustTop = 3, kTB_JustBottom = 4 };

/* TB_SetTextColor: the entries of the colour table gFontColors, and the flashing colour (MAT3 enum kTB_FlashText). */
enum {
    kTB_BlackText = 0, kTB_WhiteText = 1, kTB_GrayText = 2, kTB_MedGryText = 3, kTB_DrkGryText = 4, kTB_RedText = 5,
    kTB_MagentaText = 6, kTB_BlueText = 7, kTB_MedBlueText = 8, kTB_DrkBlueText = 9, kTB_CyanText = 10,
    kTB_GreenText = 11, kTB_MedGrnText = 12, kTB_DrkGrnText = 13, kTB_YellowText = 14, kTB_DrkYelText = 15,
    kTB_YelBlkText = 16, kTB_YelRedText = 17, kTB_RedYelText = 18, kTB_RedRedText = 19, kTB_CynCynText = 20,
    kTB_CynGrnText = 21,
    kTB_FlashText = 22 /* between gFlashColor2 and gFlashColor1, by gColorCycle */
};

/* A double-byte string starts with this byte (TB_Strlen ...). */
#define TEXT_DOUBLE_BYTE 0xFF

/* HAL_SetTextPos: leave that coordinate as it is. */
#define TEXT_POS_KEEP (-0x8000)

/* gFontAttribute holds the blitter attributes (BLIT_ATTR_*) the text is drawn with: BLIT_ATTR_TALL from
 * HAL_StartText on a tall screen, BLIT_ATTR_ZDIST from a text depth, BLIT_ATTR_XLU from a translucency. */

/* The waves of font n (0..15): its data (header, letters, kerning) and its texture pages (the N64's numbering; MAT3's
 * are 24 + n and 8 + n). */
#define FONT_DATA_WAVE(n)    (0x26 + (n))
#define FONT_TEXTURE_WAVE(n) (0x16 + (n))
#define NUM_FONTS 16

/* HAL_DrawText's buffered commands in gBuffer (HAL_FinishText runs them). */
enum {
    TEXT_CMD_FONT = 0,         /* a byte: the font */
    TEXT_CMD_LEADING = 1,      /* a byte */
    TEXT_CMD_SPACING = 2,      /* a byte */
    TEXT_CMD_POS = 3,          /* x and y, two big-endian halfwords */
    TEXT_CMD_TRANSLUCENCY = 4, /* the float's bits, big-endian */
    TEXT_CMD_COLOR = 5,        /* RGBA */
    TEXT_CMD_TEXT = 6          /* a length byte, then the string's bytes */
};
#define TEXT_BUFFER_SIZE 800   /* gBuffer */

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
extern TextJust gJust;
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
extern FontHeader *gFontHeader;   /* the current font's header (SetupFontData): the start of its data wave */
extern FontLetter *gFontLetters;
extern struct tTexture *gFontTextures[EXTENT(16)];
extern s16 gCharMap[EXTENT(256)];
extern s32 font_image_format;
extern s32 font_texel_size;
extern TextColor gFontColor;   /* the text's colour, R, G, B, A (HAL_SetTextColor) */
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
s32 HAL_SetFontInternal(s32 fontID);
void HAL_SetFontLeading(s16 a);
void HAL_SetFontSpacing(s16 a);
void HAL_SetMenuTextDepth(f32 value);
void HAL_SetTextColor(u8 red, u8 green, u8 blue, u8 alpha);
void HAL_SetTextDepth(f32 value);
void HAL_SetTextPos(s16 x, s16 y);
void HAL_SetTextTranslucency(f32 x);
void HAL_StartText(void);
void InitTextWave(void);
void LoadTextWave(void);
void pause_TitlePane(void);
void process_scheduler_message(void);
void SetDefaults(u8 *fontColor, f32 *trans, s16 *fontXpos, s16 *fontYpos, s8 *fontIndex, s8 *leading, s8 *spacing);
void SetupFontData(s32 force);
void StartGraphics(void);
s16 TB_CenterText(void *text, s16 centerX);
void TB_DrawEllipseText(s16 x, s16 y, s16 width, void *text);
void TB_DrawText(s16 x, s16 y, void *text);
void TB_DrawTextC(s16 x, s16 y, s32 color, void *text);
void TB_DrawTextScrollBox(s16 x, s16 y, s16 width, s16 height, s16 page, s16 line, void *text);
void TB_EllipseText(void *dst, void *src, s16 width);
f32 TB_GetFlashTextCycle(void);
s16 TB_GetFontHeight(void);
s16 TB_GetFontLineHeight(void);
s16 TB_GetTextNumLines(s16 width, u8 *text);
void TB_Printf(s16 x, s16 y, const char *format, ...);
s16 TB_RightText(void *text, s16 rightX);
void TB_SetJust(u32 hJust, u32 vJust);
void TB_SetTextColor(s32 color);
void TB_Sprintf(char *buffer, const char *format, ...);
u8 *TB_Strcat(u8 *destination, u8 *source);
u8 *TB_Strcpy(u8 *destination, u8 *source);
s32 TB_Strlen(u8 *string);
void TB_TextOncePerFrame(void);
void text_lock(void);
void text_unlock(void);
u32 TextLen(const u8 *text, s16 numChars);

#endif /* GAME_TEXT_H */
