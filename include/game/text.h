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
extern s8 gFinished;
extern void *gFontHeap;
extern f32 gDepth;
extern f32 gTrans;
extern s8 gBufferInited;
extern MenuDepthPoint menu_text_depth_curve[EXTENT(14)];
extern s8 text_lock_inited;
extern s8 gInited_TBText;
extern ScreenPos gJust;
extern TextColor gFlashColor1;
extern TextColor gFlashColor2;
extern volatile f32 gColorCycleTime;
extern Color8 gFontColors[EXTENT(22)];
extern s8 text_wave_inited;
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
#endif /* GAME_TEXT_H */
