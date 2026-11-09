/* ShowCountdown: the countdown's numbers in front of each window's camera (MAT3 countdown.c). */
#include "game/game.h"

/* A 3D polygon of the scene (ZOID_NewPoly): the fields this file needs. */
typedef struct Poly {
    s16 active;
    u16 flags;
} Poly;
VIEW(tCamera[], gCamera);
#define gCamera gCamera_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

s32 sprintf(char *, const char *, ...);

/* With `active`, shows the countdown's number (texture CNTDWN3, 2, 1) on a square in front of each window's camera,
 * swinging in from the distance each second, and says the number (sounds 75, 76, 77) as it changes; without it,
 * removes the squares. */
void ShowCountdown(s32 active)
{
    f32 xyz[4][3];
    f32 dist;
    s32 i;
    s32 winidx;
    char str[20];
    static Poly *poly[12];
    static u16 polyTidx;
    static u16 lastCountDown;
    static u8 polyColor[4] = { 0xFF, 0xFF, 0xFF, 0xC0 };
    static f32 polyXYZ[4][3] = {
        { 1.0f, 1.0f, 6.0f }, { -1.0f, 1.0f, 6.0f }, { -1.0f, -1.0f, 6.0f }, { 1.0f, -1.0f, 6.0f }
    };

    for (winidx = 0; winidx < gNumWindows; winidx++) {
        if (active) {
            dist = GetCountdownTimeRemaining() - 0.001f;
            dist = (dist - (s32)dist) * 12.0f;
            if (dist < 5.0f) {
                dist = 5.0f;
            }
            for (i = 0; i < 4; i++) {
                polyXYZ[i][2] = dist;
                U64WorldVector(polyXYZ[i], xyz[i], gCamera[winidx].n64.uvs);
                xyz[i][0] += gCamera[winidx].mat.pos[0];
                xyz[i][1] += gCamera[winidx].mat.pos[1];
                xyz[i][2] += gCamera[winidx].mat.pos[2];
            }
            if (poly[winidx] == 0) {
                MBOX_FindTexture_WORK("CNTDWN3", &polyTidx, 0, gNumModels - 1, 1);
                poly[winidx] = ZOID_NewPoly(4, xyz[0], polyTidx, polyColor, (1 << winidx) | 0x8000 | 0x200 | 0x80 | 0x40, 1);
                lastCountDown = 4;
            } else if (lastCountDown > gCountDown) {
                if (winidx == gNumWindows - 1) {
                    sprintf(str, "CNTDWN%d", gCountDown);
                    MBOX_FindTexture_WORK(str, &polyTidx, 0, gNumModels - 1, 1);
                    lastCountDown = gCountDown;
                    if (gCountDown == 3) {
                        PlayGameSound(75, 0, 1, 0);
                    } else if (gCountDown == 2) {
                        PlayGameSound(76, 0, 1, 0);
                    } else {
                        PlayGameSound(77, 0, 1, 0);
                    }
                }
                ZOID_UpdatePoly(poly[winidx], 4, xyz[0], polyTidx, polyColor, 0, 1);
                ZOID_ShowPoly(poly[winidx]);
            } else {
                ZOID_UpdatePoly(poly[winidx], 4, xyz[0], polyTidx, polyColor, 0, 1);
            }
        } else if (poly[winidx] != 0) {
            ZOID_RemovePoly(poly[winidx]);
            poly[winidx] = 0;
            gCountDown = -1;
        }
    }
}
