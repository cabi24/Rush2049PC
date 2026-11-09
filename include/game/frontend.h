/* include/game/frontend.h: the front end's screens (runtime image A and the game image's option, controls, pause,
 * high-score and game-over screens), the track objects' start and animation functions, and the MusyX glue
 * (docs/code/frontend.md). */
#ifndef GAME_FRONTEND_H
#define GAME_FRONTEND_H

#include "game/types.h"

/* ---- Constants. Names from MAT3 where it has them: the GS_ states are its rush_flags.h's, the k names its
 * rush_types.h enums'. Macros, each spelled as MAT3 spells it, so that a file that defines the same one
 * (resurrect.c: kOneRace ... kBattle, GS_DEMO) agrees with this one. The game states, the buttons, the race types
 * and the pause states are the whole game's: they belong in race.h and input.h once the areas are merged. */

/* gGameState: the game's state, one bit set. setGameState asks for one; updateGameState switches at the top of the
 * next frame (uninitGameState, initGameState). The front-end states are dispatched by updateFrontEnd, the race's by
 * UpdateGame. */
#define GS_BOOT          0x1        /* one-time init, then GS_LICENSE */
#define GS_LICENSE       0x2        /* updateLicenseScreen */
#define GS_TITLE         0x4        /* updateTitleScreen */
#define GS_DEMO          0x8        /* the attract-mode race, every car a drone */
#define GS_START         0x10       /* updateStartScreen */
#define GS_PLAYER        0x20       /* updatePlayerScreen */
#define GS_MODE          0x40       /* updateModeScreen */
#define GS_TRACK         0x80       /* updateTrackScreen */
#define GS_CAR           0x100      /* updateCarScreen */
#define GS_OPTIONS       0x200      /* updateOptionScreen */
#define GS_CONTROLS      0x400      /* updateControlScreen */
#define GS_AUDIO         0x800      /* updateAudioScreen */
#define GS_VIDEO         0x1000     /* updateVideoScreen */
#define GS_RECORDS       0x2000     /* updateRecordScreen */
#define GS_CIRCUIT       0x4000     /* updateCircuitScreen */
#define GS_GAME_INIT     0x40000    /* the race's set-up */
#define GS_PREPLAY       0x80000    /* the track streams in, then GS_COUNTDOWN */
#define GS_GO            0x100000   /* the countdown hit zero */
#define GS_COUNTDOWN     0x200000   /* the start countdown */
#define GS_RACING        0x400000   /* the race proper */
#define GS_GAME_OVER     0x800000   /* updateGameOverScreen */
#define GS_END_GAME      0x1000000  /* the race torn down */
#define GS_HIGH_SCORE    0x2000000  /* updateHighScoreScreen */
#define GS_PAK           0x4000000  /* updatePakScreen: the controller-pak manager */
#define GS_CHEAT         0x8000000  /* updateCheatScreen */
#define GS_WARNING       0x40000000 /* updateWarningScreen */
#define GS_FRONTEND_MASK 0x7C03FFFE /* every state updateFrontEnd handles (the menus, title, demo, pak and warning) */
#define GS_RACE_MASK     0x7C0000   /* the race's set-up through the race: GS_GAME_INIT ... GS_RACING */

/* gSkipToGameState: the screen the front end goes straight on to after a race (the game-over menu's new mode,
 * track, car ...: each screen on the way passes the player through until it reaches it), or SKIP_NONE. */
#define SKIP_NONE        GS_BOOT

/* The game's buttons, as gActiveEdges, gActiveAutoEdges and the per-pad edge words hold them. ControllerUpdate maps
 * the pad's buttons into these bits through the cartridge's n64_button_map; ControlIRQ adds the stick as the d-pad
 * for the menus. Start, A and B by PlayStartABSound, the directions by PlayUDSound, PlayLRSound and ControlIRQ. */
#define BUTTON_START     0x1
#define BUTTON_A         0x2
#define BUTTON_B         0x4
#define BUTTON_UP        0x400
#define BUTTON_DOWN      0x800
#define BUTTON_LEFT      0x1000
#define BUTTON_RIGHT     0x2000
#define BUTTON_ACCEPT    (BUTTON_START | BUTTON_A)  /* what every menu takes as "go on" */

/* gRace: the race type (MAT3 enum_kNumRaceTypes); the mode menu's rows in this order. */
#define kOneRace 0
#define kPractice 1
#define kGhost 2
#define kCircuit 3
#define kStunt 4
#define kObstacle 5
#define kBattle 6
#define kNumRaceTypes 7

/* gPauseState (MAT3 enum_kNumPauseStates): kNoPause while the race runs. */
#define kNoPause 0
#define kPauseMainScreen 1
#define kPauseAudioScreen 2
#define kPauseVideoScreen 3
#define kPauseControlsScreen 4
#define kPauseOptionsScreen 5
#define kPauseRumbleScreen 6
#define kPauseRestart 7
#define kPauseQuit 8

/* The text engine's justification (TB_SetJust(horizontal, vertical); MAT3 enum_kTB_JustBottom) and colours
 * (TB_SetTextColor; MAT3 enum_kTB_FlashText: kTB_FlashText alternates between the two colours TB_FlashTextColors
 * set, the selected row's look). The text engine's: they belong in text.h once the areas are merged. */
#define kTB_JustLeft 0
#define kTB_JustCenter 1
#define kTB_JustRight 2
#define kTB_JustTop 3
#define kTB_JustBottom 4
#define kTB_BlackText 0
#define kTB_WhiteText 1
#define kTB_GrayText 2
#define kTB_MedGryText 3
#define kTB_DrkGryText 4
#define kTB_RedText 5
#define kTB_MagentaText 6
#define kTB_BlueText 7
#define kTB_MedBlueText 8
#define kTB_DrkBlueText 9
#define kTB_CyanText 10
#define kTB_GreenText 11
#define kTB_MedGrnText 12
#define kTB_DrkGrnText 13
#define kTB_YellowText 14
#define kTB_DrkYelText 15
#define kTB_YelBlkText 16
#define kTB_YelRedText 17
#define kTB_RedYelText 18
#define kTB_RedRedText 19
#define kTB_CynCynText 20
#define kTB_CynGrnText 21
#define kTB_FlashText 22

/* The controller-pak manager's state (gState, MAT3 enum_kNumStates). */
#define kStatePickPak 0
#define kStateFileDel 1
#define kStateFileDelVerify 2
#define kStateRumblePak 3           /* the chosen pak is a Rumble Pak: a notice */
#define kStateDamaged 4             /* the chosen pak is damaged: a notice */

/* The player (profile select) screen: each window's state (gPlayerState[], MAT3 enum_kPlayerStateCount). */
#define kPlayerStateSelect 0
#define kPlayerStateWait 1          /* chosen: waiting for the others */
#define kPlayerStateControls 2
#define kPlayerStateCreatePlayer 3
#define kPlayerStateDeletePlayer 4
#define kPlayerStatePakFull 5
#define kPlayerStatePakDamaged 6
#define kPlayerStatePaks 7

