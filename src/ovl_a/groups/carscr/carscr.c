/* The car select screen (MAT3 carscr.c), GS_CAR: each player (a window each) turns through the roster on a
 * turntable and sets the car up in a scrolling list of 3D buttons: transmission, handling, engine, tyres, frame,
 * wings, the paint's three colours, battle team, rims, horn, tyre sizes and, in a ghost race, the three ghosts to
 * race. A one-player race also shows the car's stats (animStatBars) and the part being changed on the car
 * (updateCarComponents). When every player has locked a car in (A or Start), the cars turn to the front and the race
 * starts (GS_GAME_INIT). The 3D cars themselves are selectcarload.c's. */
#include "game/game.h"

/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10). */
typedef struct FileInfo {void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak;} FileInfo;
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): fileh NULL for a guest. */
typedef struct Profile Profile;
struct Profile {Profile **next,**previous;FileInfo **fileh;u32 id0,id1;u8 name[24];u8 **stats;};
/* A player's pad record (MAT3 tPadState, gPlayers): its buttons, its transmission choice and its profile. */
typedef struct Player {u8 slot,controller,pad02[2];u32 pressed,levels,repeated;u8 unknown10[50];u8 trans,unknown43[5];Profile **profile;} Player;
/* A ghost's save header (MAT3 tSaveHeader): the car it was recorded with and its settings (CAR_SETTING_*). */
typedef struct SaveHeader {u8 unknown00[8],car,trans;s8 settings[12];} SaveHeader;
typedef struct Record Record;
/* A ghost file (MAT3 tGhostFile; SaveInit's Ghost) in its PC layout: its links, pak file, data and header handles are
 * pointers (docs/port/CASTS.md). */
struct Record {Record **next;FileInfo **fileh;s8 track;u8 backward,name[14];u32 id0,id1;f32 finish;void *datah;SaveHeader **header;};
extern Player gPlayers[];
/* A doubly linked list's header (MAT3 tDLList): the ghost files on the paks. */
typedef struct RecordList {u8 indirect,doubly,unknown02[2];s32 count;Record **first, **last;} RecordList;
VIEW(RecordList, gGhostFiles);
#define gGhostFiles gGhostFiles_view
VIEW(Record **[4], gGhosts);
#define gGhosts gGhosts_view

typedef struct FiveParts FiveParts;
typedef struct Blit Blit;
typedef struct MultiBlit {char *name;s16 x,y,width,height,top,bottom,left,right;u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;} MultiBlit;

VIEW(void *[48], gCarMatrices);
#define gCarMatrices gCarMatrices_view
VIEW(MultiBlit[], carscr_mb);
#define carscr_mb carscr_mb_view

extern void audio_interrupt_handler(s32);

extern void particle_lifetime_set(void);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern FiveParts *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);

extern Blit *NewMultiBlit(s16,s16,MultiBlit *,s16);

VIEW(s16[], gCheatState);
#define gCheatState gCheatState_view

VIEW(s8[][13], car_color_choice_a);
#define car_color_choice_a car_color_choice_a_view
VIEW(s8[][13], car_color_choice_b);
#define car_color_choice_b car_color_choice_b_view
VIEW(s8[][13], car_color_choice_c);
#define car_color_choice_c car_color_choice_c_view

extern void ChangePaneSize(FiveParts *,s32,s32,s32,s32);

/* carscr_gOpts[]: the screen's options, in their order down the list (calcValidOptions keeps the offered ones). */
#define CAR_OPT_CAR 0
#define CAR_OPT_TRANS 1
#define CAR_OPT_HANDLING 2
#define CAR_OPT_ENGINE 3
#define CAR_OPT_TIRES 4
#define CAR_OPT_FRAME 5
#define CAR_OPT_WINGS 6
#define CAR_OPT_COLOR_A 7               /* the paint's three colour choices (car_color_choice_a/_b/_c) */
#define CAR_OPT_COLOR_B 8
#define CAR_OPT_COLOR_C 9
#define CAR_OPT_TEAM 10                 /* battle only */
#define CAR_OPT_RIMS 11                 /* the car turns side-on while it is selected */
#define CAR_OPT_HORN 12                 /* never offered */
#define CAR_OPT_FRONT_TIRE_SCALE 13     /* with gEnableTireScaling */
#define CAR_OPT_REAR_TIRE_SCALE 14
#define CAR_OPT_SHEEN 15                /* never offered */
#define CAR_OPT_GHOST_1 16              /* ghost races: the three ghosts to race */
#define CAR_OPT_GHOST_2 17
#define CAR_OPT_GHOST_3 18
#define CAR_OPTS 19

/* A profile's per-car settings (GetPlayerCarSetting, SavePlayerCarSetting; a ghost's SaveHeader.settings). */
#define CAR_SETTING_HANDLING 0
#define CAR_SETTING_ENGINE 1
#define CAR_SETTING_TIRES 2
#define CAR_SETTING_FRAME 3
#define CAR_SETTING_WINGS 4
#define CAR_SETTING_COLOR_A 5
#define CAR_SETTING_COLOR_B 6
#define CAR_SETTING_COLOR_C 7
#define CAR_SETTING_RIMS 8
#define CAR_SETTING_HORN 9
#define CAR_SETTING_FRONT_TIRE_SCALE 10 /* (scale - 0.75) x 100 */
#define CAR_SETTING_REAR_TIRE_SCALE 11

/* carscr_buttonInfo[player][]: a label button per option, its value button CAR_VALUE_BUTTON on, then these. */
#define CAR_VALUE_BUTTON(opt) ((opt) + CAR_OPTS)
#define CAR_BUTTON_TITLE 38
#define CAR_BUTTON_CURSOR 39
#define CAR_BUTTON_A_ICON 40
#define CAR_BUTTON_A_ARROW 41
#define CAR_BUTTON_B_ICON 42
#define CAR_BUTTON_B_ARROW 43
#define CAR_BUTTONS 44

#define CARS 13                         /* the roster; carscr_cars[][].car is car + 13 x the window's player */
#define CAR_TALL 10                     /* the roster's tall car, drawn higher on its turntable */
#define CAR_READY_FRAMES 10             /* 30 Hz frames from the last player's lock to the race (carscr_wait) */
#define BATTLE_TRANS 6                  /* the transmission every battle car has */
#define CHEAT_BATTLE_PAINT 13           /* gCheatState[]: battle cars keep their own paint, not the team's */
#define PANE_STYLE_MESSAGE 192          /* InitPane's style for the "waiting" boxes */
#define SND_BACK 38                     /* the menu sounds */
#define SND_MENU_DOWN 39
#define SND_MENU_UP 40
#define SND_MENU_ERROR 42
#define SND_HORNS 26                    /* a horn's effect: this + the horn */
#define BUTTON_DEFAULT_OPTION 0x80      /* with carscr_gActiveZ, would put an option back to its default */
#define TEXT_WAITING_FOR 151            /* gText.single[]: "waiting for" over the player still choosing */
#define TEXT_WAITING_FOR_ALL 31         /* gText.single[]: ... over several */
#define TEXT_NO_GHOSTS_SAVED 224        /* gText.single[]: a ghost window without a ghost */
#define TEXT_GHOSTS_OFF 225

/* The stats of player's car (gStats: four bars, 0-1): each the product of the stat of its handling, transmission,
 * engine, tyres and frame choices. The car settings' tables have a row per player at player + 1 (row 0 holds the
 * defaults). */
void calcStats(s32 player)
{
    s16 component;

    for (component = 0; component < 4; component++) {
        gStats[player][component] = gHandlingStats[gCarHandling[player + 1][gThisCar[player]]][component] * gTransStats[gCarTrans[player + 1][gThisCar[player]]][component];
        gStats[player][component] *= gEngineStats[gCarEngines[player + 1][gThisCar[player]]][component];
        gStats[player][component] *= gTireStats[gCarTires[player + 1][gThisCar[player]]][component];
        gStats[player][component] *= gFrameStats[gCarFrame[player + 1][gThisCar[player]]][component];
    }
}
/* Choose the ghosts of a ghost race (gGhosts[1..3]; [0] is the player) from the paks' ghost files (gGhostFiles) and
 * the three kept in memory (limboGhosts), those of this track and direction whose data can be read:
 * - gGhostSelect off: if the first player has a ghost of their own here, its best one and up to four around it in
 *   time (two faster, two slower) give the three nearest; else the three slowest ghosts;
 * - gGhostSelect on: the three fastest.
 * Without an Expansion Pak only one ghost is raced: the second and third are dropped (PurgeGhostData). Also clears
 * gLastCar[1..3]. */
