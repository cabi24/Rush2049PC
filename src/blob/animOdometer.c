/* animOdometer: the odometer's digits, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define HW (blt->width / 2)

/* Digit animID & 0xF (0 hundreds .. 3 tenths of a mile, or of a kilometre with kOptMetric) of the odometer of the
 * player in animID's high nibble, a strip of digits scrolled to the car's distance: the tenths roll smoothly, each
 * higher digit rolls when all those below it read 9. Hidden (and its animation stopped) past the windows or on the
 * obstacle course; hidden when the HUD hides it or the car finished. Placed at odometerPos. Returns 1. (The hide
 * flag, 0 or 1, is read as a signed byte, as the original reads it here.) */
s32 animOdometer(tBlit *blt) {
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

    slot = (blt->animID & 0xF0) >> 4;
    digit = blt->animID & 0xF;
    frac = 0.0f;
    dist = PC_CAR_MILEAGE(slot);
    if (gOptions[kOptMetric]) {
        dist = dist / 0.6f;
    }
    if (!(slot < gNumWindows) || gRace == kObstacle) {
        blt->animFunc = 0;
        if (blt->hide != 1) {
            blt->hide = 1;
            UpdateBlit(blt);
        }
        return 1;
    }
    hide = gShowOdometer == 0 || PC_CAR_PLACE_LOCKED(pc_models[slot].slot) == 1;
    if (hide != (s8)blt->hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    hide = (s8)blt->hide;
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
        blt->x = odometerPos[gNumWindows - 1][slot].x - HW * 2;
        blt->y = odometerPos[gNumWindows - 1][slot].y;
        blt->left = 0;
        blt->top = ((s32) (dist / 528000.0f) % 10) * HW;
        if ((s32) (dist / 52800.0f) % 10 == 9 && (s32) (dist / 5280.0f) % 10 == 9 && itenths % 10 == 9) {
            blt->top = blt->top + frac * HW;
        }
        break;
    case 1:
        blt->x = odometerPos[gNumWindows - 1][slot].x - HW;
        blt->y = odometerPos[gNumWindows - 1][slot].y;
        blt->left = 0;
        blt->top = ((s32) (dist / 52800.0f) % 10) * HW;
        if ((s32) (dist / 5280.0f) % 10 == 9 && itenths % 10 == 9) {
            blt->top = blt->top + frac * HW;
        }
        break;
    case 2:
        blt->x = odometerPos[gNumWindows - 1][slot].x;
        blt->y = odometerPos[gNumWindows - 1][slot].y;
        blt->left = 0;
        blt->top = ((s32) (dist / 5280.0f) % 10) * HW;
        if (itenths % 10 == 9) {
            blt->top = blt->top + frac * HW;
        }
        break;
    case 3:
        blt->x = odometerPos[gNumWindows - 1][slot].x + HW;
        blt->y = odometerPos[gNumWindows - 1][slot].y;
        blt->left = HW;
        blt->top = HW * (itenths % 10 + tenths - itenths);
        break;
    }
    blt->right = blt->left + HW - 1;
    blt->bot = blt->top + HW - 1;
    UpdateBlit(blt);
    return 1;
}
