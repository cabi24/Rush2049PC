#include "game/game.h"

typedef union Color {
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

typedef struct Poly {
    s16 nverts;          /* 0x00 */
    u16 flags;           /* 0x02 */
    u8 pad4[16];
    PolyVert verts[4];   /* 0x14 */
} Poly;

typedef struct SoundState SoundState;

VIEW(BackRect[4][4], gBackRects);
#define gBackRects gBackRects_view
VIEW(s32[4][4], gBackCamera);
#define gBackCamera gBackCamera_view
VIEW(Poly *[4], backPolys);
#define backPolys backPolys_view
VIEW(void *, bg_texture);
#define bg_texture bg_texture_view
VIEW(s32[4], offsetsValid);
#define offsetsValid offsetsValid_view
void *MBOX_FindTexture_WORK(char *name, u16 *index, s8 firstModel, s8 lastModel, s32 flag);   /* MBOX_FindTexture */
Poly *ZOID_NewPoly(s32 nverts, f32 (*xyz)[3], u16 texidx, Color *color, u16 flags, s32 useTidx);
extern SoundState *NewMultiBlit(s16 ulx, s16 uly, SoundClearRecord *mblit, s16 nblits);

void ZOID_ShowPoly_bg(Poly *poly)
{
    poly->flags &= ~0x8000;
}

void createBG(void)
{
    static s32 bgDist;
    s32 polyIdx;
    f32 polyVerts[4][3];
    u16 texIdx;

    bgDist = 1025;
    for (polyIdx = 0; polyIdx < 4; polyIdx++) {
        Color backColor = { { 0x00, 0x00, 0xC0, 0xFF } };

        bg_texture = MBOX_FindTexture_WORK("BACKGROUND_SFX", &texIdx, 0, gNumModels[0] - 1, 1);
        polyVerts[0][0] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].maxX;
        polyVerts[0][1] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].maxY;
        polyVerts[0][2] = (f32)bgDist;
        polyVerts[1][0] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].minX;
        polyVerts[1][1] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].maxY;
        polyVerts[1][2] = (f32)bgDist;
        polyVerts[2][0] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].minX;
        polyVerts[2][1] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].minY;
        polyVerts[2][2] = (f32)bgDist;
        polyVerts[3][0] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].maxX;
        polyVerts[3][1] = (f32)bgDist * gBackRects[gNumWindows - 1][polyIdx].minY;
        polyVerts[3][2] = (f32)bgDist;
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
