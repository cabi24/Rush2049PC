/* The controller system (MAT3 controlsys.c): from the pads' state ControllerUpdate reads into gControllerData,
 * ControlIRQ (on the model thread, each retrace) keeps the held buttons, the buttons newly pressed since the game
 * last looked, and the sticks with a calibration that grows with what the stick reaches; UpdateControls (on the
 * game thread, each frame) takes them as the frame's held, pressed and auto-repeated buttons and sticks
 * (gContLevels, gContEdges, gAutoEdges, gContStick) and their union over the active controllers (gActiveLevels
 * ...). The two meet under controlsys_lock_queue. Which controllers count as active, and which turn the stick into
 * d-pad presses, is set here too. */
#include "game/game.h"

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

/* The buttons held at the last ControlIRQ, one word a controller: MAT3's volatile unsigned int[4]. include/game
 * declares the 4 bytes the data loader gives its symbol (the 16 are free on the N64 as on the PC: scissorul follows). */
VIEW(volatile s32[4], gNewLevels);
#define gNewLevels gNewLevels_view

/* The scheduler's retrace count (gScheduler.retraceCount), the controls' clock. */
#define gTick gScheduler.retraceCount

/* The stick's calibration when a controller is plugged in or its reset button pressed. */
#define STICK_RANGE_DEFAULT 70
/* How far the raw stick must go to count as a d-pad press (gStickEdges). */
#define STICK_EDGE 56
/* The game's button bits UpdateControls and ControlIRQ use (n64_button_map's order). */
#define BUTTON_STICK_RESET 0x40000  /* the last mapped bit: by its use, the pad's reset flag (L+R+START recentres) */

/* Makes the frame's input state from what ControlIRQ gathered: the sticks scaled to -1..1, the buttons held and
 * pressed, and the auto-repeated presses (a held button repeats every 0.2 s; button 0 never does), each controller's
 * and their union over the active controllers; notes the time of the last key (gLastKeyTime). While gIgnoreCount
 * is set (counted down once a 30 Hz frame), every input reads as nothing. */
void UpdateControls(void)
{
    s32 i;
    s32 b;
    s32 raw;
    u32 held;
    u32 mask;
    s32 active;
    f32 x;
    f32 y;

    osRecvMesg(&controlsys_lock_queue, NULL, 1);
    if (gResetLastKeyTime != 0) {
        gResetLastKeyTime = 0;
        gLastKeyTime = gTick;
    }
    gActiveEdges = gActiveAutoEdges = gActiveLevels = 0;
    if (gIgnoreCount != 0) {
        PC_EACH30(gIgnoreCount--;)
        for (i = 0; i < 4; i++) {
            gNewEdges[i] = 0;
            gContStick[i][1] = 0.0f;
            gContStick[i][0] = 0.0f;
            gAutoEdges[i] = 0;
            gContLevels[i] = 0;
            gContEdges[i] = 0;
        }
        osSendMesg(&controlsys_lock_queue, NULL, 0);
        return;
    }
    for (i = 0; i < 4; i++) {
        raw = gNewStick[i][0];
        x = PotRange(1.0f, gContStickRange[i][0], raw);
        raw = gNewStick[i][1];
        y = PotRange(1.0f, gContStickRange[i][1], raw);
        gContStick[i][0] = x;
        gContLevels[i] = gNewLevels[i];
        held = gContLevels[i];
        gContStick[i][1] = y;
        gContEdges[i] = gNewEdges[i];
        gNewEdges[i] = 0;
        gAutoEdges[i] = 0;
        if (held != 0) {
            gLastKeyTime = gTick;
        }
        active = gActiveControllers[i];
        if (active) {
            gActiveEdges |= gContEdges[i];
            gActiveLevels |= held;
        }
        for (b = 0; b < 32; b++) {
            if (b == 0) {
                continue;
            }
            mask = 1 << b;
            if (held & mask) {
                if (gAutoEdgeTimes[i][b] == 0 ||
                    (u32) (s32) (0.2f * gRetraceRate) < gTick - gAutoEdgeTimes[i][b]) {
                    gAutoEdgeTimes[i][b] = gTick;
                    gAutoEdges[i] |= mask;
                }
            } else {
                gAutoEdgeTimes[i][b] = 0;
            }
        }
        if (active) {
            gActiveAutoEdges |= gAutoEdges[i];
        }
    }
    osSendMesg(&controlsys_lock_queue, NULL, 0);
}