void setGhosts(void)
{
    Record **nearGhosts[5];
    Record **ghosth;
    Record *ghost;
    Record **best = 0;
    Record **nextGhost;
    Profile *player;
    s32 ghostCount;
    s32 i;
    s32 j;
    player = *gPlayers[0].profile;
    for (i = 0; i < 4; i++) gGhosts[i] = 0;
    for (i = 0; i < 5; i++) nearGhosts[i] = 0;
    for (i = 1; i < 4; i++) gLastCar[i] = -1;
    if (gRace != kGhost) return;
    if (!gGhostSelect) {
        for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
            ghost = *ghosth;
            nextGhost = ghost->next;
            if (ghost->track == gTrackNum && ghost->backward == gBackwards &&
                !game_strcmp(ghost->name, player->name) &&
                player->id0 == ghost->id0 && player->id1 == ghost->id1) {
                if ((!best || ghost->finish < (*best)->finish) && ReadGhostData(ghosth)) best = ghosth;
            }
        }
        for (i = 0; i < 3; i++) {
            ghosth = limboGhosts + i;
            ghost = *ghosth;
            if (ghost->header && ghost->track == gTrackNum && ghost->backward == gBackwards &&
                !game_strcmp(ghost->name, player->name) &&
                player->id0 == ghost->id0 && player->id1 == ghost->id1) {
                if ((!best || ghost->finish < (*best)->finish) && ReadGhostData(ghosth)) best = ghosth;
            }
        }
        if (!best) {
            for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
                ghost = *ghosth;
                nextGhost = ghost->next;
                if (ghost->track == gTrackNum && ghost->backward == gBackwards) {
                    for (i = 3; i >= 1; i--) {
                        if ((!gGhosts[i] || ghost->finish > (*gGhosts[i])->finish) && ReadGhostData(ghosth)) {
                            for (j = 1; j < i; j++)
                                gGhosts[j] = gGhosts[j+1];
                            gGhosts[i] = ghosth;
                            break;
                        }
                    }
                }
            }
            for (ghosth = limboGhosts; ghosth < limboGhosts + 3; ghosth++) {
                ghost = *ghosth;
                if (ghost->header && ghost->track == gTrackNum && ghost->backward == gBackwards) {
                    for (i = 3; i >= 1; i--) {
                        if ((!gGhosts[i] || ghost->finish > (*gGhosts[i])->finish) && ReadGhostData(ghosth)) {
                            for (j = 1; j < i; j++)
                                gGhosts[j] = gGhosts[j+1];
                            gGhosts[i] = ghosth;
                            break;
                        }
                    }
                }
            }
            for (i = 1; i < 3; i++) {
                if (!gGhosts[i] && gGhosts[i+1]) {
                    gGhosts[i] = gGhosts[i+1];
                    gGhosts[i+1] = 0;
                    i = 0;
                }
            }
        } else {
            nearGhosts[2] = best;
            for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
                ghost = *ghosth;
                nextGhost = ghost->next;
                if (ghost->track == gTrackNum && ghost->backward == gBackwards) {
                    if (ghosth != best) {
                        if (ghost->finish < (*best)->finish) {
                            if (!nearGhosts[1] || ghost->finish > (*nearGhosts[1])->finish) {
                                if (ReadGhostData(ghosth)) {
                                    nearGhosts[0] = nearGhosts[1];
                                    nearGhosts[1] = ghosth;
                                }
                            } else if (!nearGhosts[0] || ghost->finish > (*nearGhosts[0])->finish) {
                                if (ReadGhostData(ghosth)) nearGhosts[0] = ghosth;
                            }
                        } else {
                            if (!nearGhosts[3] || ghost->finish < (*nearGhosts[3])->finish) {
                                if (ReadGhostData(ghosth)) {
                                    nearGhosts[4] = nearGhosts[3];
                                    nearGhosts[3] = ghosth;
                                }
                            } else if (!nearGhosts[4] || ghost->finish < (*nearGhosts[4])->finish) {
                                if (ReadGhostData(ghosth)) nearGhosts[4] = ghosth;
                            }
                        }
                    }
                }
            }
            for (ghosth = limboGhosts; ghosth < limboGhosts + 3; ghosth++) {
                ghost = *ghosth;
                if (!ghost->header || ghost->track != gTrackNum || ghost->backward != gBackwards || ghosth == best) continue;
                if (ghost->finish < (*best)->finish) {
                    if (!nearGhosts[1] || ghost->finish > (*nearGhosts[1])->finish) {
                        if (ReadGhostData(ghosth)) {
                            nearGhosts[0] = nearGhosts[1];
                            nearGhosts[1] = ghosth;
                        }
                    } else if (!nearGhosts[0] || ghost->finish > (*nearGhosts[0])->finish) {
                        if (ReadGhostData(ghosth)) nearGhosts[0] = ghosth;
                    }
                } else {
                    if (!nearGhosts[3] || ghost->finish < (*nearGhosts[3])->finish) {
                        if (ReadGhostData(ghosth)) {
                            nearGhosts[4] = nearGhosts[3];
                            nearGhosts[3] = ghosth;
                        }
                    } else if (!nearGhosts[4] || ghost->finish < (*nearGhosts[4])->finish) {
                        if (ReadGhostData(ghosth)) nearGhosts[4] = ghosth;
                    }
                }
            }
            ghostCount = 1;
            for (i = 0; i < 5; i++) {
                if (nearGhosts[i] && ghostCount < 4) { gGhosts[ghostCount] = nearGhosts[i]; ghostCount++; }
            }
        }
    } else {
        for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
            ghost = *ghosth;
            nextGhost = ghost->next;
            if (ghost->track == gTrackNum && ghost->backward == gBackwards) {
                for (i = 1; i < 4; i++) {
                    if ((!gGhosts[i] || ghost->finish < (*gGhosts[i])->finish) && ReadGhostData(ghosth)) {
                        for (j = 3; j > i; j--)
                            gGhosts[j] = gGhosts[j-1];
                        gGhosts[i] = ghosth;
                        break;
                    }
                }
            }
        }
        for (ghosth = limboGhosts; ghosth < limboGhosts + 3; ghosth++) {
            ghost = *ghosth;
            if (ghost->header && ghost->track == gTrackNum && ghost->backward == gBackwards) {
                for (i = 1; i < 4; i++) {
                    if ((!gGhosts[i] || ghost->finish < (*gGhosts[i])->finish) && ReadGhostData(ghosth)) {
                        for (j = 3; j > i; j--)
                            gGhosts[j] = gGhosts[j-1];
                        gGhosts[i] = ghosth;
                        break;
                    }
                }
            }
        }
    }
    if (!expansion_pak_present) {
        if (gGhosts[2]) { PurgeGhostData((void **)gGhosts[2]); gGhosts[2] = 0; }
        if (gGhosts[3]) { PurgeGhostData((void **)gGhosts[3]); gGhosts[3] = 0; }
    }
}

/* Set the screen up: choose the ghosts, load the screen's waves (62; with an Expansion Pak 80 and 61, without one 80
 * on tracks 6 and up, else 81), give each player the first option, unlocked, with the whole roster in carscr's
 * order (gCarOrder), make the 48 car matrices, and split the screen: a window per player (four for three), and in a
 * ghost race four windows (the player and three ghosts; two windows without an Expansion Pak). Each player gets a
 * hidden "waiting" box, a turntable (shown in every window in a ghost race), the buttons, and each car's settings
 * from the profile, stepped down to ones the player has unlocked (and saved back); the profile's car (or the next
 * unlocked one down), placed on the carousel; and the profile's transmission (BATTLE_TRANS in battle) for every
 * car, stepped down by two until it is unlocked. Then the text blit. */
void carscr_InitScreen(void)
{
    Player *pl;
    s32 p;
    s32 i;

    setGhosts();
    LoadWave(62, 0, 0, 0, 0);
    if (expansion_pak_present) {
        LoadWave(80, 0, 0, 0, 0);
        LoadWave(61, 0, 0, 0, 0);
    } else if (gTrackNum >= 6) {
        LoadWave(80, 0, 0, 0, 0);
    } else {
        LoadWave(81, 0, 0, 0, 0);
    }
    allLocked = 0;
    for (p = 0; p < gNumPlayers; p++) {
        carscr_gActiveOption[p] = 0;
        carscr_gActiveOptionEnabled[p] = 1;
        carscr_gFirstOption[p] = 1;
        gCarLocked[p] = 0;
        gHornSnd[p] = -1;
        calcStats(p);
    }
    for (p = 0; p < gNumPlayers; p++) {
        for (gNumCarsActive[p] = 0, i = 0; i < CARS; i++) {
            gCars[p][gNumCarsActive[p]++] = gCarOrder[i];
        }
    }
    for (i = 0; i < 48; i++) gCarMatrices[i] = ZOID_NewMatrix();
    SetOptions();
    gDriveHulk[0] = 0;
    gDriveHulk[1] = 0;
    gDriveHulk[2] = 0;
    gDriveHulk[3] = 0;
    HFOV = 90.0f;
    ZOID_ResetWindows();
    gNumWindows = gNumPlayers;
    if (gNumWindows == 3) gNumWindows = 4;
    if (gRace == kGhost) {
        gNumGhosts = 3;
        gNumWindows = 4;
        if (!expansion_pak_present) {
            gNumGhosts = 1;
            gNumWindows = 2;
        }
    }
    ZOID_SplitScreen(gNumWindows);
    if (gNumWindows == 1) {
        carscr_gButtonZoom = 1.0f;
        carscr_gButtonScale = 0.045f;
        carscr_gButtonSpace = 40.0f;
        carscr_buttonX = 0.0f;
        carscr_maxY = -5.4f;
        carscr_minY = -8.4f;
        carscr_buttonZ = 15.0f;
        carscr_cursorZ = 11.25f;
    } else {
        carscr_gButtonZoom = 1.0f;
        carscr_gButtonScale = 0.055f;
        carscr_gButtonSpace = 40.0f;
        carscr_buttonX = 0.0f;
        carscr_maxY = -3.45f;
        carscr_minY = -7.7f;
        carscr_buttonZ = 15.0f;
        carscr_cursorZ = 11.25f;
    }
    audio_interrupt_handler(1);
    InitVisuals(1);
    particle_lifetime_set();
    for (p = 0; p < 4; p++) carscr_pane[p] = InitPane(-20, 0, -10, 10, PANE_STYLE_MESSAGE, 0, 0, 0);
    for (p = 0; p < gNumPlayers; p++) {
        pl = &gPlayers[p];
        CopyUV(unitmatrix, gTurntableMat[p][0]);
        CopyUV(unitmatrix, gTurntableMat[p][1]);
        gTurntableIdx[p][0] = MBOX_NewObject(MBOX_FindObject_WORK(carscr_turntable_model_name, 0, gNumModels[0] - 1, 1), gTurntableMat[p][0], -1, 0x40000);
        gTurntableIdx[p][1] = MBOX_NewObject(MBOX_FindObject_WORK(carscr_turntable_base_model_name, 0, gNumModels[0] - 1, 1), gTurntableMat[p][1], -1, 0);
        MBOX_HideObject(gTurntableIdx[p][0], 1, 15);
        MBOX_HideObject(gTurntableIdx[p][1], 1, 15);
        if (gRace == kGhost) {
            MBOX_ShowObject(gTurntableIdx[p][0], 0, 15);
            MBOX_ShowObject(gTurntableIdx[p][1], 0, 15);
        } else {
            MBOX_ShowObject(gTurntableIdx[p][0], 0, 1 << p);
            MBOX_ShowObject(gTurntableIdx[p][1], 0, 1 << p);
        }
        gTurntableMat[p][0][9] = 0.0;
        gTurntableMat[p][0][10] = 2.0f;
        gTurntableMat[p][0][11] = 15.0f;
        gTurntableMat[p][1][9] = 0.0;
        gTurntableMat[p][1][10] = 2.0f;
        gTurntableMat[p][1][11] = 15.0f;
        carscr_initButtons(p);
        for (i = 0; i < CARS; i++) {
            gCarHandling[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_HANDLING);
            gCarEngines[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_ENGINE);
            while (!IsEngineUnlocked(p, gCarEngines[p + 1][i])) {
                gCarEngines[p + 1][i]--;
                if (gCarEngines[p + 1][i] < 0) { gCarEngines[p + 1][i] = 0; break; }
            }
            SavePlayerCarSetting(pl->profile, i, CAR_SETTING_ENGINE, gCarEngines[p + 1][i]);
            gCarTires[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_TIRES);
            while (!IsTiresUnlocked(p, gCarTires[p + 1][i])) {
                gCarTires[p + 1][i]--;
                if (gCarTires[p + 1][i] < 0) { gCarTires[p + 1][i] = 0; break; }
            }
            SavePlayerCarSetting(pl->profile, i, CAR_SETTING_TIRES, gCarTires[p + 1][i]);
            gCarFrame[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_FRAME);
            while (!IsFrameUnlocked(p, gCarFrame[p + 1][i])) {
                gCarFrame[p + 1][i]--;
                if (gCarFrame[p + 1][i] < 0) { gCarFrame[p + 1][i] = 0; break; }
            }
            SavePlayerCarSetting(pl->profile, i, CAR_SETTING_FRAME, gCarFrame[p + 1][i]);
            gCarWings[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_WINGS);
            if (gRace != kStunt && gCarWings[p + 1][i] > 1) gCarWings[p + 1][i] = 1;
            car_color_choice_a[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_COLOR_A);
            car_color_choice_b[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_COLOR_B);
            car_color_choice_c[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_COLOR_C);
            TIRE_RIMS(p + 1)[i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_RIMS);
            gCarHorn[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_HORN);
            gFrontTireScale[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_FRONT_TIRE_SCALE) / 100.0f + 0.75f;
            gRearTireScale[p + 1][i] = GetPlayerCarSetting(pl->profile, i, CAR_SETTING_REAR_TIRE_SCALE) / 100.0f + 0.75f;
            if (!gEnableTireScaling) {
                gFrontTireScale[p + 1][i] = gFrontTireScale[0][i];
                gRearTireScale[p + 1][i] = gRearTireScale[0][i];
            }
        }
        gThisCar[p] = GetPlayerCar(pl->profile);
        while (!IsCarUnlocked(p, gThisCar[p])) {
            if (!gThisCar[p]) gThisCar[p] = 12;
            else gThisCar[p]--;
        }
        for (gThisCarIndex[p] = 0; gThisCarIndex[p] < gNumCarsActive[p]; gThisCarIndex[p]++) {
            if (gThisCar[p] == gCars[p][gThisCarIndex[p]]) break;
        }
        if (gThisCarIndex[p] >= gNumCarsActive[p]) {
            gThisCarIndex[p] = p + 1;
            gThisCar[p] = gCars[p][gThisCarIndex[p]];
            SavePlayerCar(pl->profile, gThisCar[p]);
        }
        gLastCar[p] = -1;
        calcValidOptions(p);
        for (i = 0; i < gNumCarsActive[p]; i++) {
            carscr_cars[p][i].position = i - gThisCarIndex[p];
            if (carscr_cars[p][i].position < 0) carscr_cars[p][i].position += gNumCarsActive[p];
            carscr_cars[p][i].car = gCars[p][i] + p * CARS;
            carscr_cars[p][i].angle = 3.1415927f;
        }
        if (gRace == kBattle) gPlayers[p].trans = BATTLE_TRANS;
        else {
            gPlayers[p].trans = GetPlayerCarTrans((void **)pl->profile);
            while (!IsTransUnlocked(p, gPlayers[p].trans)) {
                gPlayers[p].trans -= 2;
                if ((s32)gPlayers[p].trans < 0) { gPlayers[p].trans += 2; break; }
            }
        }
        for (i = 0; i < CARS; i++)
            gCarTrans[p + 1][i] = gPlayers[p].trans;
    }
    carscr_blit = NewMultiBlit(0, 0, carscr_mb, 38);
    ZeroCameras();
    UpdateCameras();
    ClearRushControls();
    carscr_inited = 1;
}

