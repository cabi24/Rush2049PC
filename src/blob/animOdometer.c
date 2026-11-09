/* animOdometer: the odometer's digits, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define kObstacle 5   /* gRace (MAT3 enum kNumRaceTypes) */
#define FEET_PER_TENTH_MILE 528.0f

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    void *Name, *Image, *Info;
    u16 TexIndex;
    s16 X;
    s16 Y;
    s16 pad12;
    s16 Width;
    s16 Height;
    u8 pad18[2];
    s8 Hide;
    u8 Init;
    s16 Top;
    s16 Bot;
    s16 Left;
    s16 Right;
    s32 pad24;
    intptr_t animFunc;
    u32 AnimID;
} Blit;

VIEW(s8[], gOptions);
#define gOptions gOptions_view

#define HW (blt->Width / 2)

/* Digit animID & 0xF (0 hundreds .. 3 tenths of a mile, or of a kilometre with kOptMetric) of the odometer of the
 * player in animID's high nibble, a strip of digits scrolled to the car's distance: the tenths roll smoothly, each
 * higher digit rolls when all those below it read 9. Hidden (and its animation stopped) past the windows or on the
 * obstacle course; hidden when the HUD hides it or the car finished. Placed at odometerPos. Returns 1. */
s32 animOdometer(Blit *blt) {
    s32 slot;
    s32 digit;
    s32 unused0;
    s32 itenths;
    s32 hide;
    s32 unused1;
    s32 unused2;
    f32 dist;
    f32 frac;
    f32 tenths;

    slot = (blt->AnimID & 0xF0) >> 4;
    digit = blt->AnimID & 0xF;
    frac = 0.0f;
    dist = PC_CAR_F108(slot);
    if (gOptions[kOptMetric]) {
        dist = dist / 0.6f;
    }
    if (!(slot < gNumWindows) || gRace == kObstacle) {
        blt->animFunc = 0;
        if (blt->Hide != 1) {
            blt->Hide = 1;
            UpdateBlit(blt);
        }
        return 1;
    }
    hide = gShowOdometer == 0 || PC_CAR_FINISHED(PC_MODEL_SLOT(slot)) == 1;
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    hide = blt->Hide;
    if (hide) {
        return 1;
    }
    tenths = dist / FEET_PER_TENTH_MILE;
    itenths = tenths;
    if (itenths % 10 == 9) {
        frac = tenths - itenths;
    }
    switch (digit) {
    case 0:
        blt->X = odometerPos[gNumWindows - 1][slot].x - HW * 2;
        blt->Y = odometerPos[gNumWindows - 1][slot].y;
        blt->Left = 0;
        blt->Top = ((s32) (dist / 528000.0f) % 10) * HW;
        if ((s32) (dist / 52800.0f) % 10 == 9 && (s32) (dist / 5280.0f) % 10 == 9 && itenths % 10 == 9) {
            blt->Top = blt->Top + frac * HW;
        }
        break;
    case 1:
        blt->X = odometerPos[gNumWindows - 1][slot].x - HW;
        blt->Y = odometerPos[gNumWindows - 1][slot].y;
        blt->Left = 0;
        blt->Top = ((s32) (dist / 52800.0f) % 10) * HW;
        if ((s32) (dist / 5280.0f) % 10 == 9 && itenths % 10 == 9) {
            blt->Top = blt->Top + frac * HW;
        }
        break;
    case 2:
        blt->X = odometerPos[gNumWindows - 1][slot].x;
        blt->Y = odometerPos[gNumWindows - 1][slot].y;
        blt->Left = 0;
        blt->Top = ((s32) (dist / 5280.0f) % 10) * HW;
        if (itenths % 10 == 9) {
            blt->Top = blt->Top + frac * HW;
        }
        break;
    case 3:
        blt->X = odometerPos[gNumWindows - 1][slot].x + HW;
        blt->Y = odometerPos[gNumWindows - 1][slot].y;
        blt->Left = HW;
        blt->Top = HW * (itenths % 10 + tenths - itenths);
        break;
    }
    blt->Right = blt->Left + HW - 1;
    blt->Bot = blt->Top + HW - 1;
    UpdateBlit(blt);
    return 1;
}
