/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * createBG (historical label) = GC createBG@front.c (MAT3 front.c createBG, GC 0x80013C00, size 0x220):
 * builds the four scrolling menu-background polys.
 * : D_801391E4 (only this function references it) is a function-local static (N64 BG_DIST source, an int 1025
 * converted to float; GC #define BG_DIST 16777000.0f). Unit: 51 of 183 words differ (best: 65). Residual: as1
 * order of the static store vs `sw ra`, and the ugen temp ring two registers behind retail from the gBackCamera load
 * on. NOT a match.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef union {
    u8 c[4];
    u32 rgba;
} Color;

typedef struct {
    f32 minX;            /* 0x00 */
    f32 maxX;            /* 0x04 */
    f32 minY;            /* 0x08 */
    f32 maxY;            /* 0x0C */
    f32 delX;            /* 0x10 */
    f32 delY;            /* 0x14 */
    f32 offX[4];         /* 0x18 */
    f32 offY[4];         /* 0x28 */
} BackRect; /* 0x38 */

typedef struct {
    s16 tc[2];
    u8 pad4[16];
} PolyVert; /* 0x14 */

typedef struct {
    s16 nverts;          /* 0x00 */
    u16 flags;           /* 0x02 */
    u8 pad4[16];
    PolyVert verts[4];   /* 0x14 */
} Poly;

typedef struct SoundClearRecord SoundClearRecord;
typedef struct SoundState SoundState;
/* gcc rejects arrays of an incomplete type, which IDO accepts. NewMultiBlit's descriptor (MultiBlit): its first
 * word is the texture name, a pointer, so the data loader gives the two tables the PC layout (pc_prelude.h). */
struct SoundClearRecord { struct pc_multiblit m; };

extern BackRect gBackRects[4][4];        /* gBackRects */
extern s32 gBackCamera[4][4];             /* gBackCamera */
extern Poly *backPolys[4];              /* backPolys */
extern void *bg_texture;                 /* background texture (N64) */
extern s32 offsetsValid[4];                /* offsetsValid */
extern SoundState *backBlit;
extern SoundClearRecord frontBack[];    /* frontBack */
extern SoundClearRecord trackBack[];    /* trackBack */
extern s16 gNumWindows;
extern volatile u8 gNumModels;
extern u32 gGameState;

void *MBOX_FindTexture_WORK(char *name, u16 *index, s8 firstModel, s8 lastModel, s32 flag);   /* MBOX_FindTexture */
Poly *ZOID_NewPoly(s32 nverts, f32 (*xyz)[3], u16 texidx, Color *color, u16 flags, s32 useTidx);
extern SoundState *NewMultiBlit(s16 ulx, s16 uly, SoundClearRecord *mblit, s16 nblits);

void ZOID_ShowPoly_bg(Poly *poly)
{
    poly->flags &= ~0x8000;
}

void createBG(void)
{
    static s32 D_801391E4;
    s32 polyIdx;
    f32 polyVerts[4][3];
    u16 texIdx;

    D_801391E4 = 1025;
    for (polyIdx = 0; polyIdx < 4; polyIdx++) {
        Color backColor = { { 0x00, 0x00, 0xC0, 0xFF } };

        bg_texture = MBOX_FindTexture_WORK("BACKGROUND_SFX", &texIdx, 0, gNumModels - 1, 1);
        polyVerts[0][0] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].maxX;
        polyVerts[0][1] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].maxY;
        polyVerts[0][2] = (f32)D_801391E4;
        polyVerts[1][0] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].minX;
        polyVerts[1][1] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].maxY;
        polyVerts[1][2] = (f32)D_801391E4;
        polyVerts[2][0] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].minX;
        polyVerts[2][1] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].minY;
        polyVerts[2][2] = (f32)D_801391E4;
        polyVerts[3][0] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].maxX;
        polyVerts[3][1] = (f32)D_801391E4 * gBackRects[gNumWindows - 1][polyIdx].minY;
        polyVerts[3][2] = (f32)D_801391E4;
        backPolys[polyIdx] = ZOID_NewPoly(4, polyVerts, texIdx, &backColor,
                                            gBackCamera[gNumWindows - 1][polyIdx] | 0x1210, 1);
        ZOID_ShowPoly_bg(backPolys[polyIdx]);
        if (offsetsValid[gNumWindows - 1] == 1) {
            Poly *backPoly;
            s32 vertIdx;

            backPoly = backPolys[polyIdx];
            for (vertIdx = 0; vertIdx < backPoly->nverts; vertIdx++) {
                backPoly->verts[vertIdx].tc[0] = gBackRects[gNumWindows - 1][polyIdx].offX[vertIdx];
                backPoly->verts[vertIdx].tc[1] = gBackRects[gNumWindows - 1][polyIdx].offY[vertIdx];
            }
        }
    }
    if (gGameState & 0x80) {
        backBlit = NewMultiBlit(0, 0, trackBack, 2);
    } else {
        backBlit = NewMultiBlit(0, 0, frontBack, 2);
    }
}