/* The high-score screen's state (gHighScoreState, MAT3 enum_kNumHighScoreStates): the pages after a race, in the
 * order they come. */
#define kHighScoreNone 0            /* off: the screen is torn down and the game goes on to the game-over menu */
#define kHighScoreUnlocked 1        /* what the race unlocked, per player */
#define kHighScoreCircuitCode 2     /* the circuit's code */
#define kHighScoreNames 3           /* name entry for the players who made the tables */
#define kHighScoreGhost 4           /* the ghost save menu */
#define kHighScoreScores 5          /* the tables */
/* Its ghost menu's rows (gGhostMenuSelect) and a pak's ghost for this track (gGhostStatus[pak]). */
#define kGhostMenuPak 0             /* left and right pick the pak */
#define kGhostMenuStatus 1          /* the pak's status: never selectable */
#define kGhostMenuSave 2
#define kGhostMenuNoSave 3
#define kGhostMenuCount 4
#define kGhostNone 0                /* no ghost of this track there: save */
#define kGhostMine 1                /* this profile's: ask before overwriting it */
#define kGhostOther 2               /* another profile's: can't save */
#define kGhostNotSame 3

/* The game-over screen (gameover.c): its state (gGameOverState) and its menu's rows (gMainSelect). */
#define kGameOverStateNone 0
#define kGameOverStateMain 1
#define kGameOverStateScore 2
#define kGameOverMainMenuScore 0
#define kGameOverMainMenuRaceAgain 1
#define kGameOverMainMenuNewMode 2
#define kGameOverMainMenuNewTrack 3
#define kGameOverMainMenuNewCar 4
#define kGameOverMainMenuNextRace 5
#define kGameOverMainMenuQuit 6
#define kGameOverMainMenuCount 7