/* PC: game.frame_rate 60 (pc_game_fps.h). The car select counts the frames since every player was ready (carscr_wait)
 * and leaves at 10, turning the cars to the front over them: once a 30 Hz frame, and the turn's share of what is
 * left, 1 / (frames left - 1) a frame, taken to the frame's length (PC_EASE_DIV(a, n): a / n at original). */
#define PC_EASE_DIV(a, n) (pc_fps60 ? (a) * (1.0f - pc_fps_filter(1.0f - 1.0f / (n))) : (a) / (n))

/* The screen's frame.
 * - Each frame the ghosts still chosen must be on a pak that is in, or in memory; after a race with a screen to skip
 *   to the game goes straight on (setGhosts, GS_GAME_INIT). The first frame sets the screen up. The cars are drawn
 *   (showCarSelect). A profile's pak pulled sends everyone back to the player screen; a soft reset to the title.
 * - Each player, while not everyone is locked in: B unlocks the player's car, or if it isn't locked goes back to the
 *   track screen. A locked player sees a box saying who is still choosing. Start or A locks the car in
 *   and fills the player's link record (the car, its colours: the team's in battle). Up and down move through the
 *   offered options; left and right change the option (the car, transmission, handling, engine, tyres, frame and
 *   wings stepping over locked ones, the colours, the team (never all players on one), the rims, the tyre sizes, a
 *   ghost), each change saved in the profile at once. The list scrolls to keep the option second from the top.
 * - A ghost race sizes the ghost windows' name boxes.
 * - Once every player is locked in for CAR_READY_FRAMES frames: the music fades, the race is set up (GS_GAME_INIT),
 *   a circuit goes on (ContinueCircuit), a ghost race drops the ghosts' data it won't race and closes the gaps in
 *   gGhosts, and the controls are applied. */
