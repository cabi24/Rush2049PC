/* include/game/input.h: controllers and the Rumble Pak */
#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "game/types.h"

/* Data */
extern u32 gControlTable[EXTENT(26)];
extern s8 gInitControls;
extern u32 gLastKeyTime;
extern s8 gResetLastKeyTime;
extern u8 gIgnoreCount;
extern s32 controls_updateButtonColor;
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
extern s8 gNewControls[EXTENT(4)][10];
extern s32 gDefaultControls;
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
void ReadGasAndBrake(struct CarState *car, struct InputRecord *player);
void SetControllerActive(s32 player, s32 value);
void SetControllerStickEdges(s32 player, s32 value);
void SetControls(s32 resetToDefault);
void SetDefaultControls(s8 *controls);
void UpdateControls(void);
void UpdateModelControls(void);
void UpdateRushControls(void);

#endif /* GAME_INPUT_H */
