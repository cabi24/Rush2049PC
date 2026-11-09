/* include/game/gbi.h: the display-list encodings the game writes (the F3DEX2 microcode's commands and the RDP's, as
 * libultra's gbi.h names them). The game's C builds N64 display lists (the blitter, the scene draw, the frame's start
 * and end) and reads the ones the track and car files hold; port/gfx interprets them (port/gfx/gfx_gbi.h is the
 * renderer's copy of the same encodings, its othermode values unshifted, as it decodes the fields). The macros that
 * build commands (gDPSetTile, gSPDisplayList ...) are still in the files that use them, each over its own Gfx view. */
#ifndef GAME_GBI_H
#define GAME_GBI_H

/* The commands (the first byte of a command's first word). */
#define G_VTX 0x01
#define G_CULLDL 0x03
#define G_BRANCH_Z 0x04
#define G_TRI1 0x05
#define G_TRI2 0x06
#define G_TEXTURE 0xD7
#define G_POPMTX 0xD8
#define G_GEOMETRYMODE 0xD9
#define G_MTX 0xDA
#define G_MOVEWORD 0xDB
#define G_MOVEMEM 0xDC
#define G_LOAD_UCODE 0xDD
#define G_DL 0xDE
#define G_ENDDL 0xDF
#define G_SPNOOP 0xE0
#define G_RDPHALF_1 0xE1
#define G_SETOTHERMODE_L 0xE2
#define G_SETOTHERMODE_H 0xE3
#define G_RDPLOADSYNC 0xE6
#define G_RDPPIPESYNC 0xE7
#define G_RDPTILESYNC 0xE8
#define G_SETSCISSOR 0xED
#define G_SETPRIMDEPTH 0xEE
#define G_LOADTLUT 0xF0
#define G_SETTILESIZE 0xF2
#define G_LOADBLOCK 0xF3
#define G_LOADTILE 0xF4
#define G_SETTILE 0xF5
#define G_FILLRECT 0xF6
#define G_SETFILLCOLOR 0xF7
#define G_SETFOGCOLOR 0xF8
#define G_SETBLENDCOLOR 0xF9
#define G_SETPRIMCOLOR 0xFA
#define G_SETENVCOLOR 0xFB
#define G_SETCOMBINE 0xFC
#define G_SETTIMG 0xFD
#define G_SETZIMG 0xFE
#define G_SETCIMG 0xFF

/* G_DL: a call (push the return) or a branch. */
#define G_DL_PUSH 0x00
#define G_DL_NOPUSH 0x01

/* G_MTX's parameters. */
#define G_MTX_MODELVIEW 0x00
#define G_MTX_PROJECTION 0x04
#define G_MTX_MUL 0x00
#define G_MTX_LOAD 0x02
#define G_MTX_NOPUSH 0x00
#define G_MTX_PUSH 0x01

/* G_MOVEWORD's indices and offsets, G_MOVEMEM's. */
#define G_MW_NUMLIGHT 0x02
#define G_MW_SEGMENT 0x06
#define G_MW_FOG 0x08
#define G_MW_PERSPNORM 0x0E
#define G_MWO_NUMLIGHT 0x00
#define G_MWO_FOG 0x00
#define G_MV_VIEWPORT 8
#define G_MV_LIGHT 10
#define G_MVO_LOOKATX 0
#define G_MVO_LOOKATY 24

/* The geometry mode's bits (G_GEOMETRYMODE). */
#define G_ZBUFFER 0x00000001
#define G_FOG 0x00010000
#define G_LIGHTING 0x00020000
#define G_TEXTURE_GEN 0x00040000
#define G_ON 1

/* The other modes: each field's shift in its word (G_SETOTHERMODE_H, G_SETOTHERMODE_L), then the fields' values in
 * place. */
