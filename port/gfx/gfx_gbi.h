/* port/gfx/gfx_gbi.h: the values the renderer interprets in the commands' records (port/include/pc_gfx_list.h): what
 * GFX_OP_MODIFY_VERTEX sets, the geometry mode's bits, the other modes' fields and the blender's inputs, the image
 * formats, the tiles' clamp and mirror bits. They are the RDP's and the F3DEX2 microcode's, from the SDK's documented
 * encodings; the game's own copy is include/game/gbi.h. */
#ifndef GFX_GBI_H
#define GFX_GBI_H

/* GFX_OP_MODIFY_VERTEX: what it sets */
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