/* Data */
extern s32 build_date_text;
extern s32 build_time_text;
extern s32 gAudioMenuSelect;
extern s32 exitAudioScreen;
extern s32 gLastSong;
extern f32 gFrontTireScale_players[EXTENT(4)][13];
extern f32 gRearTireScale_players[EXTENT(4)][13];
extern s8 car_color_choice_a[EXTENT(13)];
extern s8 car_color_choice_b[EXTENT(13)];
extern s8 car_color_choice_c[EXTENT(13)];
extern char *controls_buttonNames[EXTENT(11)];
extern MenuButton controls_buttonInfo[EXTENT(4)][17];
extern f32 controls_gButtonZoom;
extern f32 controls_gButtonScale;
extern f32 controls_gButtonSpace;
extern f32 controls_buttonX;
extern f32 controls_maxY;
extern f32 controls_minY;
extern f32 controls_buttonZ;
extern f32 controls_cursorZ;
extern struct pc_multiblit buttonsFrontEnd[EXTENT(81)];
extern struct pc_multiblit buttonsPauseSingle[EXTENT(21)];
extern struct pc_multiblit buttonsPauseMulti[EXTENT(41)];
extern s8 controls_inited;
extern void *controls_blit;
extern s32 exitControlScreen;
extern PlayerRect controls_gPlayerRects[EXTENT(4)][4];
extern s8 gControlValid[EXTENT(28)];
extern s32 controls_idMap[EXTENT(27)];
extern s32 controls_idMap_conflict;
extern s32 controls_initButtonColor;
extern SoundClearRecord frontBack[EXTENT(2)];
extern SoundClearRecord trackBack[EXTENT(2)];
extern MotionFrame gBackRects[EXTENT(4)];
extern s32 gBackCamera[EXTENT(4)][4];
extern struct SoundState *backBlit;
extern struct PointList *backPolys[EXTENT(4)];
extern struct Dims *bg_texture;
extern s32 offsetsValid[EXTENT(5)];
extern ColorWord flashColors_at_1;
extern BlitColor controller_dim_color;
extern struct pc_multiblit gameover_multiblit[EXTENT(1)];
extern u8 gameover_titlePos[EXTENT(4)];
extern s8 gameover_inited;
extern struct Voice *gameover_blit;
extern char *gDefaultNames[EXTENT(55)];
extern struct pc_multiblit highscore_mblit[EXTENT(1)];
extern intptr_t highscore_blit;
extern s8 gTable;
extern s32 gLastPlayer[EXTENT(4)];
extern HighScoreRect highscore_gPlayerRects[EXTENT(4)][4];
extern char name_entry_del_label[EXTENT(4)];
extern char name_entry_end_label[EXTENT(4)];
extern char name_entry_space_label[EXTENT(4)];
extern ObscenityList obscenities;
extern char *cStrLanguageNames[EXTENT(6)];
extern MenuButton options_buttonInfo[EXTENT(32)];
extern s32 options_buttonsInited;
extern s32 options_buttonsJustInited;
extern s32 options_validOption[EXTENT(14)];
extern const HudBlitDef options_frontEndBlit;
extern const HudBlitDef options_pauseBlit;
extern s8 options_inited;
extern struct Blit *options_blit;
extern s16 options_gSelectedOption;
extern s32 exitOptionScreen;
extern intptr_t options_gTitlePane;
extern s8 options_gControllerRemoved;
extern TextColor options_buttonColor;
extern s8 pak_recognize_error_seen;
extern BlitColor pause_dim_color;
extern s32 gPauseState;
extern const HudBlitDef DimMultiBlit;
extern const HudBlitDef TitleMultiBlit;
extern const HudBlitDef PauseMultiBlit;
extern const HudBlitDef SureMultiBlit;
extern const HudBlitDef RumbleMultiBlit;
extern u32 coin_gold_glow_color;
extern u32 coin_silver_glow_color;
extern s8 video_panesCreated;
extern s32 exitVideoScreen;
extern s32 gVideoMenuSelect;
extern s32 gVertCenter;
extern s32 gHorzCenter;
extern s8 gTestFrequency;
extern s8 gConfirmFrequency;
extern s8 gYesFrequency;
extern void *gTestPane;
extern void *gConfirmPane;
extern void (*musyx_retrace_callback)();
extern char controller_removed_format[EXTENT(48)];
extern char race_standings_drone_name[EXTENT(8)];
extern char race_standings_time_format[EXTENT(16)];
extern char race_standings_lap_format[EXTENT(16)];
extern char stunt_standings_drone_name[EXTENT(8)];
extern char stunt_standings_points_format[EXTENT(4)];
extern char stunt_standings_best_format[EXTENT(4)];
extern char battle_standings_drone_name[EXTENT(8)];
extern char battle_standings_total_format[EXTENT(4)];
extern char race_score_time_format[EXTENT(16)];
extern char race_score_best_lap_format[EXTENT(16)];
extern char race_score_lap_time_format[EXTENT(16)];
extern char stunt_score_total_format[EXTENT(4)];
extern char stunt_score_best_format[EXTENT(4)];
extern char stunt_score_count_format[EXTENT(4)];
extern char battle_score_total_format[EXTENT(4)];
extern char battle_score_cell_format[EXTENT(608)];
extern char ghost_menu_title_text[EXTENT(12)];
extern char default_score_time_format[EXTENT(16)];
extern char score_time_format[EXTENT(16)];
extern char unlocked_circuit_format[EXTENT(8)];
extern char unlocked_trans_format[EXTENT(8)];
extern char unlocked_engine_format[EXTENT(8)];
extern char unlocked_tires_format[EXTENT(8)];
extern char unlocked_frame_format[EXTENT(1872)];
extern char pause_controller_removed_format[EXTENT(20)];
extern char pause_player_title_format[EXTENT(8)];
extern char coin_glow_object_name[EXTENT(12)];
extern char gold_coin_object_name[EXTENT(16)];
extern char silver_coin_object_name[EXTENT(16)];
extern f32 velrot_gravity[EXTENT(3)];
extern f32 cactus_frame_time;
extern f32 fence_frame_time;
extern CircuitScaleView object_list_scale_view[EXTENT(1)];
extern s32 gGameOverState;
extern s32 gMainSelect;
extern s32 gHighScoreState;
extern intptr_t gThisPlayer[EXTENT(4)];
extern s16 gThisTrack;
extern s8 gStuffUnlocked[EXTENT(5)];
extern u8 gPrevUnlockedTracks[EXTENT(76)];
extern u8 gPrevUnlockedCars[EXTENT(52)];
extern u8 gPrevUnlockedCircuits[EXTENT(16)];
extern u8 gPrevUnlockedTrans[EXTENT(32)];
extern u8 gPrevUnlockedEngines[EXTENT(36)];
extern u8 gPrevUnlockedTires[EXTENT(24)];
extern u8 gPrevUnlockedFrames[EXTENT(24)];
extern char gScoreName[EXTENT(4)][13];
extern u8 circuitCode;
extern u8 highscore_gCircuitCode[EXTENT(32)];
extern s8 gDrawName[EXTENT(4)];
extern char gScoreTimes[EXTENT(5)][10];
extern char *gScoreNames[EXTENT(5)];
extern s16 highscore_gNameSelect[EXTENT(4)];
extern s16 highscore_gNameLen[EXTENT(4)];
extern s32 gGhostMenuSelect;
extern s8 gGhostPak;
extern s8 gGhostMenuConflict;
extern s8 gGhostMenuConfirm;
extern s8 highscore_gYes;
extern s8 gGhostMenuNotify;
extern struct Ghost **gPakGhosts[EXTENT(4)];
extern void *gSavedGhost;
extern s32 gGhostStatus[EXTENT(4)];
extern OSMesgQueue text_lock_queue;
extern s8 *codePane;
extern s8 *namePanes[EXTENT(4)];
extern Gfx *blit_dl;
extern s8 *scorePanes[EXTENT(2)];
extern s8 *ghostPanes[EXTENT(3)];
extern s16 gOptionMenuSize;
extern s32 pauseMenuSelect;
extern s16 gMenuCount;
extern s16 options_gFirstOption;
extern s8 gConflict[EXTENT(4)][10];
extern s8 gConflictExists[EXTENT(4)];
extern s16 gControlSelect[EXTENT(4)];
extern s16 gControlFirst[EXTENT(4)];
extern s16 gControlMenuSize;
extern s8 prevControlScrActive[EXTENT(4)];
extern OSMesgQueue musyx_task_done_queue;
extern s8 controlScrActive[EXTENT(4)];
extern s32 controls_buttonsInited[EXTENT(4)];
extern s32 playerInPause;
extern s8 expansion_pak_present;
extern s8 attract_alt_text_flag;
extern s32 ghosts_enabled;
extern char *carscr_buttonNames[EXTENT(9)];
extern MenuButton carscr_buttonInfo[EXTENT(4)][44];
extern f32 carscr_gButtonZoom;
extern f32 carscr_gButtonScale;
extern f32 carscr_gButtonSpace;
extern f32 carscr_buttonX;
extern f32 carscr_maxY;
extern f32 carscr_minY;
extern f32 carscr_buttonZ;
extern f32 carscr_cursorZ;
extern s32 gCarPartFrame;
extern s32 gTurntableIdx[EXTENT(4)][2];
extern char *carComponentNames[EXTENT(5)];
extern s16 componentToName[EXTENT(11)];
extern CarComponentSlot gCarComponents[EXTENT(1)][11];
/* compLight: declared by its files (src/ovl_a/groups/carscr/carscr.c: defines it) */
extern s8 carscr_inited;
extern struct Blit *carscr_blit;
extern s16 gCarSheen[EXTENT(4)];
extern s32 carscr_gActiveOption[EXTENT(4)];
extern f32 gTransStats[EXTENT(8)][4];
extern f32 gHandlingStats[EXTENT(3)][4];
extern f32 gEngineStats[EXTENT(9)][4];
extern f32 gTireStats[EXTENT(5)][4];
extern f32 gFrameStats[EXTENT(6)][4];
extern HudBlitDef carscr_mb[EXTENT(38)];
extern s8 gCarOrder[EXTENT(16)];
extern s8 carscr_color_option_masks[EXTENT(16)];
extern CarScreenMsgPos carscr_msgPos;
extern TextColor carscr_solidColor;
extern TextColor carscr_normalColor;
extern const HudBlitDef cheatscr_multiblit[EXTENT(1)];
extern s8 cheatscr_inited;
extern struct Blit *cheatscr_blit;
extern s32 cheatscr_gSelectedOption;
extern s32 gCheatMenuExit;
extern char *circuitscr_buttonNames[EXTENT(9)];
extern MenuButton circuitscr_buttonInfo[EXTENT(12)];
extern s32 circuitscr_buttonsInited;
extern s32 circuitscr_buttonsJustInited;
extern f32 circuitscr_gButtonScale;
extern f32 circuitscr_gButtonSpace;
extern f32 circuitscr_buttonX;
extern f32 circuitscr_maxY;
extern f32 circuitscr_minY;
extern f32 circuitscr_buttonZ;
extern f32 circuitscr_cursorZ;
extern HudBlitDef circuitscr_mblit[EXTENT(1)];
extern struct Blit *circuitscr_blit;
extern s8 circuitscr_inited;
extern s32 gCircuitScreenExit;
extern s8 gEnterCircuitCode;
extern s8 gConfirmNewCircuit;
extern s8 gInvalidCode;
extern s8 gWrongNumPlayers;
extern s8 gContinueEnabled;
extern s8 gStatsEnabled;
extern s8 gShowTrophy;
extern char *gTrophyNames[EXTENT(3)];
extern char *gTrophySheenNames[EXTENT(3)];
extern s32 trophyObjIdx;
extern s32 trophySheenObjIdx;
extern s32 trophySheenColor[EXTENT(2)];
extern ButtonColor circuitscr_init_button_color;
extern ButtonColor circuitscr_button_color;
extern Egg gEggs[EXTENT(21)];
extern Egg start_screen_egg[EXTENT(1)];
extern union InputSequence *gEgg;
extern f32 gNextTimeOut;
extern s32 gEggIdx;
extern HudBlitDef licensescr_blits[EXTENT(2)];
extern struct Blit *licensescr_blit;
extern s8 licensescr_inited;
extern char *modescr_buttonNames[EXTENT(8)];
extern ModeScreenButton modescr_buttonInfo[EXTENT(13)];
extern f32 modescr_title_button_position[EXTENT(3)];
extern s32 modescr_buttonsInited;
extern s32 modescr_buttonsJustInited;
extern f32 modescr_gButtonScale;
extern f32 modescr_gButtonSpace;
extern f32 modescr_buttonX;
extern f32 modescr_maxY;
extern f32 modescr_buttonZ;
extern f32 modescr_cursorZ;
extern HudBlitDef modescr_mblit;
extern s8 modescr_inited;
extern struct Blit *modescr_blit;
extern s8 modescr_validOption[EXTENT(4)][7];
extern s32 gModeMenuSelect;
extern s32 exitModeScreen;
extern PcText4 modescr_button_color;
extern s8 pakscr_gInited;
extern struct Blit *gBlit;
extern s8 gDrawText;
extern s32 gPakErr;
extern HudBlitDef pakscr_mblit;
extern char *playerscr_buttonNames[EXTENT(12)];
extern MenuButton playerscr_buttonInfo[EXTENT(4)][22];
extern f32 playerscr_gButtonZoom;
extern f32 playerscr_gButtonScale;
extern f32 playerscr_gButtonSpace;
extern f32 playerscr_buttonX;
extern f32 playerscr_maxY;
extern f32 playerscr_minY;
extern f32 playerscr_buttonZ;
extern f32 playerscr_cursorZ;
extern HudBlitDef multiBlit[EXTENT(1)];
extern PlayerRect playerscr_gPlayerRects[EXTENT(4)][4];
extern s8 screenInited;
extern s32 exitPlayerScreen;
extern struct Blit *playerscr_blit;
extern u8 playerscr_del[EXTENT(4)];
extern u8 playerscr_end[EXTENT(4)];
extern u8 playerscr_spc[EXTENT(12)];
extern char *recordscr_buttonNames[EXTENT(14)];
extern MenuButton recordscr_buttonInfo[EXTENT(23)];
extern f32 recordscr_gButtonScale;
extern f32 recordscr_gButtonSpace;
extern f32 recordscr_buttonX;
extern f32 recordscr_maxY;
extern f32 recordscr_minY;
extern f32 recordscr_buttonZ;
extern f32 recordscr_cursorZ;
extern struct pc_multiblit recordscr_mblit[EXTENT(1)];
extern s8 recordscr_inited;
extern void *recordscr_blit;
extern void *recordscr_viewPane;
extern s8 gViewTrackValid[EXTENT(32)];
extern s8 gViewStatValid[EXTENT(6)][13];
extern s8 recordscr_gSelectPakIdx;
extern HudBlitDef startscr_mblit[EXTENT(1)];
extern s8 startscr_inited;
extern struct Blit *startscr_blit;
extern s8 startscr_validOption[EXTENT(12)];
extern char *startscr_buttonNames[EXTENT(8)];
extern StartScreenButton startscr_buttonInfo[EXTENT(18)];
extern f32 startscr_players_button_position[EXTENT(3)];
extern f32 startscr_title_button_position[EXTENT(3)];
extern s32 startscr_buttonsInited;
extern s32 startscr_buttonsJustInited;
extern f32 startscr_gButtonScale;
extern f32 startscr_gButtonSpace;
extern f32 startscr_buttonX;
extern f32 startscr_maxY;
extern f32 startscr_buttonZ;
extern f32 startscr_cursorZ;
extern s32 gStartMenuSelect;
extern s32 gMaxPlayers;
extern s32 exitStartScreen;
extern HudBlitDef titlescr_blits[EXTENT(3)];
extern s32 titlescr_inited;
extern struct Blit *titlescr_blit;
extern s32 movieIdx;
extern char *trackscr_buttonNames[EXTENT(9)];
extern TrackScreenButton trackscr_buttonInfo[EXTENT(48)];
extern f32 trackscr_title_button_position[EXTENT(3)];
extern s32 trackscr_buttonsInited;
extern s32 trackscr_buttonsJustInited;
extern f32 trackscr_gButtonScale;
extern f32 trackscr_gButtonSpace;
extern f32 trackscr_buttonX;
extern f32 trackscr_maxY;
extern f32 trackscr_minY;
extern f32 trackscr_buttonZ;
extern f32 trackscr_cursorZ;
extern char *trackMapObjNames[EXTENT(20)];
extern char *trackScreenNames[EXTENT(19)];
extern s32 trackscr_validOption[EXTENT(7)][21];
extern f32 yFog[EXTENT(19)];
extern HudBlitDef trackscr_mblit[EXTENT(17)];
extern struct Blit *trackscr_blit;
extern s8 trackscr_inited;
extern s32 kNumMapSects;
extern s32 pathWaveIdx;
extern f32 pathPos;
extern TrackMapEntry trackMap[EXTENT(20)];
extern s32 trackMapObj;
extern struct Blit *trackScreenPoly;
extern struct Blit *trackFogPoly;
extern char *trackscr_arrowNames[EXTENT(16)][2];
extern s8 trackReady[EXTENT(19)];
extern ButtonColor loadTrackScreen_backColor;
extern f32 buildTrackMap_mapPos[EXTENT(3)];
extern ButtonColor race_map_vertex_color;
extern f32 buildRaceTrackMaps_mapPos[EXTENT(3)];
extern f32 buildRaceTrackMaps_selMapPos[EXTENT(3)];
extern f32 buildStuntTrackMaps_mapPos[EXTENT(3)];
extern f32 buildBattleTrackMaps_mapPos[EXTENT(3)];
extern ButtonColor trackscr_init_button_color;
extern ButtonColor trackscr_update_button_color;
extern s8 gPakScr;
extern u8 num_paks_found;
extern HudBlitDef warningscr_blits[EXTENT(1)];
extern u8 carscr_ready_text[EXTENT(20)];
extern u8 carscr_ghost_width_text_first[EXTENT(16)];
extern u8 carscr_ghost_width_text_second[EXTENT(16)];
extern char carscr_turntable_model_name[EXTENT(12)];
extern char carscr_turntable_base_model_name[EXTENT(16)];
extern char carscr_part_frame_model_name[EXTENT(12)];
extern char carscr_stat_bars_texture_name[EXTENT(16)];
extern char carscr_stat_text_texture_name[EXTENT(72)];
extern char carscr_title_texture_name[EXTENT(16)];
extern char carscr_cursor_texture_name[EXTENT(8)];
extern char carscr_a_button_texture_name[EXTENT(4)];
extern char carscr_b_button_texture_name[EXTENT(4)];
extern char carscr_select_texture_name[EXTENT(16)];
extern char carscr_button_texture_name[EXTENT(8)];
extern char cheatscr_number_format[EXTENT(208)];
extern u8 circuitscr_empty_code_text[EXTENT(24)];
extern char circuitscr_sheen_texture_name[EXTENT(8)];
extern u8 circuitscr_del_text[EXTENT(4)];
extern u8 circuitscr_review_track_format[EXTENT(8)];
extern u8 circuitscr_trophy_title_format[EXTENT(8)];
extern u8 circuitscr_new_circuit_title_format[EXTENT(8)];
extern u8 circuitscr_new_circuit_tracks_format[EXTENT(8)];
extern u8 circuitscr_progress_title_format[EXTENT(8)];
extern u8 circuitscr_finished_title_format[EXTENT(8)];
extern u8 circuitscr_review_place_format[EXTENT(4)];
extern u8 circuitscr_review_points_format[EXTENT(4)];
extern u8 circuitscr_finished_count_format[EXTENT(4)];
extern u8 circuitscr_to_go_count_format[EXTENT(4)];
extern u8 circuitscr_place_separator[EXTENT(4)];
extern u8 circuitscr_points_format[EXTENT(4)];
extern u8 circuitscr_finish_place_separator[EXTENT(4)];
extern char circuitscr_title_texture_name[EXTENT(16)];
extern char circuitscr_cursor_texture_name[EXTENT(8)];
extern char circuitscr_a_button_texture_name[EXTENT(4)];
extern char circuitscr_b_button_texture_name[EXTENT(4)];
extern char circuitscr_select_texture_name[EXTENT(16)];
extern char circuitscr_button_texture_name[EXTENT(16)];
extern u8 licensescr_build_info_format[EXTENT(108)];
extern char modescr_one_player_format[EXTENT(8)];
extern char modescr_players_format[EXTENT(8)];
extern char modescr_cursor_texture_name[EXTENT(8)];
extern char modescr_a_button_texture_name[EXTENT(4)];
extern char modescr_b_button_texture_name[EXTENT(4)];
extern char modescr_title_texture_name[EXTENT(16)];
extern char modescr_select_texture_name[EXTENT(16)];
extern char modescr_button_texture_name[EXTENT(8)];
extern char pakscr_status_no_controller_format[EXTENT(8)];
extern char pakscr_status_no_pak_format[EXTENT(8)];
extern char pakscr_status_damaged_format[EXTENT(8)];
extern char pakscr_status_rumble_format[EXTENT(8)];
extern char pakscr_status_in_use_format[EXTENT(8)];
extern char pakscr_status_free_format[EXTENT(8)];
extern char pakscr_status_full_format[EXTENT(8)];
extern char pakscr_pak_title_format[EXTENT(8)];
extern char pakscr_action_separator[EXTENT(4)];
extern char pakscr_file_number_format[EXTENT(4)];
extern char pakscr_name_suffix_separator[EXTENT(4)];
extern char pakscr_file_blocks_format[EXTENT(4)];
extern char pakscr_free_pages_format[EXTENT(4)];
extern char pakscr_blocks_needed_format[EXTENT(16)];
extern char pakscr_verify_name_separator[EXTENT(4)];
extern char pakscr_rumble_text_separator[EXTENT(4)];
extern char pakscr_damaged_text_separator[EXTENT(148)];
extern u8 playerscr_player_title_format[EXTENT(8)];
extern u8 playerscr_delete_message_format[EXTENT(8)];
extern u8 playerscr_pak_full_title_format[EXTENT(8)];
extern u8 playerscr_pak_full_text[EXTENT(12)];
extern u8 playerscr_pak_damaged_title_format[EXTENT(8)];
extern u8 playerscr_pak_damaged_text[EXTENT(12)];
extern char playerscr_title_texture_name[EXTENT(16)];
extern char playerscr_cursor_texture_name[EXTENT(8)];
extern char playerscr_a_button_texture_name[EXTENT(4)];
extern char playerscr_b_button_texture_name[EXTENT(220)];
extern config gScreenRect;
extern u8 recordscr_circuit_title_format[EXTENT(8)];
extern u8 recordscr_track_variant_title_format[EXTENT(8)];
extern u8 recordscr_race_laps_format[EXTENT(4)];
extern u8 recordscr_race_count_format[EXTENT(4)];
extern u8 recordscr_race_first_format[EXTENT(4)];
extern u8 recordscr_race_second_format[EXTENT(4)];
extern u8 recordscr_race_third_format[EXTENT(4)];
extern u8 recordscr_race_crashes_format[EXTENT(4)];
extern u8 recordscr_race_death_attempted_format[EXTENT(4)];
extern u8 recordscr_race_death_completed_format[EXTENT(4)];
extern u8 recordscr_race_ghosts_format[EXTENT(4)];
extern u8 recordscr_race_distance_format[EXTENT(12)];
extern u8 recordscr_race_coins_a_format[EXTENT(8)];
extern u8 recordscr_race_coins_b_format[EXTENT(8)];
extern u8 recordscr_battle_matches_format[EXTENT(4)];
extern u8 recordscr_battle_wins_format[EXTENT(4)];
extern u8 recordscr_battle_kills_format[EXTENT(4)];
extern u8 recordscr_battle_deaths_format[EXTENT(4)];
extern u8 recordscr_stunt_best_format[EXTENT(4)];
extern u8 recordscr_stunt_best_average_format[EXTENT(4)];
extern u8 recordscr_stunt_points_format[EXTENT(4)];
extern u8 recordscr_stunt_time_format[EXTENT(4)];
extern u8 recordscr_stunt_crashes_format[EXTENT(4)];
extern u8 recordscr_stunt_count_format[EXTENT(4)];
extern u8 recordscr_stunt_coins_a_format[EXTENT(8)];
extern u8 recordscr_stunt_coins_b_format[EXTENT(8)];
extern u8 recordscr_stunt_kind_label_format[EXTENT(4)];
extern u8 recordscr_stunt_kind_count_format[EXTENT(4)];
extern u8 recordscr_obstacle_races_format[EXTENT(4)];
extern u8 recordscr_obstacle_finished_format[EXTENT(4)];
extern u8 recordscr_obstacle_crashes_format[EXTENT(4)];
extern u8 recordscr_obstacle_death_attempted_format[EXTENT(4)];
extern u8 recordscr_obstacle_death_completed_format[EXTENT(4)];
extern u8 recordscr_circuit_count_format[EXTENT(4)];
extern u8 recordscr_circuit_first_format[EXTENT(4)];
extern u8 recordscr_circuit_second_format[EXTENT(4)];
extern u8 recordscr_circuit_third_format[EXTENT(4)];
extern u8 recordscr_circuit_death_attempted_format[EXTENT(4)];
extern u8 recordscr_circuit_death_completed_format[EXTENT(4)];
extern u8 recordscr_circuit_best_points_format[EXTENT(4)];
extern u8 recordscr_circuit_total_points_format[EXTENT(4)];
extern u8 recordscr_total_km_format[EXTENT(12)];
extern u8 recordscr_total_miles_format[EXTENT(12)];
extern u8 recordscr_total_stunt_points_format[EXTENT(4)];
extern u8 recordscr_total_battle_points_format[EXTENT(4)];
extern u8 recordscr_silver_race_coins_format[EXTENT(8)];
extern u8 recordscr_gold_race_coins_format[EXTENT(8)];
extern u8 recordscr_silver_stunt_coins_format[EXTENT(8)];
extern u8 recordscr_gold_stunt_coins_format[EXTENT(8)];
extern u8 recordscr_unlocked_circuit_format[EXTENT(24)];
extern u8 recordscr_no_time_text[EXTENT(12)];
extern u8 recordscr_time_format[EXTENT(16)];
extern char recordscr_title_texture_name[EXTENT(16)];
extern char recordscr_cursor_texture_name[EXTENT(8)];
extern char recordscr_a_button_texture_name[EXTENT(4)];
extern char recordscr_b_button_texture_name[EXTENT(52)];
extern char recordscr_select_texture_name[EXTENT(16)];
extern char recordscr_button_texture_name[EXTENT(104)];
extern char startscr_player_count_format[EXTENT(4)];
extern char startscr_button_player_count_format[EXTENT(136)];
extern u8 titlescr_prompt_text[EXTENT(1216)];
extern ResourceRange trackscr_arrowPos[EXTENT(2)][4];
extern char trackscr_laps_format[EXTENT(4)];
extern char trackscr_option5_format[EXTENT(4)];
extern char trackscr_option6_format[EXTENT(4)];
extern char trackscr_option7_format[EXTENT(4)];
extern char trackscr_option9_format[EXTENT(4)];
extern char trackscr_option10_format[EXTENT(4)];
extern char trackscr_drones_format[EXTENT(4)];
extern char track_fog_texture_name[EXTENT(4)];
extern char trackscr_title_texture_name[EXTENT(16)];
extern char trackscr_cursor_texture_name[EXTENT(8)];
extern char trackscr_a_button_texture_name[EXTENT(4)];
extern char trackscr_b_button_texture_name[EXTENT(4)];
extern char trackscr_left_select_texture_name[EXTENT(16)];
extern char trackscr_left_button_texture_name[EXTENT(8)];
extern char trackscr_right_select_texture_name[EXTENT(16)];
extern char trackscr_right_button_texture_name[EXTENT(8)];
extern char warningscr_message_format[EXTENT(768)];
extern f32 circuitscr_trophy_scale;
extern f32 circuitscr_degrees_to_radians;
extern f32 carscr_marker_hide_angle_min;
extern f32 carscr_marker_hide_angle_max;
extern f32 carscr_slider_hide_angle_min;
extern f32 carscr_slider_hide_angle_max;
extern f32 carscr_slider_flip_angle;
extern f32 carscr_front_tire_scale_step;
extern f32 carscr_rear_tire_scale_step;
extern f32 gTrophyMat[EXTENT(12)];
extern s16 gCodeSelect;
extern s16 gCodeLen;
extern s16 gReviewTrack;
extern s16 gProgressWidth;
extern s16 gProgressHeight;
extern s16 gProgressTop;
extern s16 gMenuBottom;
extern s32 *gCarMatrices[EXTENT(48)];
extern BlitColor carscr_marker_colors[EXTENT(4)][3];
extern s32 carscr_buttonsInited[EXTENT(4)];
extern s32 carscr_buttonsJustInited[EXTENT(4)];
extern s32 gState;
extern s32 gMaxMenuHeight;
extern s8 pakscr_gYes;
extern s16 gPickedPak;
extern s16 pakscr_gFreePages;
extern void **gDeleteFile;
extern s16 gDeleteSize;
extern s16 gDeleteSelect;
extern CarScreenCar carscr_cars[EXTENT(4)][13];
extern s16 gDeleteSelTop;
extern s16 gDeleteHeight;
extern s16 gDeleteSelPos;
extern f32 gTurntableMat[EXTENT(4)][2][12];
extern struct FiveParts *carscr_pane[EXTENT(4)];
extern s8 gThisCar[EXTENT(4)];
extern s8 gThisCarIndex[EXTENT(4)];
extern s8 gCars[EXTENT(4)][13];
extern s8 gNumGhosts;
extern struct Record **limboGhosts;
extern s16 gLastCar[EXTENT(4)];
extern s16 gNumCarsActive[EXTENT(4)];
extern s8 gCarLocked[EXTENT(4)];
extern s8 allLocked;
extern s32 carscr_wait;
extern s32 carscr_gFirstOption[EXTENT(4)];
extern s8 carscr_gActiveOptionEnabled[EXTENT(4)];
extern s8 carscr_gActiveZ[EXTENT(4)];
extern s32 carscr_gNumOptions[EXTENT(4)];
extern s32 carscr_gOpts[EXTENT(4)][19];
extern f32 gStats[EXTENT(4)][4];
extern s32 gHornSnd[EXTENT(4)];
extern u8 trackscr_fogColor[EXTENT(8)];
extern u8 n64FogPal[EXTENT(20)];
extern SelectCarWaveSlot gSelectCarWavePtrs[EXTENT(16)];
extern SelectCarState gSelectCar[EXTENT(52)];
extern s32 recordscr_buttonsInited;
extern s32 recordscr_buttonsJustInited;
extern f32 recordscr_cursorSpeed;
extern f32 recordscr_scrollSpeed;
extern s8 recordscr_gSelectMenuValid[EXTENT(11)];
extern char gBlocksNeeded[EXTENT(5)];
extern s8 recordscr_gSelectMenuVisible[EXTENT(12)];
extern s8 gViewMenuValid[EXTENT(2)];
extern s8 gViewMenuVisible[EXTENT(2)];
extern struct ListNode *gStatPlayer;
extern s32 gRecordState;
extern s32 recordscr_gSelectMenuSelect;
extern s8 gSelectMenuClear;
extern s32 gViewMenuSelect;
extern s32 gViewStatType[EXTENT(6)];
extern s32 recordscr_gNumPakPlayers[EXTENT(4)];
extern u8 gDeleteName[EXTENT(13)];
extern s8 gDeleteConfirm;
extern s16 oldTrack;
extern s8 recordscr_gYes;
extern s16 gNumTracks;
extern s8 gDeleteNotify;
extern s16 gTrack;
extern s8 gRecordTrack;
extern s16 trackscr_gActiveOption;
extern s8 gRecordCircuit;
extern s32 playerscr_buttonsInited[EXTENT(4)];
extern s16 trackscr_gFirstOption;
extern s32 gTrackType;
extern s32 playerscr_buttonsJustInited[EXTENT(4)];
extern s16 trackscr_gNumOptions;
extern f32 playerscr_cursorSpeed[EXTENT(4)];
extern s16 trackscr_gOpts[EXTENT(36)];
extern f32 playerscr_scrollSpeed[EXTENT(4)];
extern s8 trackscr_gActiveOptionEnabled;
extern f32 gDefTime;
extern s8 trackscr_gActiveZ;
extern TrackSlot gTracks[EXTENT(19)];
extern s8 trackMapsBuilt;
extern MapLod mapObj[EXTENT(7)];
extern MapLod saved_map_lods[EXTENT(7)];
extern void *map_display_list_blocks[EXTENT(7)];
extern void *map_vertex_blocks[EXTENT(7)];
extern struct FiveParts *playerscr_pane[EXTENT(4)];
extern f32 modescr_cursorSpeed;
extern f32 startscr_cursorSpeed;
extern s32 playerscr_gNumPakPlayers[EXTENT(4)];
extern f32 trackMapMat[EXTENT(12)];
extern f32 playerCursorSpeed;
extern void *gPakFull[EXTENT(4)];
extern s32 currentTrackMap;
extern struct Owner *gSelectedPlayer[EXTENT(4)];
extern s32 desiredTrackMap;
extern u8 gCreatePlayerNames[EXTENT(4)][13];
extern f32 mapTransition;
extern s16 playerscr_gNameSelect[EXTENT(4)];
extern s16 playerscr_gNameLen[EXTENT(4)];
extern s32 gPlayerState[EXTENT(4)];
extern struct FiveParts *circuitscr_pane;
extern s8 gSelectMenuMaxSize;
extern s32 gCircuitMenuSelect;
extern s8 playerscr_gSelectMenuValid[EXTENT(4)][14];
extern s32 newCircuitType;
extern s8 playerscr_gSelectMenuVisible[EXTENT(4)][14];
extern s8 playerscr_gSelectMenuCount[EXTENT(8)];
extern s32 playerscr_gSelectMenuSelect[EXTENT(4)];
extern struct tCircuit *gCircuit;
extern u8 circuitscr_gCircuitCode[EXTENT(20)];
extern s8 gSelectMenuControls[EXTENT(4)];
extern s8 playerscr_gSelectPakIdx[EXTENT(4)];
extern s8 gSelectPlayerNum[EXTENT(4)];
extern s8 circuitscr_gYes;
extern s8 gDeleteMenuSelect[EXTENT(4)];
extern s8 gDeleteMenuConfirm[EXTENT(4)];
extern s8 gDeleteMenuYes[EXTENT(4)];
extern s8 gDeleteMenuNotify[EXTENT(4)];
extern u8 gDeleteMenuName[EXTENT(4)][13];
extern f32 trackscr_InitScreen_fogPos[EXTENT(3)];

