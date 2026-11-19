#include "game/game.h"

typedef struct SoundState SoundState;

void ZOID_ShowPoly_bg(tPoly *poly)
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
        tRGBA backColor = { { 0x00, 0x00, 0xC0, 0xFF } };

        bg_texture = MBOX_FindTexture_WORK("BACKGROUND_SFX", &texIdx, 0, gNumModels[0] - 1, 1);
        polyVerts[0][0] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].maxX;
        polyVerts[0][1] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].maxY;
        polyVerts[0][2] = (f32)bgDist;
        polyVerts[1][0] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].minX;
        polyVerts[1][1] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].maxY;
        polyVerts[1][2] = (f32)bgDist;
        polyVerts[2][0] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].minX;
        polyVerts[2][1] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].minY;
        polyVerts[2][2] = (f32)bgDist;
        polyVerts[3][0] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].maxX;
        polyVerts[3][1] = (f32)bgDist * gBackRects[gNumWindows - 1].slot[polyIdx].minY;
        polyVerts[3][2] = (f32)bgDist;
        backPolys[polyIdx] = ZOID_NewPoly(4, polyVerts, texIdx, &backColor,
                                            gBackCamera[gNumWindows - 1][polyIdx] | 0x1210, 1);
        ZOID_ShowPoly_bg(backPolys[polyIdx]);
        if (offsetsValid[gNumWindows - 1] == 1) {
            tPoly *backPoly;
            s32 vertIdx;

            backPoly = backPolys[polyIdx];
            for (vertIdx = 0; vertIdx < backPoly->nverts; vertIdx++) {
                backPoly->verts[vertIdx].tc[0] = gBackRects[gNumWindows - 1].slot[polyIdx].offX[vertIdx];
                backPoly->verts[vertIdx].tc[1] = gBackRects[gNumWindows - 1].slot[polyIdx].offY[vertIdx];
            }
        }
    }
    if (gGameState & GS_TRACK) {
        backBlit = NewMultiBlit(0, 0, trackBack, 2);
    } else {
        backBlit = NewMultiBlit(0, 0, frontBack, 2);
    }
}