void updateCarScreen(void)
{
    s16 i;
    s16 p;
    s32 sound = 1;
    Record **ghosth;
    Record *ghost;
    Record **nextGhost;

    if (!expansion_pak_present) limboGhosts = gLimboGhosts;
    else limboGhosts = gLimboGhosts + ((gBackwards ? 6 : 0) + gTrackNum) * 3;
    if (gSkipToGameState != SKIP_NONE) {
        if (gSkipToGameState != gGameState) { setGhosts(); setGameState(GS_GAME_INIT); return; }
        gSkipToGameState = SKIP_NONE;
    }
    for (i = 1; i < 4; i++) {
        for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
            ghost = *ghosth;
            nextGhost = ghost->next;
            if (ghost->fileh && !PS_PakInserted((*ghost->fileh)->pak)) continue;
            if (ghosth == gGhosts[i]) break;
        }
        if (!ghosth) {
            for (p = 0; p < 3; p++) {
                ghosth = limboGhosts + p;
                if (ghosth == gGhosts[i]) break;
                ghosth = 0;
            }
        }
        gGhosts[i] = ghosth;
    }
    if (!carscr_inited) carscr_InitScreen();
    showCarSelect();
    for (i = 0; i < gNumPlayers; i++) {
        if ((*gPlayers[i].profile)->fileh && PS_PakChanged((*(*gPlayers[i].profile)->fileh)->pak)) {
            carscr_UninitScreen(); setGameState(GS_PLAYER); carscr_wait = 0; return;
        }
    }
    if (gSoftReset) { carscr_UninitScreen(); setGameState(GS_TITLE); carscr_wait = 0; return; }
    PC_EACH30(carscr_wait++;)
    for (p = 0; p < gNumPlayers; p++) {
        s32 notReadyCount;
        s32 notReadyIdx;
        s32 pIdx;
        s16 x, y, w, h;
        s16 screenPos[2];
        CarScreenMsgPos msgPos;
        s32 ghostIdx;
        s16 gx, gy, gw, gh;
        f32 pos[3];
        s16 gscreenPos[2];
        if (!allLocked && (gPlayers[p].pressed & BUTTON_B)) {
            if (gCarLocked[p] == 1) gCarLocked[p] = 0;
            else { AudPlayEffect(SND_BACK, 0, 1, 0); carscr_UninitScreen(); setGameState(GS_TRACK); carscr_wait = 0; return; }
        }
        if (gCarLocked[p]) {
            msgPos = carscr_msgPos;
            if (gNumWindows == 1) HAL_SetFont(13); else HAL_SetFont(12);
            ZOID_ProjectWindow(p, msgPos.v, &gCamera[p], 0, screenPos);
            x = screenPos[0];
            y = screenPos[1] - TB_GetFontLineHeight();
            notReadyCount = 0;
            for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
                if (pIdx != p && !gCarLocked[pIdx]) { notReadyIdx = pIdx; notReadyCount++; }
            }
            if (!notReadyCount) {
                y += TB_GetFontLineHeight();
                h = TB_GetFontLineHeight();
                w = HAL_GetTextWidth(carscr_ready_text, -1);
            } else {
                w = HAL_GetTextWidth(gText.single[TEXT_WAITING_FOR], -1);
                h = TB_GetFontLineHeight() * 2;
                if (notReadyCount == 1) w = (w > HAL_GetTextWidth((*gPlayers[notReadyIdx].profile)->name, -1) ? w : HAL_GetTextWidth((*gPlayers[notReadyIdx].profile)->name, -1));
                else w = (w > HAL_GetTextWidth(gText.single[TEXT_WAITING_FOR_ALL], -1) ? w : HAL_GetTextWidth(gText.single[TEXT_WAITING_FOR_ALL], -1));
            }
            y -= TB_GetFontLineHeight() / 2;
            ChangePaneSize(carscr_pane[p], x - w / 2 - 4, y - 4, x + w / 2 + 4, y + h + 4);
            carscr_updateButtons(p);
            continue;
        }
        ChangePaneSize(carscr_pane[p], -20, 0, -10, 10);
        if (gPlayers[p].pressed & BUTTON_ACCEPT) {
            PlayStartABSound(gPlayers[p].pressed);
            gCarLocked[p] = 1;
            PC_LINK_FLAGS(p) = 160;     /* 0xA0: 0x80 in the game (MAT3 LINK_FLAG_IN_GAME), and 0x20 */
            PC_LINK_CAR(p) = gThisCar[p];
            if (gRace == kBattle && gCheatState[CHEAT_BATTLE_PAINT] == 0) {
                PC_LINK_COLOR0(p) = gCarTeamColor[gCarTeam[p]];
                PC_LINK_COLOR1(p) = gCarTeamColor[gCarTeam[p]];
                PC_LINK_COLOR2(p) = gCarTeamColor[gCarTeam[p]];
            } else {
                PC_LINK_COLOR0(p) = car_color_choice_a[p + 1][gThisCar[p]];
                PC_LINK_COLOR1(p) = car_color_choice_b[p + 1][gThisCar[p]];
                PC_LINK_COLOR2(p) = car_color_choice_c[p + 1][gThisCar[p]];
            }
            carscr_wait = 0;
        }
        if (gPlayers[p].repeated & BUTTON_UP) {
            AudPlayEffect(SND_MENU_UP, 0, 1, 0);
            carscr_gActiveOption[p]--;
            if (carscr_gActiveOption[p] < 0) carscr_gActiveOption[p] = carscr_gNumOptions[p] - 1;
        } else if (gPlayers[p].repeated & BUTTON_DOWN) {
            AudPlayEffect(SND_MENU_DOWN, 0, 1, 0);
            carscr_gActiveOption[p]++;
            if (carscr_gActiveOption[p] > carscr_gNumOptions[p] - 1) carscr_gActiveOption[p] = 0;
        } else if (gPlayers[p].repeated & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_DEFAULT_OPTION)) {
            /* d: -1 left, 1 right, 0 the option's default */
            s32 d = (gPlayers[p].repeated & BUTTON_LEFT) ? -1 : ((gPlayers[p].repeated & BUTTON_RIGHT) ? 1 : 0);
            /* Original game: nothing sets carscr_gActiveZ (MAT3 neither), so the default button only ever plays the
             * error sound, and every `else ... = [0][car]` (the default) below is unreachable. */
            if (!carscr_gActiveOptionEnabled[p] || (!d && !carscr_gActiveZ[p])) {
                AudPlayEffect(SND_MENU_ERROR, 0, 1, 0);
                goto after_options;
            }
            switch (carscr_gOpts[p][carscr_gActiveOption[p]]) {
            case CAR_OPT_CAR:
                if (d) {
                    do {
                        gThisCarIndex[p] += d;
                        if (gThisCarIndex[p] < 0) gThisCarIndex[p] = gNumCarsActive[p] - 1;
                        else if (gThisCarIndex[p] >= gNumCarsActive[p]) gThisCarIndex[p] = 0;
                    } while (!IsCarUnlocked(p, gCars[p][gThisCarIndex[p]]));
                }
                gThisCar[p] = gCars[p][gThisCarIndex[p]];
                if (gRace == kBattle) gPlayers[p].trans = BATTLE_TRANS;
                else gPlayers[p].trans = GetPlayerCarTrans((void **)gPlayers[p].profile);
                for (i = 0; i < CARS; i++) gCarTrans[p + 1][i] = gPlayers[p].trans;
                calcValidOptions(p);
                carscr_buttonsJustInited[p] = 1;
                break;
            case CAR_OPT_TRANS:
                if (gRace != kBattle) {
                    if (d) {
                        do {
                            gCarTrans[p + 1][gThisCar[p]] += d;
                            if (gCarTrans[p + 1][gThisCar[p]] < 0) gCarTrans[p + 1][gThisCar[p]] = 7;
                            else if (gCarTrans[p + 1][gThisCar[p]] >= 8) gCarTrans[p + 1][gThisCar[p]] = 0;
                        } while (!IsTransUnlocked(p, gCarTrans[p + 1][gThisCar[p]]) || (gRace == kBattle && (gCarTrans[p + 1][gThisCar[p]] & 1)));
                    } else gCarTrans[p + 1][gThisCar[p]] = gCarTrans[0][gThisCar[p]];
                    gPlayers[p].trans = gCarTrans[p + 1][gThisCar[p]];
                    SavePlayerCarTrans(gPlayers[p].profile, gPlayers[p].trans);
                } else sound = 0;
                break;
            case CAR_OPT_HANDLING:
                if (d) {
                    gCarHandling[p + 1][gThisCar[p]] += d;
                    if (gCarHandling[p + 1][gThisCar[p]] < 0) gCarHandling[p + 1][gThisCar[p]] = 2;
                    else if (gCarHandling[p + 1][gThisCar[p]] > 2) gCarHandling[p + 1][gThisCar[p]] = 0;
                } else gCarHandling[p + 1][gThisCar[p]] = gCarHandling[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_HANDLING, gCarHandling[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_ENGINE:
                if (d) {
                    do {
                        gCarEngines[p + 1][gThisCar[p]] += d;
                        if (gCarEngines[p + 1][gThisCar[p]] < 0) gCarEngines[p + 1][gThisCar[p]] = 8;
                        else if (gCarEngines[p + 1][gThisCar[p]] > 8) gCarEngines[p + 1][gThisCar[p]] = 0;
                    } while (!IsEngineUnlocked(p, gCarEngines[p + 1][gThisCar[p]]));
                } else gCarEngines[p + 1][gThisCar[p]] = gCarEngines[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_ENGINE, gCarEngines[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_TIRES:
                if (d) {
                    do {
                        gCarTires[p + 1][gThisCar[p]] += d;
                        if (gCarTires[p + 1][gThisCar[p]] < 0) gCarTires[p + 1][gThisCar[p]] = 4;
                        else if (gCarTires[p + 1][gThisCar[p]] > 4) gCarTires[p + 1][gThisCar[p]] = 0;
                    } while (!IsTiresUnlocked(p, gCarTires[p + 1][gThisCar[p]]));
                } else gCarTires[p + 1][gThisCar[p]] = gCarTires[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_TIRES, gCarTires[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_FRAME:
                if (d) {
                    do {
                        gCarFrame[p + 1][gThisCar[p]] += d;
                        if (gCarFrame[p + 1][gThisCar[p]] < 0) gCarFrame[p + 1][gThisCar[p]] = 5;
                        else if (gCarFrame[p + 1][gThisCar[p]] > 5) gCarFrame[p + 1][gThisCar[p]] = 0;
                    } while (!IsFrameUnlocked(p, gCarFrame[p + 1][gThisCar[p]]));
                } else gCarFrame[p + 1][gThisCar[p]] = gCarFrame[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_FRAME, gCarFrame[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_WINGS:
                if (d) {
                    gCarWings[p + 1][gThisCar[p]] += d;
                    if (gCarWings[p + 1][gThisCar[p]] < 0) gCarWings[p + 1][gThisCar[p]] = (gRace != kStunt ? 1 : 2);
                    else if (gCarWings[p + 1][gThisCar[p]] > (gRace != kStunt ? 1 : 2)) gCarWings[p + 1][gThisCar[p]] = 0;
                } else gCarWings[p + 1][gThisCar[p]] = gCarWings[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_WINGS, gCarWings[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_COLOR_A:
                if (d) {
                    car_color_choice_a[p + 1][gThisCar[p]] += d;
                    if (car_color_choice_a[p + 1][gThisCar[p]] < 0) car_color_choice_a[p + 1][gThisCar[p]] = 31;
                    else if (car_color_choice_a[p + 1][gThisCar[p]] >= 32) car_color_choice_a[p + 1][gThisCar[p]] = 0;
                } else car_color_choice_a[p + 1][gThisCar[p]] = car_color_choice_a[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_COLOR_A, car_color_choice_a[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_COLOR_B:
                if (d) {
                    car_color_choice_b[p + 1][gThisCar[p]] += d;
                    if (car_color_choice_b[p + 1][gThisCar[p]] < 0) car_color_choice_b[p + 1][gThisCar[p]] = 31;
                    else if (car_color_choice_b[p + 1][gThisCar[p]] >= 32) car_color_choice_b[p + 1][gThisCar[p]] = 0;
                } else car_color_choice_b[p + 1][gThisCar[p]] = car_color_choice_b[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_COLOR_B, car_color_choice_b[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_COLOR_C:
                if (d) {
                    car_color_choice_c[p + 1][gThisCar[p]] += d;
                    if (car_color_choice_c[p + 1][gThisCar[p]] < 0) car_color_choice_c[p + 1][gThisCar[p]] = 31;
                    else if (car_color_choice_c[p + 1][gThisCar[p]] >= 32) car_color_choice_c[p + 1][gThisCar[p]] = 0;
                } else car_color_choice_c[p + 1][gThisCar[p]] = car_color_choice_c[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_COLOR_C, car_color_choice_c[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_TEAM:
                if (d) {
                    s32 teamFull = 1;
                    do {
                        gCarTeam[p] += d;
                        if (gCarTeam[p] < 0) gCarTeam[p] = 3;
                        else if (gCarTeam[p] > 3) gCarTeam[p] = 0;
                        for (i = 0; i < gNumPlayers; i++) {
                            if (i != p && gCarTeam[i] != gCarTeam[p]) teamFull = 0;
                        }
                    } while (teamFull == 1);
                } else gCarTeam[p] = p;
                break;
            case CAR_OPT_RIMS:
                if (d) {
                    TIRE_RIMS(p + 1)[gThisCar[p]] += d;
                    if (TIRE_RIMS(p + 1)[gThisCar[p]] < 0) TIRE_RIMS(p + 1)[gThisCar[p]] = 20;
                    else if (TIRE_RIMS(p + 1)[gThisCar[p]] > 20) TIRE_RIMS(p + 1)[gThisCar[p]] = 0;
                } else TIRE_RIMS(p + 1)[gThisCar[p]] = TIRE_RIMS(0)[gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_RIMS, TIRE_RIMS(p + 1)[gThisCar[p]]);
                break;
            case CAR_OPT_HORN:
                if (d) {
                    gCarHorn[p + 1][gThisCar[p]] += d;
                    if (gCarHorn[p + 1][gThisCar[p]] < 0) gCarHorn[p + 1][gThisCar[p]] = 9;
                    else if (gCarHorn[p + 1][gThisCar[p]] > 9) gCarHorn[p + 1][gThisCar[p]] = 0;
                } else gCarHorn[p + 1][gThisCar[p]] = gCarHorn[0][gThisCar[p]];
                sound = 0;
                AudStopEffect(gHornSnd[p]);
                gHornSnd[p] = AudPlayEffect(gCarHorn[p + 1][gThisCar[p]] + SND_HORNS, 0, 1, 0);
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_HORN, gCarHorn[p + 1][gThisCar[p]]);
                break;
            case CAR_OPT_FRONT_TIRE_SCALE:
                if (d) {
                    gFrontTireScale[p + 1][gThisCar[p]] += d * 0.05f;
                    if (gFrontTireScale[p + 1][gThisCar[p]] < 0.74f) gFrontTireScale[p + 1][gThisCar[p]] = 1.5f;
                    else if (gFrontTireScale[p + 1][gThisCar[p]] > 1.6f) gFrontTireScale[p + 1][gThisCar[p]] = 0.75f;
                } else gFrontTireScale[p + 1][gThisCar[p]] = gFrontTireScale[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_FRONT_TIRE_SCALE, (s8)(100.0f * gFrontTireScale[p + 1][gThisCar[p]] - 75.0f));
                break;
            case CAR_OPT_REAR_TIRE_SCALE:
                if (d) {
                    gRearTireScale[p + 1][gThisCar[p]] += d * 0.05f;
                    if (gRearTireScale[p + 1][gThisCar[p]] < 0.74f) gRearTireScale[p + 1][gThisCar[p]] = 1.5f;
                    else if (gRearTireScale[p + 1][gThisCar[p]] > 1.5f) gRearTireScale[p + 1][gThisCar[p]] = 0.75f;
                } else gRearTireScale[p + 1][gThisCar[p]] = gRearTireScale[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].profile, gThisCar[p], CAR_SETTING_REAR_TIRE_SCALE, (s8)(100.0f * gRearTireScale[p + 1][gThisCar[p]] - 75.0f));
                break;
            case CAR_OPT_SHEEN:
                if (d) {
                    gCarSheen[p] += d;
                    if (gCarSheen[p] < 0) gCarSheen[p] = 10;
                    else if (gCarSheen[p] > 10) gCarSheen[p] = 0;
                } else gCarSheen[p] = 0;
                SetSheen(gThisCar[p], p, gCarSheen[p]);
                break;
            case CAR_OPT_GHOST_1: gGhosts[1] = getNextGhost(gGhosts[1], d); break;
            case CAR_OPT_GHOST_2: gGhosts[2] = getNextGhost(gGhosts[2], d); break;
            case CAR_OPT_GHOST_3: gGhosts[3] = getNextGhost(gGhosts[3], d); break;
            }
            if (sound) PlayLRSound(gPlayers[p].repeated);
        }
        if (carscr_gFirstOption[p] > carscr_gActiveOption[p] - 1) carscr_gFirstOption[p] = carscr_gActiveOption[p] - 1;
        if (carscr_gFirstOption[p] < carscr_gActiveOption[p] - 1) carscr_gFirstOption[p] = carscr_gActiveOption[p] - 1;
        if (carscr_gFirstOption[p] > carscr_gNumOptions[p] - 3) carscr_gFirstOption[p] = carscr_gNumOptions[p] - 3;
        if (carscr_gFirstOption[p] < 1) carscr_gFirstOption[p] = 1;
        carscr_gActiveOptionEnabled[p] = 1;
after_options:
        if (gLastCar[p] != gThisCar[p]) {
            if (gLastCar[p] >= 0) SavePlayerCar(gPlayers[p].profile, gThisCar[p]);
            gLastCar[p] = gThisCar[p];
        }
        if (gRace == kGhost) {
            for (ghostIdx = 1; ghostIdx < gNumGhosts + 1; ghostIdx++) {
                if (carscr_pane[ghostIdx]) {
                    pos[0] = carscr_buttonInfo[0][CAR_BUTTON_TITLE].position[0];
                    pos[1] = carscr_buttonInfo[0][CAR_BUTTON_TITLE].position[1];
                    pos[2] = carscr_buttonInfo[0][CAR_BUTTON_TITLE].position[2];
                    pos[1] = -pos[1] / 2.0f;
                    ZOID_ProjectWindow(ghostIdx, pos, &gCamera[ghostIdx], 0, gscreenPos);
                    gx = gscreenPos[0];
                    gy = gscreenPos[1];
                    HAL_SetFont(12);
                    if (!gGhosts[ghostIdx]) {
                        if (!ghosts_enabled) gw = HAL_GetTextWidth(gText.single[TEXT_GHOSTS_OFF], -1);
                        else gw = HAL_GetTextWidth(gText.single[TEXT_NO_GHOSTS_SAVED], -1);
                        gh = TB_GetFontLineHeight();
                    } else {
                        gw = HAL_GetTextWidth((*gGhosts[ghostIdx])->name, -1);
                        gw = (gw > HAL_GetTextWidth(carscr_ghost_width_text_first, -1) ? gw : HAL_GetTextWidth(carscr_ghost_width_text_second, -1));
                        gh = TB_GetFontLineHeight() * 2;
                    }
                    gy -= TB_GetFontLineHeight() / 2;
                    ChangePaneSize(carscr_pane[ghostIdx], gx - gw / 2 - 4, gy - 4, gx + gw / 2 + 4, gy + gh + 4);
                }
            }
        }
        carscr_updateButtons(p);
        calcStats(p);
    }
    allLocked = 1;
    for (i = 0; i < gNumPlayers; i++) if (!gCarLocked[i]) allLocked = 0;
    if (allLocked && carscr_wait >= CAR_READY_FRAMES) {
        AudSetMusicVol(0.0f, 3.0f);
        setGameState(GS_GAME_INIT);
        carscr_UninitScreen();
        if (gRace == kCircuit) ContinueCircuit();
        if (gRace == kGhost) {
            {
                Record **gh;
                for (gh = gGhostFiles.first; gh; gh = (*gh)->next) PurgeGhostData((void **)gh);
            }
            for (i = 1; i < gNumGhosts; i++) {
                if (!gGhosts[i]) { gGhosts[i] = gGhosts[i + 1]; gGhosts[i + 1] = 0; }
            }
            for (i = 1; i < gNumGhosts; i++) {
                if (!gGhosts[i]) { gGhosts[i] = gGhosts[i + 1]; gGhosts[i + 1] = 0; }
            }
        }
        SetControls(0);
    }
}

/* Is option item (CAR_OPT_*) offered to player? Not: the tyre sizes without gEnableTireScaling; without an
 * Expansion Pak the rims on tracks 0-5 and the second and third ghosts; the ghosts unless it is a ghost race with
 * ghosts on; the wings in ghost races and battles; the colours in a battle (unless the cheat keeps the paint) and
 * each colour the car's paint hasn't (carscr_color_option_masks); the team outside battles; the horn and the sheen
 * ever. 1 offered, 0 not. */
s32 carscr_validOption(s32 player, s32 item)
{
    s32 available = 1;
    if ((item == CAR_OPT_FRONT_TIRE_SCALE || item == CAR_OPT_REAR_TIRE_SCALE) && !gEnableTireScaling) available = 0;
    if (!expansion_pak_present) {
        if (item == CAR_OPT_RIMS && gTrackNum >= 0 && gTrackNum < 6) available = 0;
        if (item == CAR_OPT_GHOST_2 || item == CAR_OPT_GHOST_3) available = 0;
    }
    if (!ghosts_enabled && (item == CAR_OPT_GHOST_1 || item == CAR_OPT_GHOST_2 || item == CAR_OPT_GHOST_3)) available = 0;
    if (item == CAR_OPT_WINGS && (gRace == kGhost || gRace == kBattle)) available = 0;
    if ((item == CAR_OPT_COLOR_A || item == CAR_OPT_COLOR_B || item == CAR_OPT_COLOR_C) && gRace == kBattle && !gCheatState[CHEAT_BATTLE_PAINT]) available = 0;
    if (item == CAR_OPT_COLOR_A && !(carscr_color_option_masks[gThisCar[player]] & 1)) available = 0;
    if (item == CAR_OPT_COLOR_B && !(carscr_color_option_masks[gThisCar[player]] & 2)) available = 0;
    if (item == CAR_OPT_COLOR_C && !(carscr_color_option_masks[gThisCar[player]] & 4)) available = 0;
    if (item == CAR_OPT_TEAM && gRace != kBattle) available = 0;
    if ((item == CAR_OPT_GHOST_1 || item == CAR_OPT_GHOST_2 || item == CAR_OPT_GHOST_3) && gRace != kGhost) available = 0;
    if (item == CAR_OPT_HORN || item == CAR_OPT_SHEEN) available = 0;
    return available;
}

/* List player's offered options in order (carscr_gOpts, carscr_gNumOptions). */
void calcValidOptions(s32 player)
{
    s32 item;
    carscr_gNumOptions[player]=0; for(item=0;item<CAR_OPTS;item++) {
        if(carscr_validOption(player,item))carscr_gOpts[player][carscr_gNumOptions[player]++]=item;
    }
}

/* The next ghost after node (direction 1) or before it (-1) for a ghost slot: one of this track and direction, not
 * already in a slot, whose data is in memory (or can be read from its pak). The ghosts are walked as one list, the
 * paks' files then the three in memory, wrapping round; from no ghost the walk starts at either end. NULL if none
 * qualifies. */
Record **getNextGhost(Record **node,s8 direction)
{
    Record **next;
    Record *record;
    if(!node) {
        if(direction>0)node=gGhostFiles.first;
        else node=limboGhosts+2;
    }
    while(node) {
        record=*node;
        if(direction>0)next=record->next;
        else if(node==gGhostFiles.first)next=0;
        else {
            for(next=gGhostFiles.first;next && (*next)->next!=node;next=(*next)->next) {}
        }
        if(record->track==gTrackNum && record->backward==gBackwards &&
           node!=gGhosts[1] && node!=gGhosts[2] && node!=gGhosts[3] &&
           (!(*node)->fileh || ReadGhostData(node)) && (*node)->datah)break;
        if(!next) {
            if(node==limboGhosts) {
                if(direction>0)next=limboGhosts+1;
                else next=gGhostFiles.last;
            } else if(node==limboGhosts+1) {
                if(direction>0)next=limboGhosts+2;
                else next=limboGhosts;
            } else if(node==limboGhosts+2) {
                if(direction<0)next=limboGhosts+1;
            } else if(direction>0)next=limboGhosts;
        }
        node=next;
    }
    return node;
}

typedef struct CarComponent { s16 idx, level; f32 uvs[9]; f32 pos[3]; } CarComponent;

extern void sound_handles_array_clear(FiveParts *);

extern void RemoveBlit(Blit *);

/* Take the screen down: its waves, the grid backdrop, the "waiting" boxes, each player's buttons, turntable and part
 * models, the text blit and the background; one full-screen window again, and the horns stopped. */
void carscr_UninitScreen(void)
{
    s32 player,count;
    FiveParts **box;
    UnloadWaveAll(62);
    if(gNumPlayers==1 && gRace!=kGhost) releaseGridMap();
    if(expansion_pak_present) {
        UnloadWaveAll(80);
        UnloadWaveAll(61);
    } else if(gTrackNum>=6)UnloadWaveAll(80);
    else UnloadWaveAll(81);
    for(box=carscr_pane;box<carscr_pane+4;box++) {
        if(*box) {sound_handles_array_clear(*box);*box=0;}
    }
    for(player=0;player<(count=gNumPlayers);player++) {
        carscr_uninitButtons(player);
        if(gTurntableIdx[player][0]!=-1) {ZOID_RemoveObject(gTurntableIdx[player][0]);gTurntableIdx[player][0]=-1;}
        if(gTurntableIdx[player][1]!=-1) {ZOID_RemoveObject(gTurntableIdx[player][1]);gTurntableIdx[player][1]=-1;}
        clearCarComponents(player);
    }
    if(carscr_blit) {RemoveBlit(carscr_blit);carscr_blit=0;}
    removeBG();
    HFOV=90.0f;gNumWindows=1;
    ZOID_ResetWindows();ZOID_Reset();
    for(player=0;player<gNumPlayers;player++)AudStopEffect(gHornSnd[player]);
    carscr_inited=0;
}

/* Remove the part models updateCarComponents shows (player 0 only, the only one that has them) and their frame. */
void clearCarComponents(s32 p)
{
    s32 i;
    if (p > 0) return;
    for(i=0;i!=11;i++) {
        if(gCarComponents[p][i].idx!=-1) {ZOID_RemoveObject(gCarComponents[p][i].idx);gCarComponents[p][i].idx=-1;gCarComponents[p][i].level=-1;}
    }
    if(gCarPartFrame!=-1) {ZOID_RemoveObject(gCarPartFrame);gCarPartFrame=-1;}
}

/* Empty: MAT3 frees the grid backdrop createGridMap builds; the N64 has none. */
void releaseGridMap(void)
{
}

/* Remove player p's buttons from the scene. */
void carscr_uninitButtons(s8 p)
{
    s32 i;
    MenuButton *slot;
    for(i=0;i!=CAR_BUTTONS;i++) {
        slot=&carscr_buttonInfo[p][i];
        if(slot->handle!=-1) {ZOID_RemoveObject(slot->handle);slot->handle=-1;}
    }
    carscr_buttonsInited[p]=0;
}

typedef union Color4 {u8 c[4];u32 word;} Color4;

extern void *memcpy(void *,const void *,u32);

extern void *MBOX_FindTexture_WORK(char *,u16 *,s32,s8,s32);
extern void ZOID_SetTexture(s16,void *,s16);

/* Make player p's buttons (carscr_buttonInfo[p], 64-byte MAT3 tMenuButton records): each that doesn't exist yet is
 * its type's model (carscr_buttonNames; types 0-2 see-through, 0x42000), white, shown only in p's window. Then the
 * title (shown in every window in a ghost race), the cursor, and the A and B icons with their arrows at the
 * window's foot, a little lower with more than one window. MAT3 has 40 buttons (17 options a side); the N64 44 (19
 * a side). zoom is MAT3's function-local static; it stays 1. */
void carscr_initButtons(s8 p)
{
    Color4 buttonColor={{255,255,255,255}};
    void *tex;
    u16 texIdx;
    s32 buttonIdx;
    static f32 zoom=1.0f;
    if(gNumWindows==1)zoom=1.0;
    for(buttonIdx=0;buttonIdx<CAR_BUTTONS;buttonIdx++) {
        if(carscr_buttonInfo[p][buttonIdx].handle==-1) {
            memcpy(carscr_buttonInfo[p][buttonIdx].matrix,unitmatrix,36);
            carscr_buttonInfo[p][buttonIdx].handle=MBOX_NewObject(
                MBOX_FindObject_WORK(carscr_buttonNames[carscr_buttonInfo[p][buttonIdx].type],0,gNumModels[0]-1,1),
                carscr_buttonInfo[p][buttonIdx].matrix,-1,
                (carscr_buttonInfo[p][buttonIdx].type==0 || carscr_buttonInfo[p][buttonIdx].type==1 || carscr_buttonInfo[p][buttonIdx].type==2) ? 0x42000 : 0);
            ZOID_SetColor((s16)carscr_buttonInfo[p][buttonIdx].handle,&buttonColor);
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx].handle,1,15);
            MBOX_ShowObject(carscr_buttonInfo[p][buttonIdx].handle,0,1<<p);
        }
    }
    tex=MBOX_FindTexture_WORK(carscr_title_texture_name,&texIdx,0,gNumModels[0]-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][CAR_BUTTON_TITLE].handle,tex,-1);
    memcpy(carscr_buttonInfo[p][CAR_BUTTON_TITLE].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][CAR_BUTTON_TITLE].matrix);
    carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[0]=0.0f;
    carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[1]=gNumWindows>1 ? 105.0f : 115.0f;
    carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[2]=200.0f;
    carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[0]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[0];
    carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[1]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[1];
    carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[2]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_TITLE].position[2];
    if(gRace==kGhost)MBOX_ShowObject(carscr_buttonInfo[p][CAR_BUTTON_TITLE].handle,0,15);
    tex=MBOX_FindTexture_WORK(carscr_cursor_texture_name,&texIdx,0,gNumModels[0]-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][CAR_BUTTON_CURSOR].handle,tex,-1);
    tex=MBOX_FindTexture_WORK(carscr_a_button_texture_name,&texIdx,0,gNumModels[0]-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][CAR_BUTTON_A_ICON].handle,tex,-1);
    memcpy(carscr_buttonInfo[p][CAR_BUTTON_A_ICON].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][CAR_BUTTON_A_ICON].matrix);
    carscr_buttonInfo[p][CAR_BUTTON_A_ICON].position[0]=75.1f;
    carscr_buttonInfo[p][CAR_BUTTON_A_ICON].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][CAR_BUTTON_A_ICON].position[2]=100.0f;
    ScaleUV(carscr_buttonInfo[p][CAR_BUTTON_A_ICON].matrix,carscr_buttonInfo[p][CAR_BUTTON_A_ICON].matrix,0.5f);
    memcpy(carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].matrix);
    WRollUV(3.1415927f,carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].matrix);
    carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].position[0]=87.1f;
    carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].position[2]=100.0f;
    tex=MBOX_FindTexture_WORK(carscr_b_button_texture_name,&texIdx,0,gNumModels[0]-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][CAR_BUTTON_B_ICON].handle,tex,-1);
    memcpy(carscr_buttonInfo[p][CAR_BUTTON_B_ICON].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][CAR_BUTTON_B_ICON].matrix);
    carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[0]=-75.1f;
    carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[2]=100.0f;
    carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[0]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[0];
    carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[1]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[1];
    carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[2]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_B_ICON].position[2];
    ScaleUV(carscr_buttonInfo[p][CAR_BUTTON_B_ICON].matrix,carscr_buttonInfo[p][CAR_BUTTON_B_ICON].matrix,0.5f);
    memcpy(carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].matrix);
    carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[0]=-87.1f;
    carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[2]=100.0f;
    carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[0]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[0];
    carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[1]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[1];
    carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[2]=(1.0f/zoom)*carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].position[2];
    carscr_buttonsInited[p]=1;
    carscr_buttonsJustInited[p]=1;
    ZeroCameras();
}

