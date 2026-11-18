/* include/game/input.h: controllers and the Rumble Pak */
#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "game/types.h"

/* The game's buttons, as gActiveEdges, gActiveLevels, gActiveAutoEdges and each pad's edge and level words hold
 * them. ControllerUpdate maps the pad's buttons into these bits through the cartridge's n64_button_map; ControlIRQ
 * adds the stick as the d-pad for the menus. Bit 0 is Start: the title screen starts on it and name entry ends on it,
 * where A (bit 1) takes a letter. */
#define BUTTON_START 0x1
#define BUTTON_A 0x2
#define BUTTON_B 0x4
#define BUTTON_UP 0x400
#define BUTTON_DOWN 0x800
#define BUTTON_LEFT 0x1000
#define BUTTON_RIGHT 0x2000
#define BUTTON_ACCEPT (BUTTON_START | BUTTON_A)   /* what every menu takes as "go on" */

/* A controller as ControllerUpdate reads it, for ControlIRQ: gControllerData[4], one per port (MAT3 tPadInfo, which
 * also has the buttons the pad offers). Online play's state hash takes these 64 bytes as they are (the curated
 * "pads" region, docs/port/LOCKSTEP.md 5), and both PCs compare it: keep the layout. */
typedef struct ControllerState {
    s8 attached;   /* plugged in */
    u8 unused1[3];
    u32 buttons;   /* held, in the game's button bits (n64_button_map) */
    s8 stick[2];   /* x, y, about -80..80 */
    s8 stick2[2];  /* a second stick: the N64 has none (0) */
    u8 analog[2];  /* analog triggers, 0-255 (the control values CONTROL_VALUE_ANALOG_1, _2): the N64 has none (0) */
    u8 unusedE[2];
} ControllerState;

/* A player's input record (MAT3 tPadState): gPlayers[4], one per player, the player's car and controller and that
 * controller's input as the game reads it. Each frame UpdateRushControls copies the controller's buttons and stick
 * here (FindPlayerNum when a controller first joins); UpdateModelControls turns them into the car's steering,
 * throttle, brake and gear through the player's control layout (controls[], from gNewControls by SetControls). A
 * human car's record (its `input`, pc_cars) points here; a drone has none. 80 bytes. */
typedef struct tPadState {
    u8 slot;              /* the car the player drives: its slot in gGameCar[] and model[] */
    u8 controller;        /* the player's controller port 0-3, CONTROLLER_NONE (5) for none yet */
    u32 edges;            /* the buttons newly pressed this frame (the BUTTON_ bits) */
    u32 levels;           /* the buttons held */
    u32 autoEdges;        /* pressed, and repeated while held (UpdateControls) */
    f32 analog[2];        /* the stick, x and y, -1..1 */
    u32 controls[10];     /* each control's buttons (CONTROL_GAS ...): gControlTable[] of the player's layout, or a
                           * CONTROL_VALUE_ for a stick or a trigger */
    u16 numCrashes;       /* the car's crashes this race (the stunt mode's deaths, the records' totals) */
    u8 trans;             /* the transmission chosen on the car screen, bit 0 set for a manual gearbox (init_cars
                           * gives it to the car's model) */
    s8 unused;            /* MAT3's unused */
    s32 listener;         /* the player's sound listener: its number in gListeners, -1 for none */
    tProfile **save;      /* the player's profile (a handle): a pak's, or the controller's own (gJustPlayers) */
} tPadState;

/* The controls of a layout, by their index in gNewControls[controller][] and tPadState.controls[] (UpdateModelControls,
 * the battle's shots, the camera, the horn). */
#define CONTROL_GAS 0
#define CONTROL_BRAKE 1
#define CONTROL_STEER 2         /* how the stick steers (CONTROL_VALUE_STICK_DEAD or not) */
#define CONTROL_SHIFT_UP 3
#define CONTROL_SHIFT_DOWN 4
#define CONTROL_REVERSE 5       /* held: reverse gear and full throttle */
#define CONTROL_ABORT 6         /* pressed: abort the car (respawn); in battle, drop the weapon */
#define CONTROL_VIEW 7          /* pressed: the next camera view */
#define CONTROL_HORN 8          /* pressed: the horn */
#define CONTROL_WINGS 9         /* held: the wings; in battle, fire */
/* The values in tPadState.controls[] that aren't button bits (ReadGasAndBrake, ReadSteeringWheel). */
#define CONTROL_VALUE_STICK_Y 0x13      /* the stick's y axis: up the gas, down the brake */
#define CONTROL_VALUE_ANALOG_1 0x15     /* the first analog trigger (none on the N64) */
#define CONTROL_VALUE_ANALOG_2 0x16     /* the second analog trigger (none on the N64) */
#define CONTROL_VALUE_STICK_DEAD 0x19   /* the stick, with a dead zone: steering, or the y axis as above */
/* The controller port of a player who has none. */
#define CONTROLLER_NONE 5

