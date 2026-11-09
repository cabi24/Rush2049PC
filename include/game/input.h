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

/* Data */
extern u32 gControlTable[EXTENT(26)];
extern s8 gInitControls;
extern s32 gLastKeyTime;
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
extern s32 gAutoEdges[EXTENT(4)];
extern s32 gActiveAutoEdges;
extern OSMesgQueue controlsys_lock_queue;
extern s8 gContStickRange[EXTENT(4)][2];
extern s32 gNewEdges[EXTENT(4)];
extern s32 gNewLevels;
extern s32 gNewStick[EXTENT(2)];
extern s8 gActiveControllers[EXTENT(4)];
extern s8 gStickEdges[EXTENT(4)];
extern s32 gAutoEdgeTimes[EXTENT(128)];
extern OSMesg controlsys_lock_msg;
extern s32 controls_buttonsJustInited[EXTENT(4)];
extern f32 controls_cursorSpeed[EXTENT(4)];
extern f32 controls_scrollSpeed[EXTENT(4)];
extern s32 gActiveLevels;
extern s32 gActiveEdges;
extern MenuDepthPoint gContStick[EXTENT(4)];
extern s32 gContLevels[EXTENT(4)];
extern s32 gContEdges[EXTENT(4)];
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