extern f32 fabsf(f32);
#pragma intrinsic(fabsf)
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* Lay player p's buttons out for this frame (once they exist). The selected option's place on the list (the list
 * squeezed to fit between maxY and minY when it is long) sets where the list scrolls to; the cursor spins beside it
 * and eases there (its speed follows the distance, within 180 to 540 a second, scaled). Each option's label and
 * value buttons turn to show their selected face when the option is selected (a half turn at 4 pi a second),
 * scroll at most scrollSpeed, fade over one button's height past the list's ends, and hide when not offered, past
 * the ends or once the car is locked. The A icon hides once the car is locked, the B icon once everyone is. */
void carscr_updateButtons(s8 p)
{
    static f32 cursorSpeed[4];
    static f32 scrollSpeed[4];
    s32 buttonIdx;
    f32 prevPos[3];
    f32 buttonY;
    f32 selectedPos;
    s8 clearScrollSpeed=1;
    s32 mask;
    void *tex;
    u16 texIdx;
    Color4 buttonColor={{255,255,255,255}};
    if(!carscr_buttonsInited[p])return;
    if((carscr_gNumOptions[p]-1)*carscr_gButtonSpace*carscr_gButtonScale>carscr_maxY-carscr_minY)
        selectedPos=carscr_maxY-carscr_gActiveOption[p]*(carscr_maxY-carscr_minY)/(carscr_gNumOptions[p]-1);
    else selectedPos=carscr_maxY-carscr_gActiveOption[p]*carscr_gButtonSpace*carscr_gButtonScale;
    memcpy(prevPos,carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position,12);
    carscr_buttonInfo[p][CAR_BUTTON_CURSOR].angle+=6.2831855f*gFrameSeconds;
    if(carscr_buttonInfo[p][CAR_BUTTON_CURSOR].angle>6.2831855f)carscr_buttonInfo[p][CAR_BUTTON_CURSOR].angle-=6.2831855f;
    carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[0]=carscr_buttonX*carscr_cursorZ/carscr_buttonZ+-170.0f*carscr_gButtonScale;
    carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]=selectedPos*carscr_cursorZ/carscr_buttonZ;
    carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[2]=carscr_cursorZ;
    if(!carscr_buttonsJustInited[p]) {
        if(cursorSpeed[p]<fabsf(carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]-prevPos[1])*60.0f*carscr_gButtonScale/10.0f) {
            cursorSpeed[p]=fabsf(carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]-prevPos[1])*60.0f*carscr_gButtonScale/10.0f;
            cursorSpeed[p]=cursorSpeed[p]<540.0f*carscr_gButtonScale ? cursorSpeed[p] : 540.0f*carscr_gButtonScale;
            cursorSpeed[p]=cursorSpeed[p]>180.0f*carscr_gButtonScale ? cursorSpeed[p] : 180.0f*carscr_gButtonScale;
        }
        if(carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]>prevPos[1]+gFrameSeconds*cursorSpeed[p])
            carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]=prevPos[1]+gFrameSeconds*cursorSpeed[p];
        else if(carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]<prevPos[1]-gFrameSeconds*cursorSpeed[p])
            carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[1]=prevPos[1]-gFrameSeconds*cursorSpeed[p];
        else cursorSpeed[p]=180.0f*carscr_gButtonScale;
    }
    ScaleUV(unitmatrix,carscr_buttonInfo[p][CAR_BUTTON_CURSOR].matrix,carscr_gButtonScale*0.4f);
    WRollUV(carscr_buttonInfo[p][CAR_BUTTON_CURSOR].angle,carscr_buttonInfo[p][CAR_BUTTON_CURSOR].matrix);
    WPitchUV(1.5707964f,carscr_buttonInfo[p][CAR_BUTTON_CURSOR].matrix);
    buttonY=selectedPos+carscr_gActiveOption[p]*carscr_gButtonSpace*carscr_gButtonScale;
    for(buttonIdx=0;buttonIdx<CAR_OPTS;buttonIdx++) {
        if(buttonIdx==carscr_gOpts[p][carscr_gActiveOption[p]]) {
            if(carscr_buttonInfo[p][buttonIdx].angle<3.1415927f)carscr_buttonInfo[p][buttonIdx].angle+=12.566371f*gFrameSeconds;
            if(carscr_buttonInfo[p][buttonIdx].angle>3.1415927f)carscr_buttonInfo[p][buttonIdx].angle=3.1415927f;
            if(carscr_buttonsJustInited[p])carscr_buttonInfo[p][buttonIdx].angle=3.1415927f;
        } else {
            if(carscr_buttonInfo[p][buttonIdx].angle>0.0f)carscr_buttonInfo[p][buttonIdx].angle-=12.566371f*gFrameSeconds;
            if(carscr_buttonInfo[p][buttonIdx].angle<0.0f)carscr_buttonInfo[p][buttonIdx].angle=0.0f;
            if(carscr_buttonsJustInited[p])carscr_buttonInfo[p][buttonIdx].angle=0.0f;
        }
        memcpy(prevPos,carscr_buttonInfo[p][buttonIdx].position,12);
        carscr_buttonInfo[p][buttonIdx].position[0]=-120.0f*carscr_gButtonScale+carscr_buttonX;
        carscr_buttonInfo[p][buttonIdx].position[1]=buttonY;
        carscr_buttonInfo[p][buttonIdx].position[2]=carscr_buttonZ;
        if(!carscr_buttonsJustInited[p]) {
            if(scrollSpeed[p]<fabsf(carscr_buttonInfo[p][buttonIdx].position[1]-prevPos[1])*60.0f/10.0f) {
                scrollSpeed[p]=fabsf(carscr_buttonInfo[p][buttonIdx].position[1]-prevPos[1])*60.0f/10.0f;
                scrollSpeed[p]=scrollSpeed[p]<540.0f*carscr_gButtonScale ? scrollSpeed[p] : 540.0f*carscr_gButtonScale;
                scrollSpeed[p]=scrollSpeed[p]>180.0f*carscr_gButtonScale ? scrollSpeed[p] : 180.0f*carscr_gButtonScale;
            }
            if(carscr_buttonInfo[p][buttonIdx].position[1]>prevPos[1]+gFrameSeconds*scrollSpeed[p]) {
                carscr_buttonInfo[p][buttonIdx].position[1]=prevPos[1]+gFrameSeconds*scrollSpeed[p];
                clearScrollSpeed=0;
            } else if(carscr_buttonInfo[p][buttonIdx].position[1]<prevPos[1]-gFrameSeconds*scrollSpeed[p]) {
                carscr_buttonInfo[p][buttonIdx].position[1]=prevPos[1]-gFrameSeconds*scrollSpeed[p];
                clearScrollSpeed=0;
            }
        }
        carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].position[0]=120.0f*carscr_gButtonScale+carscr_buttonX;
        carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].position[1]=carscr_buttonInfo[p][buttonIdx].position[1];
        carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].position[2]=carscr_buttonInfo[p][buttonIdx].position[2];
        carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].angle=carscr_buttonInfo[p][buttonIdx].angle;
        ScaleUV(unitmatrix,carscr_buttonInfo[p][buttonIdx].matrix,carscr_gButtonScale);
        ScaleUV(unitmatrix,carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].matrix,carscr_gButtonScale);
        WPitchUV(carscr_buttonInfo[p][buttonIdx].angle-1.5707964f,carscr_buttonInfo[p][buttonIdx].matrix);
        WPitchUV(carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].angle-1.5707964f,carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].matrix);
        if(carscr_buttonInfo[p][buttonIdx].angle>1.5707964f)tex=MBOX_FindTexture_WORK(carscr_select_texture_name,&texIdx,0,gNumModels[0]-1,1);
        else tex=MBOX_FindTexture_WORK(carscr_button_texture_name,&texIdx,0,gNumModels[0]-1,1);
        ZOID_SetTexture((s16)carscr_buttonInfo[p][buttonIdx].handle,tex,-1);
        ZOID_SetTexture((s16)carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].handle,tex,-1);
        if(carscr_buttonInfo[p][buttonIdx].position[1]>carscr_maxY+carscr_gButtonSpace*carscr_gButtonScale || carscr_buttonInfo[p][buttonIdx].position[1]<carscr_minY-carscr_gButtonSpace*carscr_gButtonScale)carscr_buttonInfo[p][buttonIdx].alpha=0;
        else if(carscr_buttonInfo[p][buttonIdx].position[1]>carscr_maxY)carscr_buttonInfo[p][buttonIdx].alpha=255.0f-(carscr_buttonInfo[p][buttonIdx].position[1]-carscr_maxY)*255/(carscr_gButtonSpace*carscr_gButtonScale);
        else if(carscr_buttonInfo[p][buttonIdx].position[1]<carscr_minY)carscr_buttonInfo[p][buttonIdx].alpha=255.0f-(carscr_minY-carscr_buttonInfo[p][buttonIdx].position[1])*255/(carscr_gButtonSpace*carscr_gButtonScale);
        else carscr_buttonInfo[p][buttonIdx].alpha=255;
        if(!carscr_validOption(p,buttonIdx))carscr_buttonInfo[p][buttonIdx].alpha=0;
        carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].alpha=buttonColor.c[3]=carscr_buttonInfo[p][buttonIdx].alpha;
        ZOID_SetColor((s16)carscr_buttonInfo[p][buttonIdx].handle,&buttonColor);
        ZOID_SetColor((s16)carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].handle,&buttonColor);
        if(!carscr_validOption(p,buttonIdx)) {
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx].handle,1,15);
            MBOX_HideObject(carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].handle,1,15);
        } else if(!carscr_buttonInfo[p][buttonIdx].alpha || gCarLocked[p]==1) {
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx].handle,1,15);
            MBOX_HideObject(carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].handle,1,15);
            buttonY-=carscr_gButtonSpace*carscr_gButtonScale;
        } else {
            mask=1<<p;
            MBOX_ShowObject(carscr_buttonInfo[p][buttonIdx].handle,0,mask);
            MBOX_ShowObject(carscr_buttonInfo[p][CAR_VALUE_BUTTON(buttonIdx)].handle,0,mask);
            buttonY-=carscr_gButtonSpace*carscr_gButtonScale;
        }
    }
    if(gCarLocked[p]==1) {
        MBOX_HideObject(carscr_buttonInfo[p][CAR_BUTTON_A_ICON].handle,1,15);
        MBOX_HideObject(carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].handle,1,15);
        carscr_buttonInfo[p][CAR_BUTTON_CURSOR].position[2]=2000.0f;
    } else {
        mask=1<<p;
        MBOX_ShowObject(carscr_buttonInfo[p][CAR_BUTTON_A_ICON].handle,0,mask);
        MBOX_ShowObject(carscr_buttonInfo[p][CAR_BUTTON_A_ARROW].handle,0,mask);
    }
    if(allLocked==1) {
        MBOX_HideObject(carscr_buttonInfo[p][CAR_BUTTON_B_ICON].handle,1,15);
        MBOX_HideObject(carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].handle,1,15);
    } else {
        mask=1<<p;
        MBOX_ShowObject(carscr_buttonInfo[p][CAR_BUTTON_B_ICON].handle,0,mask);
        MBOX_ShowObject(carscr_buttonInfo[p][CAR_BUTTON_B_ARROW].handle,0,mask);
    }
    if(clearScrollSpeed==1)scrollSpeed[p]=0.0f;
    carscr_buttonsJustInited[p]=0;
}

