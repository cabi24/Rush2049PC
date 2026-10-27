/* port/gfx/gfx_gbi.h: F3DEX2 (2.08) command and mode encodings the interpreter decodes. The game's files carry
 * their own macros; this is the renderer's copy, from the SDK's documented encodings. */
#ifndef GFX_GBI_H
#define GFX_GBI_H

/* RSP commands */
#define G_NOOP 0x00
#define G_VTX 0x01
#define G_MODIFYVTX 0x02
#define G_CULLDL 0x03
#define G_BRANCH_Z 0x04
#define G_TRI1 0x05
#define G_TRI2 0x06
#define G_QUAD 0x07
#define G_LINE3D 0x08
#define G_SPECIAL_3 0xD3
#define G_SPECIAL_2 0xD4
#define G_SPECIAL_1 0xD5
#define G_DMA_IO 0xD6
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

/* RDP commands (passed through by the RSP) */
#define G_TEXRECT 0xE4
#define G_TEXRECTFLIP 0xE5
#define G_RDPLOADSYNC 0xE6
#define G_RDPPIPESYNC 0xE7
#define G_RDPTILESYNC 0xE8
#define G_RDPFULLSYNC 0xE9
#define G_SETKEYGB 0xEA
#define G_SETKEYR 0xEB
#define G_SETCONVERT 0xEC
#define G_SETSCISSOR 0xED
#define G_SETPRIMDEPTH 0xEE
#define G_RDPSETOTHERMODE 0xEF
#define G_LOADTLUT 0xF0
#define G_RDPHALF_2 0xF1
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

/* G_MTX parameters (the command stores params ^ G_MTX_PUSH) */
#define G_MTX_PUSH 0x01
#define G_MTX_LOAD 0x02
#define G_MTX_PROJECTION 0x04

/* G_MOVEWORD indices */
#define G_MW_MATRIX 0x00
#define G_MW_NUMLIGHT 0x02
#define G_MW_CLIP 0x04
#define G_MW_SEGMENT 0x06
#define G_MW_FOG 0x08
#define G_MW_LIGHTCOL 0x0A
#define G_MW_FORCEMTX 0x0C
#define G_MW_PERSPNORM 0x0E

/* G_MOVEMEM indices */
#define G_MV_MMTX 2
#define G_MV_PMTX 6
#define G_MV_VIEWPORT 8
#define G_MV_LIGHT 10
#define G_MV_POINT 12
#define G_MV_MATRIX 14

/* G_MODIFYVTX targets */
#define G_MWO_POINT_RGBA 0x10
#define G_MWO_POINT_ST 0x14
#define G_MWO_POINT_XYSCREEN 0x18
#define G_MWO_POINT_ZSCREEN 0x1C

/* Geometry mode (F3DEX2 bit layout) */
#define G_ZBUFFER 0x00000001
#define G_SHADE 0x00000004
#define G_CULL_FRONT 0x00000200
#define G_CULL_BACK 0x00000400
#define G_CULL_BOTH 0x00000600
#define G_FOG 0x00010000
#define G_LIGHTING 0x00020000
#define G_TEXTURE_GEN 0x00040000
#define G_TEXTURE_GEN_LINEAR 0x00080000
#define G_LOD 0x00100000
#define G_SHADING_SMOOTH 0x00200000
#define G_CLIPPING 0x00800000

/* Othermode high word */
#define G_MDSFT_ALPHADITHER 4
#define G_MDSFT_RGBDITHER 6
#define G_MDSFT_COMBKEY 8
#define G_MDSFT_TEXTCONV 9
#define G_MDSFT_TEXTFILT 12
#define G_MDSFT_TEXTLUT 14
#define G_MDSFT_TEXTLOD 16
#define G_MDSFT_TEXTDETAIL 17
#define G_MDSFT_TEXTPERSP 19
#define G_MDSFT_CYCLETYPE 20
#define G_MDSFT_PIPELINE 23
#define G_CYC_1CYCLE 0
#define G_CYC_2CYCLE 1
#define G_CYC_COPY 2
#define G_CYC_FILL 3
#define G_TT_NONE 0
#define G_TT_RGBA16 2
#define G_TT_IA16 3
#define G_TF_POINT 0
#define G_TF_BILERP 2
#define G_TF_AVERAGE 3

/* Othermode low word */
#define G_MDSFT_ALPHACOMPARE 0
#define G_MDSFT_ZSRCSEL 2
#define G_MDSFT_RENDERMODE 3
#define G_AC_NONE 0
#define G_AC_THRESHOLD 1
#define G_AC_DITHER 3
#define G_ZS_PRIM 0x4
#define AA_EN 0x8
#define Z_CMP 0x10
#define Z_UPD 0x20
#define IM_RD 0x40
#define CLR_ON_CVG 0x80
#define CVG_DST_MASK 0x300
#define ZMODE_MASK 0xC00
#define ZMODE_OPA 0x000
#define ZMODE_INTER 0x400
#define ZMODE_XLU 0x800
#define ZMODE_DEC 0xC00
#define CVG_X_ALPHA 0x1000
#define ALPHA_CVG_SEL 0x2000
#define FORCE_BL 0x4000

/* Blender inputs: P and M (color), A (alpha), B (alpha) */
#define G_BL_CLR_IN 0
#define G_BL_CLR_MEM 1
#define G_BL_CLR_BL 2
#define G_BL_CLR_FOG 3
#define G_BL_A_IN 0
#define G_BL_A_FOG 1
#define G_BL_A_SHADE 2
#define G_BL_0 3
#define G_BL_1MA 0
#define G_BL_A_MEM 1
#define G_BL_1 2

/* Image formats and sizes */
#define G_IM_FMT_RGBA 0
#define G_IM_FMT_YUV 1
#define G_IM_FMT_CI 2
#define G_IM_FMT_IA 3
#define G_IM_FMT_I 4
#define G_IM_SIZ_4b 0
#define G_IM_SIZ_8b 1
#define G_IM_SIZ_16b 2
#define G_IM_SIZ_32b 3

/* Tile clamp/mirror bits */
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2

#endif