/* Functions */
s32 anim_car_color_marker(struct Blit *blt);
s32 animArrows(struct Blit *blt);
s32 animate_blit_unchanged(s32 blit);
s32 animate_pause_dim(void *blit);
void AnimateCoin(struct Visual *v, s16 op);
void AnimateFlag(struct Visual *v, s16 op);
void AnimateKnockdown(struct pc_visual *v, s16 op);
void AnimateMine(struct Visual *v, s16 op);
void AnimateRotGeneric(struct pc_visual *v, s16 op);
void AnimateVelRotGeneric(struct Visual *v, s16 op);
void AnimateWeaponIcon(struct Visual *v, s16 op);
s32 animControllerDim(void *blit);
s32 animControls(struct Blit *blt);
s32 animPause(struct Blit *blt);
s32 animPauseText(struct MenuItem *item);
s32 animRumbleText(struct Blit *blt);
s32 animStatBars(struct Blit *blt);
s32 animStatText(s32 i);
s32 animSureText(struct MenuItem *item);
s32 AnimText(void *callback);
s32 animTitleText(struct MenuItem *item);
void animTrackMaps(void);
s32 audio_animText(s32 arg0);
void BlowFog(void);
void buildBattleTrackMaps(void);
void BuildFileDelList(void);
void buildObstacleTrackMaps(void);
void buildRaceTrackMaps(void);
void buildStuntTrackMaps(void);
void buildTrackMap(void);
void buildTrackMaps(void);
void calcControlMenuMaxSize(void);
void calcProgressSize(void);
void calcSelectMenuMaxSize(void);
void calcStats(s32 player);
void calcValidOptions(s32 player);
s32 CancelFunc(s32 pak);
s32 carscr_animNewSlider(struct Blit *blt);
s32 carscr_animText(struct Blit *blt);
void carscr_initButtons(s8 p);
void carscr_InitScreen(void);
void carscr_uninitButtons(s8 p);
void carscr_UninitScreen(void);
void carscr_updateButtons(s8 p);
s32 carscr_validOption(s32 player, s32 item);
u8 censor(intptr_t name);
s32 cheatscr_animText(struct Blit *blit);
void cheatscr_initScreen(void);
void cheatscr_printOption(s32 x, s32 y, s32 item, u8 *name, u8 *value);
void cheatscr_uninitScreen(void);
s32 CheckForEgg(void);
void circuitscr_animButtonText(void);
s32 circuitscr_animText(struct Blit *blit);
void circuitscr_initButtons(void);
void circuitscr_initScreen(void);
void circuitscr_uninitButtons(void);
void circuitscr_uninitScreen(void);
void circuitscr_updateButtons(void);
void claimPlayer(s32 playerIdx, struct Owner *player);
void cleanupControlScreen(void);
void clearCarComponents(s32 p);
void clearTrackMap(void);
void clearTrackMaps(void);
void controlCircuitCode(void);
void controlControls(s32 p);
void controlCreatePlayer(s32 p);
void ControlDamaged(void);
void controlDeletePlayer(s32 p);
void ControlFileDel(void);
void ControlFileDelVerify(void);
void ControlGhost(void);
void controlMain(void);
void ControlName(s32 player);
void controlPakDamaged(s32 p);
void controlPakFull(s32 p);
void ControlPickPak(void);
void ControlRumblePak(void);
void controls_animButtonText(s8 p);
s32 controls_animText(s32 blit);
void controls_initButtons(s8 player);
void controls_uninitButtons(s8 row);
void controlShowTrophy(void);
void ControlUnlocked(void);
void controlView(void);
void controlWait(s32 p);
void createBG(void);
void createGridMap(void);
void CreateSelectCar(s16 slot, s16 player, s32 cam);
s32 demo_animText(s32 arg0);
void drawBattleScore(void);
void drawBattleStandings(void);
void drawBattleStats(void);
void DrawCircuitCode(void);
void drawCircuitStats(void);
void drawCreatePlayer(s32 player);
void DrawDamaged(void);
void drawDeletePlayer(s32 player);
void DrawFileDel(void);
void DrawFileDelVerify(void);
void DrawGhost(void);
void drawMain(void);
void DrawName(s32 p);
void drawObstacleStats(void);
void DrawPickPak(void);
void drawRaceStandings(void);
void drawRaceStats(void);
void drawRect(s32 x, s32 y, s32 w, s32 h, u8 *color);
void drawReview(void);
void DrawRumblePak(void);
void DrawScores(void);
void drawScores(s32 category);
void drawStuntScore(void);
void drawStuntStandings(void);
void drawStuntStats(void);
void drawTotalStats(void);
void DrawUnlocked(void);
void drawUnlockedCars(void);
void drawUnlockedParts(void);
void drawUnlockedTracks(void);
void drawWait(s32 player);
void FindGhosts(s32 pak);
s32 gameover_animText(s32 blit);
void gameover_uninitScreen(void);
s32 gameover_validOption(s32 option);
void getFinishedStr(u8 *destination);
void getFinishPlaceStr(s32 p, u8 *str);
struct Record **getNextGhost(struct Record **node, s8 direction);
void GetPakStatusStr(s32 player, char *str);
void getPlaceStr(s32 player, u8 *destination);
void getPlayerStr(s32 player, u8 *destination);
void getPointsStr(s32 player, u8 *destination);
void getScoreStr(u8 *destination, f32 time, s32 unused_format);
void getToGoStr(u8 *destination);
void highscore_UninitScreen(void);
void InitEgg(s32 egg);
void initRecordScreen(void);
void InitSelectCarLoad(void);
void InitVisualsCarScr(void);
s32 licensescr_animText(s32 blit);
void loadPathData(void);
s32 LoadSelectCar(s16 slot, s16 index, s32 *waveInst);
void loadTrackScreen(void);
s32 menuItemValid(s32 mode);
s32 modescr_animText(void *callback_context);
void modescr_initButtons(void);
void modescr_initScreen(void);
s32 modescr_optionValid(s32 index);
void modescr_uninitButtons(void);
void modescr_uninitScreen(void);
void modescr_updateButtons(void);
void *musyx_alloc(s32 size, s32 alignment);
intptr_t musyx_cart_address(intptr_t offset);
void musyx_clear_retrace_callback(s32 callback);
void musyx_free(u32 address);
s32 musyx_set_retrace_callback(intptr_t callback);
s32 musyx_start_dma_list(intptr_t reqs, s32 count, intptr_t doneQueue);
void musyx_wait_audio_task(void);
s32 options_animText(void *item);
void options_initScreen(void);
void options_TitlePane(void);
void options_uninitScreen(void);
void options_updateButtons(void);
s32 OptionValid(s32 opt);
void **pakscr_FindFile(s32 id);
s32 playerBeingCreated(s32 count, s32 skip);
s32 playerBeingDeleted(s32 count, s32 skip);
s32 playerClaimed(s32 skip, struct Profile **owner);
s32 playerscr_animText(void *callback_context);
void playerscr_controlSelect(s32 p);
void playerscr_drawSelect(s32 player);
void playerscr_initButtons(s8 player);
void playerscr_initScreen(void);
void playerscr_uninitButtons(s8 p);
void playerscr_uninitScreen(void);
void playerscr_updateButtons(s8 p);
void playerscr_updatePakInfo(void);
void playerscr_updateSelectInfo(s32 player);
void PrintScores(void);
void RankScores(void);
s32 recordscr_animText(void *callback_context);
void recordscr_controlSelect(void);
void recordscr_drawSelect(void);
void recordscr_drawStuntTotals(void);
void recordscr_drawView(void);
void recordscr_initButtons(void);
void recordscr_uninitButtons(void);
void recordscr_updateButtons(void);
void recordscr_updatePakInfo(void);
void recordscr_updateSelectInfo(void);
void releaseGridMap(void);
void removeBG(void);
void resetEnabled(void);
void setCamera(void);
void SetFogIntensity(s32 initialize);
void setGameOverState(s32 mode);
void setGhosts(void);
void SetHighScoreState(s32 newState);
void setPauseState(s32 state);
void setPlayerState(s32 player, s32 mode);
void setRecordState(s32 mode);
void SetSelectTireSizes(s16 id, s16 player);
void SetState(s32 mode);
void showCarSelect(void);
void showGridMap(s32 enabled);
void showTrackSelect(void);
void StartCactus(struct Target *t);
void StartCoin(struct Target *t);
void StartCone(struct Target *t);
void StartFence(struct Target *t);
void StartKnockdown(struct Target *t);
void StartMine(struct Target *t);
void startscr_animButtonText(void);
s32 startscr_animText(void *callback_context);
void startscr_initButtons(void);
void startscr_InitEgg(s32 egg);
void startscr_initScreen(void);
s8 startscr_optionValid(s32 index);
void startscr_uninitButtons(void);
void startscr_uninitScreen(void);
void startscr_updateButtons(void);
void StartWeaponIcon(struct Target *t);
void StartWindow(struct Target *t);
s32 TargetColl3Piece(s32 target, s32 slotp, s32 unused2, s32 unused3);
s32 TargetCollBBox(struct CollisionObject *object, s16 *index, void *unused3, s32 unused4);
s32 TargetCollCylinder(s16 *slotp, f32 *pos, f32 *radiusp, f32 *dist);
s32 TargetCollMineCylinder(s16 *slotp, f32 *pos, f32 *radiusp, f32 *dist);
s32 TargetCollRadius(s16 *player_index, f32 *point, f32 *radius_in, f32 *radial_error);
s32 TargetCollTrigger(struct Target *t, s16 *player, s32 unused3, s32 unused4);
s32 titlescr_animText(s32 blit);
void trackscr_animButtonText(void);
s32 trackscr_animNewSlider(struct Blit *blt);
s32 trackscr_animText(void *callback_context);
void trackscr_animTrackMap(void);
void trackscr_initButtons(void);
void trackscr_InitScreen(void);
void trackscr_uninitButtons(void);
void trackscr_UninitScreen(void);
void trackscr_updateButtons(void);
s32 TrackValid(s32 track);
void uninitRecordScreen(void);
void unloadPathData(void);
void UnloadSelectCar(s16 id);
void unloadTrackScreen(void);
void updateAudioScreen(void);
void updateBG(void);
void updateCarComponents(s32 p);
void updateCarScreen(void);
void updateCheatScreen(void);
void updateCircuitScreen(void);
void updateControlScreen(void);
void updateFrontEnd(void);
void updateGameOverScreen(void);
void updateHighScoreScreen(void);
void updateLicenseScreen(void);
void updateModeScreen(void);
void updateOptionScreen(void);
void updatePakScreen(void);
void updatePauseScreen(void);
void updatePauseStateCleanup(void);
void updatePlayerScreen(void);
void updateRecordScreen(void);
void updateSelectButtons(void);
void UpdateSelectCar(s16 slot, s16 player, u8 r, u8 g, u8 b, s16 sheenIdx, f32 *pos, f32 (*uvs)[3], u8 alpha);
void updateStartScreen(void);
void updateTitleScreen(void);
void updateTrackScreen(void);
void updateVideoScreen(void);
void updateViewButtons(void);
void updateViewInfo(void);
void updateWarningScreen(void);
void video_animButtonText(void);
s32 video_animPanes(void *item);
s32 video_animText(s32 arg0);
s32 warningscr_animText(s32 blit);

#endif /* GAME_FRONTEND_H */