VIEW(Color4, carscr_solidColor);
#define carscr_solidColor carscr_solidColor_view
VIEW(Color4, carscr_normalColor);
#define carscr_normalColor carscr_normalColor_view
/* Colour the first player's car carscr_solidColor (enabled 1) or carscr_normalColor: its body, parts 3 and 4 and
 * tyres. MAT3 shows or hides its grid backdrop here as well. */
void showGridMap(s32 enabled)
{
    if(enabled==1) {
        Color4 color=carscr_solidColor;
        if(gCarParts[gThisCar[0]].frame) {
            ZOID_SetColor(gCarParts[gThisCar[0]].frame,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].hood,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].sheen,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[0],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[1],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[2],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[3],&color);
        }
    } else {
        Color4 color=carscr_normalColor;
        if(gCarParts[gThisCar[0]].frame) {
            ZOID_SetColor(gCarParts[gThisCar[0]].frame,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].hood,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].sheen,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[0],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[1],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[2],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].tires[3],&color);
        }
    }
}

typedef struct ZMatrix { f32 uvs[9]; f32 pos[3]; } ZMatrix;
ComponentLights compLight[2] = {
    {{0, 128, 0}, 0, {0, 128, 0}, 0, {{{192, 192, 192}, 0, {192, 192, 192}, 0, {0, 100, 0}, 0}, {{0}}}},
    {{255, 0, 0}, 0, {255, 0, 0}, 0, {{{192, 192, 192}, 0, {192, 192, 192}, 0, {0, 100, 0}, 0}, {{0}}}},
};
VIEW(s16[11], componentToName);
#define componentToName componentToName_view
VIEW(char *[5], carComponentNames);
#define carComponentNames carComponentNames_view