/* A raw stick value scaled by the calibration calib to -range..range (MAT3 _PotRange, its arguments reordered). */
f32 PotRange(f32 range, f32 calib, s32 raw)
{
    f32 v;

    v = ((f32) raw * range) / calib;
    if (v < -range) {
        v = -range;
    } else if (range < v) {
        v = range;
    }
    return v;
}

/* Takes the four controllers' state (gControllerData, from ControllerUpdate) under the controls' lock (starting the
 * controller system first if need be). A detached controller has no buttons, its stick at 0 and the default
 * calibration. An attached one gives its buttons and stick; where the stick drives the d-pad (gStickEdges, the
 * front end's menus) a stick pushed far enough adds the d-pad bit; the stick reset button resets the calibration,
 * and a stick beyond its calibration widens it (up to 127). The newly pressed buttons are added to gNewEdges, which
 * UpdateControls empties; gRealLevels, gRealEdges and gRealStick are this retrace's own, for the car's controls
 * (UpdateModelControls). MAT3 also keeps a second stick. */
void ControlIRQ(void)
{
    s32 i;
    u32 buttons;
    u32 pressed;
    s32 rawX;
    s32 rawY;

    if (gInitControls == 0) {
        InitControlSys();
    }
    osRecvMesg(&controlsys_lock_queue, NULL, 1);
    for (i = 0; i < 4; i++) {
        buttons = 0;
        if (gControllerData[i].attached == 0) {
            gNewStick[i][0] = 0;
            gNewStick[i][1] = 0;
            gContStickRange[i][0] = STICK_RANGE_DEFAULT;
            gContStickRange[i][1] = STICK_RANGE_DEFAULT;
        } else {
            buttons = gControllerData[i].buttons;
            gNewStick[i][0] = gControllerData[i].stick[0];
            gNewStick[i][1] = gControllerData[i].stick[1];
            if (gStickEdges[i]) {
                if (gNewStick[i][0] < -STICK_EDGE) {
                    buttons |= BUTTON_LEFT;
                }
                if (gNewStick[i][0] > STICK_EDGE) {
                    buttons |= BUTTON_RIGHT;
                }
                if (gNewStick[i][1] < -STICK_EDGE) {
                    buttons |= BUTTON_DOWN;
                }
                if (gNewStick[i][1] > STICK_EDGE) {
                    buttons |= BUTTON_UP;
                }
            }
            if (buttons & BUTTON_STICK_RESET) {
                gContStickRange[i][0] = STICK_RANGE_DEFAULT;
                gContStickRange[i][1] = STICK_RANGE_DEFAULT;
            }
            if ((gContStickRange[i][0] < gNewStick[i][0] || gNewStick[i][0] < -gContStickRange[i][0]) &&
                gContStickRange[i][0] < 127) {
                gContStickRange[i][0] += 1;
            }
            if ((gContStickRange[i][1] < gNewStick[i][1] || gNewStick[i][1] < -gContStickRange[i][1]) &&
                gContStickRange[i][1] < 127) {
                gContStickRange[i][1] += 1;
            }
        }
        pressed = gNewLevels[i] ^ buttons;
        pressed = pressed & buttons;
        gNewEdges[i] |= pressed;
        gNewLevels[i] = buttons;
        gRealEdges[i] = pressed;
        gRealLevels[i] = buttons;
        rawX = gNewStick[i][0];
        gRealStick[i][0] = PotRange(1.0f, gContStickRange[i][0], rawX);
        rawY = gNewStick[i][1];
        gRealStick[i][1] = PotRange(1.0f, gContStickRange[i][1], rawY);
    }
    osSendMesg(&controlsys_lock_queue, NULL, 0);
}

/* Whether controller player (-1: all four) turns its stick into d-pad presses. */
void SetControllerStickEdges(s32 player, s32 value)
{
    s32 i;

    if (player == -1) {
        for (i = 0; i < 4; i++) {
            gStickEdges[i] = value;
        }
    } else if (player < 4) {
        gStickEdges[player] = value;
    }
}

/* Whether controller player (-1: all four) counts in the active controllers' union (gActiveLevels ...). */
void SetControllerActive(s32 player, s32 value)
{
    s32 i;

    if (player == -1) {
        for (i = 0; i < 4; i++) {
            gActiveControllers[i] = value;
        }
    } else if (player < 4) {
        gActiveControllers[player] = value;
    }
}