/* Data */
extern u32 gControlTable[EXTENT(26)];
extern s8 gInitControls;
extern u32 gLastKeyTime;
extern s8 gResetLastKeyTime;
extern u8 gIgnoreCount;
extern tRGBA controls_updateButtonColor;
extern u32 n64_button_map[EXTENT(19)];
extern char controls_selected_texture_name[EXTENT(16)];
extern char controls_unselected_texture_name[EXTENT(28)];
extern f32 steer_dead_zone_low;
extern f32 steer_dead_zone_offset;
extern f32 steer_dead_zone_high;
extern f32 steer_change_up;
extern f32 steer_change_down;
extern f32 brake_stick_dead_zone;
extern s8 gControlsSet[EXTENT(8)];
extern s8 gNewControls[4][10];   /* each controller's control layout: the index in gControlTable of each control's buttons */
extern s8 gDefaultControls[10];   /* the default control layout (SetDefaultControls) */
extern s8 gMirrorControls;
extern s8 gNoBrakes;
extern u32 gRealLevels[EXTENT(4)];
extern u32 gRealEdges[EXTENT(4)];
extern f32 gRealStick[EXTENT(4)][2];
extern u32 gAutoEdges[EXTENT(4)];
extern u32 gActiveAutoEdges;
extern OSMesgQueue controlsys_lock_queue;
/* What ControlIRQ (the model thread, each retrace) leaves for UpdateControls (the game thread), under
 * controlsys_lock_queue: each stick's calibration (the raw value that reads as full), the buttons pressed since the
 * last UpdateControls, the buttons held (controlsys's s32[4]: the data loader has 4 bytes for its symbol), the raw
 * sticks. Volatile, as MAT3's controlsys.c declares them. */
extern volatile s8 gContStickRange[EXTENT(4)][2];
extern volatile u32 gNewEdges[EXTENT(4)];
extern s32 gNewLevels;
extern volatile s8 gNewStick[EXTENT(4)][2];
extern s8 gActiveControllers[EXTENT(4)];
extern s8 gStickEdges[EXTENT(4)];
extern u32 gAutoEdgeTimes[EXTENT(4)][32];   /* when each held button last auto-repeated, a retrace count (MAT3: int) */
extern OSMesg controlsys_lock_msg;
extern s32 controls_buttonsJustInited[EXTENT(4)];
extern f32 controls_cursorSpeed[EXTENT(4)];
extern f32 controls_scrollSpeed[EXTENT(4)];
/* The frame's input (UpdateControls): the buttons held, pressed and auto-repeated, and their union over the active
 * controllers; the sticks, -1 to 1 (x, y). */
extern u32 gActiveLevels;
extern u32 gActiveEdges;
extern f32 gContStick[EXTENT(4)][2];
extern u32 gContLevels[EXTENT(4)];
extern u32 gContEdges[EXTENT(4)];
extern volatile ControllerState gControllerData[EXTENT(4)];
extern tPadState gPlayers[EXTENT(4)];   /* the players' input records (InitOptions.c defines them) */
extern s8 gSoftReset;

/* Functions */
s32 CheckAndRepairPlayer(void **h, s32 repair);
void ClearControls(void);
void ClearRushControls(void);
void ControlIRQ(void);
s32 ControllerUpdate(void);
void controls_updateButtons(s8 player);
s32 controls_validControl(s8 player);
s32 controls_validOption(s8 player, s32 item);
void FindPlayerNum(u32 num, s32 force);
s8 GetControllerActive(s32 which);
s8 GetControllerStickEdges(s32 which);
void InitControlSys(void);
void InitRushControls(void);
void loading_effect_none(void);
f32 PotRange(f32 range, f32 calib, s32 raw);
void ReadGasAndBrake(struct CarState *car, tPadState *player);
void SetControllerActive(s32 player, s32 value);
void SetControllerStickEdges(s32 player, s32 value);
void SetControls(s32 resetToDefault);
void SetDefaultControls(s8 *controls);
void UpdateControls(void);
void UpdateModelControls(void);
void UpdateRushControls(void);

#endif /* GAME_INPUT_H */