extern s32 sprintf(char *, const char *, ...);

extern ZMatrix *ZOID_FindMatrix(s16);
/* The part being changed shown on the car, each frame (one player, not a ghost race): while the transmission,
 * handling, engine, tyres or frame option is selected, the 11 part models of the car's current choices (named by
 * carComponentNames and the choice) are placed on the car's frame (the engine and frame parts in the middle, the
 * tyre parts at the tyres), the one for the selected option lit by the flashing light and pulsing; otherwise and
 * while the car is locked they are hidden. A part's model is made again when its choice changes. */
void updateCarComponents(s32 p)
{
    s32 i;
    f32 lightVec[3] = {-0.57f, 0.57f, -0.57f};
    f32 lightDir[3];
    CarParts *car;
    s16 currentLevel;
    s32 whichLight;
    f32 compScale;
    char componentName[16];
    s32 mask;
    CarComponent (*components)[11] = (CarComponent (*)[11])gCarComponents;   /* with the matrix and position */
    if (gNumPlayers > 1 || gRace == kGhost) return;
    if (gCarLocked[p] == 1 || (carscr_gOpts[p][carscr_gActiveOption[p]] != CAR_OPT_TRANS && carscr_gOpts[p][carscr_gActiveOption[p]] != CAR_OPT_HANDLING && carscr_gOpts[p][carscr_gActiveOption[p]] != CAR_OPT_ENGINE && carscr_gOpts[p][carscr_gActiveOption[p]] != CAR_OPT_TIRES && carscr_gOpts[p][carscr_gActiveOption[p]] != CAR_OPT_FRAME)) {
        for (i = 0; i < 11; i++) {
            if (components[p][i].idx != -1) {
                MBOX_HideObject(components[p][i].idx, 1, 15);
                ZOID_SetObjectParent(components[p][i].idx, -1);
            }
        }
        return;
    }
    car = &gCarParts[p * CARS + gThisCar[p]];
    if (gCarPartFrame == -1) {
        gCarPartFrame = MBOX_NewObjectCam(MBOX_FindObject_WORK(carscr_part_frame_model_name, 0, gNumModels[0] - 1, 1), ZOID_FindMatrix((s16)car->frame), 15, -1, 0);
        MBOX_HideObject(gCarPartFrame, 1, 15);
    } else {
        ObjInstanceList[gCarPartFrame].mat = (void *)ZOID_FindMatrix((s16)car->frame);
    }
    compLight[1].col[0] = 255.0f * TB_GetFlashTextCycle();
    BodyVector(lightVec, lightDir, ZOID_FindMatrix((s16)car->frame)->uvs);
    compLight[0].l[0].dir[0] = lightVec[0] * 10.0f;
    compLight[0].l[0].dir[1] = lightVec[1] * 10.0f;
    compLight[0].l[0].dir[2] = lightVec[2] * 10.0f;
    compLight[1].l[0].dir[0] = lightVec[0] * 10.0f;
    compLight[1].l[0].dir[1] = lightVec[1] * 10.0f;
    compLight[1].l[0].dir[2] = lightVec[2] * 10.0f;
    for (i = 0; i < 11; i++) {
        currentLevel = -1;
        whichLight = 0;
        compScale = 1.0f;
        switch (componentToName[i]) {
        case 0:     /* the engine */
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_ENGINE) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarEngines[p+1][gThisCar[p]] % 5;
            break;
        case 1:     /* the frame */
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_FRAME) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarFrame[p+1][gThisCar[p]] % 3;
            break;
        case 2:     /* the handling (suspension) */
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_HANDLING) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarHandling[p+1][gThisCar[p]] % 3;
            break;
        case 3:     /* the tyres */
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_TIRES) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarTires[p+1][gThisCar[p]] % 4;
            break;
        case 4:     /* the transmission */
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_TRANS) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarTrans[p+1][gThisCar[p]] % 2;
            break;
        }
        if (currentLevel != components[p][i].level) {
            sprintf(componentName, carComponentNames[componentToName[i]], currentLevel + 1);
            if (components[p][i].idx != -1) {
                ZOID_RemoveObject(components[p][i].idx);
                components[p][i].idx = -1;
                components[p][i].level = -1;
            }
            components[p][i].idx = MBOX_NewObject(MBOX_FindObject_WORK(componentName, 0, gNumModels[0] - 1, 1), components[p][i].uvs, -1, 0);
            MBOX_HideObject(components[p][i].idx, 1, 15);
            if (gNumWindows == 1) {
                components[p][i].pos[0] = 0.0f;
                components[p][i].pos[1] = 1.0f;
                components[p][i].pos[2] = 15.0f;
            } else {
                components[p][i].pos[0] = 0.0f;
                components[p][i].pos[1] = 2.0f;
                components[p][i].pos[2] = 15.0f;
            }
            components[p][i].level = currentLevel;
        }
        switch (i) {
        case 0: case 1: case 10:
            ScaleUV(unitmatrix, components[p][i].uvs, compScale);
            components[p][i].pos[0] = 0.f;
            components[p][i].pos[1] = 0.f;
            components[p][i].pos[2] = 0.f;
            components[p][i].pos[1] += 0.1f;
            break;
        case 2: case 6:
            ScaleUV(ZOID_FindMatrix((s16)car->tires[0])->uvs, components[p][i].uvs, compScale);
            components[p][i].pos[0] = ZOID_FindMatrix((s16)car->tires[0])->pos[0];
            components[p][i].pos[1] = ZOID_FindMatrix((s16)car->tires[0])->pos[1];
            components[p][i].pos[2] = ZOID_FindMatrix((s16)car->tires[0])->pos[2];
            YawUV(3.1415927f, components[p][i].uvs);
            break;
        case 3: case 7:
            ScaleUV(ZOID_FindMatrix((s16)car->tires[1])->uvs, components[p][i].uvs, compScale);
            components[p][i].pos[0] = ZOID_FindMatrix((s16)car->tires[1])->pos[0];
            components[p][i].pos[1] = ZOID_FindMatrix((s16)car->tires[1])->pos[1];
            components[p][i].pos[2] = ZOID_FindMatrix((s16)car->tires[1])->pos[2];
            break;
        case 5: case 9:
            ScaleUV(ZOID_FindMatrix((s16)car->tires[2])->uvs, components[p][i].uvs, compScale);
            components[p][i].pos[0] = ZOID_FindMatrix((s16)car->tires[2])->pos[0];
            components[p][i].pos[1] = ZOID_FindMatrix((s16)car->tires[2])->pos[1];
            components[p][i].pos[2] = ZOID_FindMatrix((s16)car->tires[2])->pos[2];
            YawUV(3.1415927f, components[p][i].uvs);
            break;
        case 4: case 8:
            ScaleUV(ZOID_FindMatrix((s16)car->tires[3])->uvs, components[p][i].uvs, compScale);
            components[p][i].pos[0] = ZOID_FindMatrix((s16)car->tires[3])->pos[0];
            components[p][i].pos[1] = ZOID_FindMatrix((s16)car->tires[3])->pos[1];
            components[p][i].pos[2] = ZOID_FindMatrix((s16)car->tires[3])->pos[2];
            break;
        }
        ZOID_SetObjectParent(components[p][i].idx, (s16)gCarPartFrame);
        mask = 1 << p;
        MBOX_ShowObject(components[p][i].idx, 0, mask);
        ZOID_SetObjLights(components[p][i].idx, 1, &compLight[whichLight]);
    }
}