#define G_MDSFT_RGBDITHER 6
#define G_MDSFT_TEXTFILT 12
#define G_MDSFT_TEXTLUT 14
#define G_MDSFT_TEXTPERSP 19
#define G_MDSFT_CYCLETYPE 20
#define G_MDSFT_ALPHACOMPARE 0
#define G_MDSFT_ZSRCSEL 2
#define G_MDSFT_RENDERMODE 3
#define G_CD_MAGICSQ (0 << G_MDSFT_RGBDITHER)
#define G_CD_BAYER (1 << G_MDSFT_RGBDITHER)
#define G_CD_NOISE (2 << G_MDSFT_RGBDITHER)
#define G_CD_DISABLE (3 << G_MDSFT_RGBDITHER)
#define G_TF_POINT (0 << G_MDSFT_TEXTFILT)
#define G_TF_AVERAGE (3 << G_MDSFT_TEXTFILT)
#define G_TF_BILERP (2 << G_MDSFT_TEXTFILT)
#define G_TT_NONE (0 << G_MDSFT_TEXTLUT)
#define G_TT_RGBA16 (2 << G_MDSFT_TEXTLUT)
#define G_TP_NONE (0 << G_MDSFT_TEXTPERSP)
#define G_CYC_1CYCLE (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE (1 << G_MDSFT_CYCLETYPE)
#define G_CYC_COPY (2 << G_MDSFT_CYCLETYPE)
#define G_CYC_FILL (3 << G_MDSFT_CYCLETYPE)
#define G_AC_NONE (0 << G_MDSFT_ALPHACOMPARE)
#define G_AC_THRESHOLD (1 << G_MDSFT_ALPHACOMPARE)
#define G_ZS_PIXEL (0 << G_MDSFT_ZSRCSEL)
#define G_ZS_PRIM (1 << G_MDSFT_ZSRCSEL)

/* G_SETSCISSOR's mode. */
#define G_SC_NON_INTERLACE 0

/* The image formats and texel sizes (G_SETTIMG, G_SETTILE), and what loading each size takes: its bytes, the size a
 * block load uses for it, the shift and increment that turn its texels into 16-bit ones. */
#define G_IM_FMT_RGBA 0
#define G_IM_FMT_YUV 1
#define G_IM_FMT_CI 2
#define G_IM_FMT_IA 3
#define G_IM_FMT_I 4
#define G_IM_SIZ_4b 0
#define G_IM_SIZ_8b 1
#define G_IM_SIZ_16b 2
#define G_IM_SIZ_32b 3
#define G_IM_SIZ_8b_BYTES 1
#define G_IM_SIZ_8b_TILE_BYTES G_IM_SIZ_8b_BYTES
#define G_IM_SIZ_8b_LINE_BYTES G_IM_SIZ_8b_BYTES
#define G_IM_SIZ_16b_BYTES 2
#define G_IM_SIZ_16b_TILE_BYTES G_IM_SIZ_16b_BYTES
#define G_IM_SIZ_16b_LINE_BYTES G_IM_SIZ_16b_BYTES
#define G_IM_SIZ_32b_BYTES 4
#define G_IM_SIZ_32b_TILE_BYTES 2
#define G_IM_SIZ_32b_LINE_BYTES 2
#define G_IM_SIZ_4b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_8b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_16b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_32b_LOAD_BLOCK G_IM_SIZ_32b
#define G_IM_SIZ_4b_SHIFT 2
#define G_IM_SIZ_8b_SHIFT 1
#define G_IM_SIZ_16b_SHIFT 0
#define G_IM_SIZ_32b_SHIFT 0
#define G_IM_SIZ_4b_INCR 3
#define G_IM_SIZ_8b_INCR 1
#define G_IM_SIZ_16b_INCR 0
#define G_IM_SIZ_32b_INCR 0
#define G_TEXTURE_IMAGE_FRAC 2

/* The tiles and their clamp and mirror bits (G_SETTILE). */
#define G_TX_RENDERTILE 0
#define G_TX_LOADTILE 7
#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0
#define G_TX_DXT_FRAC 11
#define G_TX_LDBLK_MAX_TXL 2047

#endif /* GAME_GBI_H */
