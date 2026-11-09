#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */
#define OSThread_s OSThread_sView   /* this file's own OSThread_s: include/game's OSThread_s is another type */
#define __OSThreadContext __OSThreadContextView   /* this file's own __OSThreadContext: include/game's __OSThreadContext is another type */

typedef struct __OSThreadContext {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    f32 fp0, fp2, fp4, fp6, fp8, fp10, fp12, fp14;
    f32 fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;
typedef struct OSThread_s {
    struct OSThread_s *next;
    s32 priority;
    struct OSThread_s **queue;
    struct OSThread_s *tlnext;
    u16 state;
    u16 flags;
    s32 id;
    s32 fp;
    struct __OSThreadprofile_s *thprof;
    __OSThreadContext context;
} OSThread;

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

typedef struct OSScTask_s {
    struct OSScTask_s *next;
    s32 state;
    s32 flags;
    void *framebuffer;
    s32 type;
    u8 pad14[0x38 - 0x14];
    void *unk38;
    s32 *unk3C;
    u8 pad40[0x50 - 0x40];
    OSMesgQueue *msgQueue;
    OSMesg msg;
} OSScTask;
typedef struct OSScClient_s {
    struct OSScClient_s *next;
    OSMesgQueue *msgQueue;
} OSScClient;

VIEW(s32[4], gContLevels);
#define gContLevels gContLevels_view
VIEW(s32[4], gContEdges);
#define gContEdges gContEdges_view
VIEW(s32[4], gAutoEdges);
#define gAutoEdges gAutoEdges_view
typedef struct {
    f32 unk0;
    f32 unk4;
} ContStick;
VIEW(ContStick[4], gContStick);
#define gContStick gContStick_view

VIEW(u8, gControllerData);
#define gControllerData gControllerData_view

VIEW(s32, gScheduler);
#define gScheduler gScheduler_view

VIEW(s32, gRealLevels);
#define gRealLevels gRealLevels_view

VIEW(s32, gRealEdges);
#define gRealEdges gRealEdges_view

VIEW(s32, controlsys_lock_queue);
#define controlsys_lock_queue controlsys_lock_queue_view

VIEW(s8[4][2], gContStickRange);
#define gContStickRange (gContStickRange_view[0][0])

VIEW(s32[4], gNewEdges);
#define gNewEdges gNewEdges_view

VIEW(s32[2], gNewStick);
#define gNewStick gNewStick_view

VIEW(s8[4], gActiveControllers);
#define gActiveControllers gActiveControllers_view

VIEW(s8[4], gStickEdges);
#define gStickEdges gStickEdges_view

VIEW(s32[128], gAutoEdgeTimes);
#define gAutoEdgeTimes gAutoEdgeTimes_view

/* typed views of prelude globals (the prelude's widths are wrong for these) */
#define gResetTick    (*(s8 *) &gResetLastKeyTime)
#define gSkipFrames   (*(u8 *) &gIgnoreCount)
#define gHeld         ((s32 *) &gNewLevels)
#define gPrevPressed  ((s32 *) &gNewEdges)
#define gConnected    gActiveControllers
#define gStickRaw     ((volatile s8 (*)[2]) &gNewStick)
#define gStickCal     ((volatile s8 (*)[2]) &gContStickRange)
#define gRepeatTime   ((u32 (*)[32]) &gAutoEdgeTimes)
#define gStick        gContStick

/* UpdateControls: match. The pad state is read through volatile views
 * (gStickRaw, gStickCal, gPadHeld); `if (b == 0) continue;` in the button loop. No shaping devices. */
typedef struct { u8 pad[636]; volatile s32 tick; } SysBlk;
#define gPadHeld ((volatile u32 *) gContLevels)
#define gTick (((SysBlk *) &gScheduler)->tick)

/* read the pads: sticks, held/pressed masks and auto-repeat per button */
void UpdateControls(void)
{
    s32 i;
    s32 b;
    s32 raw;
    u32 held;
    u32 mask;
    s32 connected;
    f32 x;
    f32 y;

    osRecvMesg((OSMesgQueue *) &controlsys_lock_queue, NULL, 1);
    if (gResetTick != 0) {
        gResetTick = 0;
        gLastKeyTime = gTick;
    }
    gActiveEdges = gActiveAutoEdges = gActiveLevels = 0;
    if (gSkipFrames != 0) {
        PC_EACH30(gSkipFrames--;)
        for (i = 0; i < 4; i++) {
            gPrevPressed[i] = 0;
            gStick[i].unk4 = 0.0f;
            gStick[i].unk0 = 0.0f;
            gAutoEdges[i] = 0;
            gContLevels[i] = 0;
            gContEdges[i] = 0;
        }
        osSendMesg((OSMesgQueue *) &controlsys_lock_queue, NULL, 0);
        return;
    }
    for (i = 0; i < 4; i++) {
        raw = gStickRaw[i][0];
        x = PotRange(1.0f, gStickCal[i][0], raw);
        raw = gStickRaw[i][1];
        y = PotRange(1.0f, gStickCal[i][1], raw);
        gStick[i].unk0 = x;
        gPadHeld[i] = gHeld[i];
        held = gPadHeld[i];
        gStick[i].unk4 = y;
        gContEdges[i] = gPrevPressed[i];
        gPrevPressed[i] = 0;
        gAutoEdges[i] = 0;
        if (held != 0) {
            gLastKeyTime = gTick;
        }
        connected = gConnected[i];
        if (connected) {
            gActiveEdges |= gContEdges[i];
            gActiveLevels |= held;
        }
        for (b = 0; b < 32; b++) {
            if (b == 0) {
                continue;
            }
            mask = 1 << b;
            if (held & mask) {
                if (gRepeatTime[i][b] == 0 ||
                    (u32) (s32) (0.2f * gRetraceRate) < gTick - gRepeatTime[i][b]) {
                    gRepeatTime[i][b] = gTick;
                    gAutoEdges[i] |= mask;
                }
            } else {
                gRepeatTime[i][b] = 0;
            }
        }
        if (connected) {
            gActiveAutoEdges |= gAutoEdges[i];
        }
    }
    osSendMesg((OSMesgQueue *) &controlsys_lock_queue, NULL, 0);
}

/* scale a raw stick value to [-range, range] */
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

/* ControlIRQ (0x800E7134, 676 B): -- read the four controller records into the shared pad
 * state, under the SI lock (queue controlsys_lock_queue). N64-only, no arcade or MAT3 ancestor. Initialises the controller
 * subsystem first if needed (InitControlSys). Per controller: a disconnected pad has its raw stick zeroed and its
 * stick calibration reset to 70; a connected one copies buttons and stick from its record (gControllerData, 16 B,
 * filled by func_800E73D8 from osContGetReadData), turns the stick into d-pad bits when the player's
 * stick-as-dpad flag (gStickEdges, set by player_mode_set) is on, resets the calibration on button bit 0x40000 and
 * grows the per-axis calibration (up to 127) whenever the stick exceeds it. Then buttons (gRealLevels), newly
 * pressed buttons (gRealEdges), the held/pressed accumulators and the scaled stick floats (gRealStick, via
 * PotRange) are updated.
 * Shaping: the shared pad state is read through volatile views (record, raw stick, calibration, held and
 * pressed-accumulator arrays): retail keeps every one of those loads and stores in source order; the stick
 * value passed to PotRange is read into a local first (rawX/rawY: retail loads it before the calibration);
 * pressed is computed in two statements (retail xor/and into one register). */
typedef struct {
    s8 connected;       /* 0x0 */
    u8 pad1[3];
    u32 buttons;        /* 0x4 */
    s8 stick[2];        /* 0x8 */
    u8 padA[6];
} ContRec16;
#define gContRec      ((volatile ContRec16 *) &gControllerData)
#define gHeldV        ((volatile s32 *) &gNewLevels)
#define gPrevPressedV ((volatile s32 *) &gNewEdges)
#define gButtons      ((s32 *) &gRealLevels)
#define gPressed      ((s32 *) &gRealEdges)
#define gStickF       ((f32 (*)[2]) &gRealStick)

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
    osRecvMesg((OSMesgQueue *) &controlsys_lock_queue, NULL, 1);
    for (i = 0; i < 4; i++) {
        buttons = 0;
        if (gContRec[i].connected == 0) {
            gStickRaw[i][0] = 0;
            gStickRaw[i][1] = 0;
            gStickCal[i][0] = 70;
            gStickCal[i][1] = 70;
        } else {
            buttons = gContRec[i].buttons;
            gStickRaw[i][0] = gContRec[i].stick[0];
            gStickRaw[i][1] = gContRec[i].stick[1];
            if (gStickEdges[i]) {
                if (gStickRaw[i][0] < -56) {
                    buttons |= 0x1000;
                }
                if (gStickRaw[i][0] > 56) {
                    buttons |= 0x2000;
                }
                if (gStickRaw[i][1] < -56) {
                    buttons |= 0x800;
                }
                if (gStickRaw[i][1] > 56) {
                    buttons |= 0x400;
                }
            }
            if (buttons & 0x40000) {
                gStickCal[i][0] = 70;
                gStickCal[i][1] = 70;
            }
            if ((gStickCal[i][0] < gStickRaw[i][0] || gStickRaw[i][0] < -gStickCal[i][0]) && gStickCal[i][0] < 127) {
                gStickCal[i][0] += 1;
            }
            if ((gStickCal[i][1] < gStickRaw[i][1] || gStickRaw[i][1] < -gStickCal[i][1]) && gStickCal[i][1] < 127) {
                gStickCal[i][1] += 1;
            }
        }
        pressed = gHeldV[i] ^ buttons;
        pressed = pressed & buttons;
        gPrevPressedV[i] |= pressed;
        gHeldV[i] = buttons;
        gPressed[i] = pressed;
        gButtons[i] = buttons;
        rawX = gStickRaw[i][0];
        gStickF[i][0] = PotRange(1.0f, gStickCal[i][0], rawX);
        rawY = gStickRaw[i][1];
        gStickF[i][1] = PotRange(1.0f, gStickCal[i][1], rawY);
    }
    osSendMesg((OSMesgQueue *) &controlsys_lock_queue, NULL, 0);
}

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

/* stand-in caller: keeps UpdateControls out of line under -O3 */

/* stand-in caller: keeps PotRange out of line under -O3 */

/* stand-in caller: keeps InitControlSys out of line under -O3 */

/* stand-in caller: keeps player_mode_set out of line under -O3 */

/* stand-in caller: keeps SetControllerActive out of line under -O3 */
