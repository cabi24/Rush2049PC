/* include/game/input.h: controllers and the Rumble Pak */
#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "game/types.h"

/* Data */
extern u32 gControlTable[EXTENT(26)];
extern u8 cont_bit_pattern;
extern s8 gInitControls;
extern s32 gLastKeyTime;
extern s8 D_8011195C;
extern u8 D_80111960;
extern f32 controls_gButtonSpace;
extern f32 controls_buttonX;
extern f32 controls_maxY;
extern f32 controls_minY;
extern f32 controls_cursorZ;
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
extern s32 D_80149B30;
extern s32 D_80149B50[EXTENT(2)];
extern s8 gActiveControllers[EXTENT(4)];
extern s8 gActiveControllers_p1;
extern s8 gActiveControllers_p2;
extern s8 gActiveControllers_p3;
extern s8 gStickEdges[EXTENT(4)];
extern s8 gStickEdges_p1;
extern s8 gStickEdges_p2;
extern s8 gStickEdges_p3;
extern s32 D_80149B90[EXTENT(128)];
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
#endif /* GAME_INPUT_H */