#define CAR_MIN(a,b) ((a) < (b) ? (a) : (b))

/* Empty: MAT3 builds a grid backdrop here; the N64 has none. */
void createGridMap(void)
{
}

/* Draw every player's cars, each frame. Each car of the roster fades in when it is the selected one and out when
 * not (5 a second); a locked car turns to the front over the frames left before the race (CAR_READY_FRAMES); while
 * the rims option is selected it turns side-on, else it spins (60 degrees a second). Each is drawn on its turntable
 * spot (the tall car 10 higher) in the player's colours (the team's in a battle) by UpdateSelectCar, and with one or
 * two players outside a ghost race again upside down below it as a reflection in windows 2 and 3, at two thirds
 * the alpha. The selected car's turntable follows it. In a ghost race the ghost windows show the cars and settings
 * the ghosts were recorded with. In a one-player race (not a ghost race) the car takes carscr_solidColor
 * (showGridMap) while a part option is selected, and the part shows on it (updateCarComponents). */
void showCarSelect(void)
{
    f32 uv[3][3];
    s32 i;
    s32 p;
    f32 carPos[3];
    u8 alpha;
    s32 framesLeft;
    s8 reflectIdx;
    f32 reflectUV[3][3];
    f32 reflectPos[3];
    s32 ghostIdx;
    s32 ghostCar;
    for (p = 0; p < gNumPlayers; p++) {
        for (i = 0; i < gNumCarsActive[p]; i++) {
            if (gThisCarIndex[p] == i) {
                carscr_cars[p][i].selection += 5.0f * gFrameSeconds;
                if (carscr_cars[p][i].selection > 1.0f) carscr_cars[p][i].selection = 1.0f;
            } else {
                carscr_cars[p][i].selection -= 5.0f * gFrameSeconds;
                if (carscr_cars[p][i].selection < 0.0f) carscr_cars[p][i].selection = 0.0f;
            }
            if (gCarLocked[p]) {
                framesLeft = 10 - carscr_wait;
                if (framesLeft <= 1) carscr_cars[p][i].angle = 3.6651914f;
                else {
                    carscr_cars[p][i].angle -= 3.6651914f;
                    while (carscr_cars[p][i].angle > 3.1415927f) carscr_cars[p][i].angle -= 6.2831855f;
                    while (carscr_cars[p][i].angle <= -3.1415927f) carscr_cars[p][i].angle += 6.2831855f;
                    carscr_cars[p][i].angle -= PC_EASE_DIV(carscr_cars[p][i].angle, (framesLeft - 1));
                    carscr_cars[p][i].angle += 3.6651914f;
                }
            } else if (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_RIMS) {
                carscr_cars[p][i].angle -= 1.5707964f;
                while (carscr_cars[p][i].angle > 3.1415927f) carscr_cars[p][i].angle -= 6.2831855f;
                while (carscr_cars[p][i].angle <= -3.1415927f) carscr_cars[p][i].angle += 6.2831855f;
                if (carscr_cars[p][i].angle > 1.5707964f)
                    carscr_cars[p][i].angle += CAR_MIN(1.0471976f * gFrameSeconds * 4.0f, 3.1415927f - carscr_cars[p][i].angle);
                else if (carscr_cars[p][i].angle < -1.5707964f) carscr_cars[p][i].angle -= CAR_MIN(1.0471976f * gFrameSeconds * 4.0f, 3.1415927f + carscr_cars[p][i].angle);
                else if (carscr_cars[p][i].angle > 0.f) carscr_cars[p][i].angle -= CAR_MIN(1.0471976f * gFrameSeconds * 4.0f, carscr_cars[p][i].angle);
                else carscr_cars[p][i].angle += CAR_MIN(1.0471976f * gFrameSeconds * 4.0f, -carscr_cars[p][i].angle);
                carscr_cars[p][i].angle += 1.5707964f;
            } else {
                carscr_cars[p][i].angle += 1.0471976f * gFrameSeconds;
                if (carscr_cars[p][i].angle >= 6.2831855f) carscr_cars[p][i].angle -= 6.2831855f;
            }
            ScaleUV(unitmatrix, uv, 2.5f);
            PitchUV(0.34906584f, uv);
            YawUV(carscr_cars[p][i].angle, uv);
            if (carscr_cars[p][i].car % CARS == CAR_TALL) {
                if (gNumWindows == 1) { carPos[0] = 0.0f; carPos[1] = 3.2f; carPos[2] = 40.5f; }
                else { carPos[0] = 0.0f; carPos[1] = 5.9f; carPos[2] = 40.5f; }
            } else {
                if (gNumWindows == 1) { carPos[0] = 0.0f; carPos[1] = 2.6f; carPos[2] = 40.5f; }
                else { carPos[0] = 0.0f; carPos[1] = 5.3f; carPos[2] = 40.5f; }
            }
            alpha = carscr_cars[p][i].selection * 255.0f;
            if (gRace == kBattle && gCheatState[CHEAT_BATTLE_PAINT] == 0)
                UpdateSelectCar(carscr_cars[p][i].car, p, gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarSheen[p], carPos, uv, alpha);
            else UpdateSelectCar(carscr_cars[p][i].car, p, car_color_choice_a[p + 1][carscr_cars[p][i].car % CARS], car_color_choice_b[p + 1][carscr_cars[p][i].car % CARS], car_color_choice_c[p + 1][carscr_cars[p][i].car % CARS], gCarSheen[p], carPos, uv, alpha);
            if (gNumPlayers < 3 && gRace != kGhost) {
                reflectIdx = p == 0 ? 2 : 3;
                ScaleUV(uv, reflectUV, 1.2f);
                RollUV(3.14159265f, reflectUV);
                reflectPos[0] = 1.2f * carPos[0];
                reflectPos[1] = 1.2f * carPos[1];
                reflectPos[2] = 1.2f * carPos[2];
                if (carscr_cars[p][i].car % CARS == CAR_TALL) reflectPos[1] -= 4.0f;
                else reflectPos[1] -= 2.5f;
                carscr_cars[reflectIdx][i].car = carscr_cars[p][i].car + 2 * CARS;   /* the reflection's own car records */
                if (gRace == kBattle && gCheatState[CHEAT_BATTLE_PAINT] == 0)
                    UpdateSelectCar(carscr_cars[reflectIdx][i].car, reflectIdx, gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarSheen[p], reflectPos, reflectUV, alpha / 1.5f);
                else UpdateSelectCar(carscr_cars[reflectIdx][i].car, reflectIdx, car_color_choice_a[p + 1][carscr_cars[p][i].car % CARS], car_color_choice_b[p + 1][carscr_cars[p][i].car % CARS], car_color_choice_c[p + 1][carscr_cars[p][i].car % CARS], gCarSheen[p], reflectPos, reflectUV, alpha / 1.5f);
            }
            if (gThisCarIndex[p] == i) {
                ScaleUV(unitmatrix, gTurntableMat[p][0], 1.3f);
                ScaleUV(unitmatrix, gTurntableMat[p][1], 1.3f);
                PitchUV(0.34906584f, gTurntableMat[p][0]);
                PitchUV(0.34906584f, gTurntableMat[p][1]);
                YawUV(carscr_cars[p][i].angle, gTurntableMat[p][0]);
                if (gNumWindows == 1) {
                    gTurntableMat[p][0][9] = 0.0f;
                    gTurntableMat[p][0][10] = 1.8f;
                    gTurntableMat[p][0][11] = 45.0f;
                    gTurntableMat[p][1][9] = 0.0f;
                    gTurntableMat[p][1][10] = 1.8f;
                    gTurntableMat[p][1][11] = 45.0f;
                } else {
                    gTurntableMat[p][0][9] = 0.0f;
                    gTurntableMat[p][0][10] = 4.8f;
                    gTurntableMat[p][0][11] = 45.0f;
                    gTurntableMat[p][1][9] = 0.0f;
                    gTurntableMat[p][1][10] = 4.8f;
                    gTurntableMat[p][1][11] = 45.0f;
                }
                if (gRace == kGhost) {
                    for (ghostIdx = 1; ghostIdx < gNumGhosts + 1; ghostIdx++) {
                        if (!gGhosts[ghostIdx] || gLastCar[ghostIdx] != (*(*gGhosts[ghostIdx])->header)->car) {
                            if (gLastCar[ghostIdx] != -1) UpdateSelectCar(carscr_cars[ghostIdx][gLastCar[ghostIdx]].car, ghostIdx, 0, 0, 0, 0, carPos, uv, 0);
                        }
                        if (!gGhosts[ghostIdx]) continue;
                        ghostCar = (*(*gGhosts[ghostIdx])->header)->car;
                        gLastCar[ghostIdx] = ghostCar;
                        carscr_cars[ghostIdx][ghostCar].car = ghostCar + ghostIdx * CARS;
                        gCarTires[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_TIRES];
                        TIRE_RIMS(ghostIdx + 1)[ghostCar] = (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_RIMS];
                        gFrontTireScale[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_FRONT_TIRE_SCALE] / 100.0f + 0.75f;
                        gRearTireScale[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_REAR_TIRE_SCALE] / 100.0f + 0.75f;
                        UpdateSelectCar(carscr_cars[ghostIdx][ghostCar].car, ghostIdx, (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_COLOR_A], (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_COLOR_B], (*(*gGhosts[ghostIdx])->header)->settings[CAR_SETTING_COLOR_C], 0, carPos, uv, 255);
                    }
                }
            }
        }
        if (gNumPlayers == 1 && gRace != kGhost) {
            s32 show;
            createGridMap();
            show = !gCarLocked[0] && (carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_TRANS || carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_HANDLING || carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_ENGINE || carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_TIRES || carscr_gOpts[p][carscr_gActiveOption[p]] == CAR_OPT_FRAME);
            showGridMap(show);
            updateCarComponents(p);
        }
    }
}
