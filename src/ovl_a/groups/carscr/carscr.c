/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: claims the three deleted-inline stubs before _UninitScreen (carscr_UninitScreen):
 * clearCarComponents clearCarComponents, carscr_uninitButtons uninitButtons@carscr.c, releaseGridMap releaseGridMap (see the
 * D05C header). Every old member MATCHes; D05C needs no shaping device now. */
/* superseding revision: setGhosts (GC setGhosts@carscr.c) and updateCarComponents (GC updateCarComponents@carscr.c) are
 * now members (MAT3 ports, headers at the functions). Every earlier member is unchanged; the one shared-declaration change is
 * the 4th parameter of MBOX_FindObject_WORK (s8 -> s32). */
/* superseding revision: carscr_initButtons (GC initButtons@carscr.c) and carscr_updateButtons (GC updateButtons@carscr.c) are now
 * members, ported from reference/repos/mat3 carscr.c; see their headers below. Old members and their text are unchanged. */
/* Image-A selected car-stat product: D300, 404 newly strict-matched bytes.
 * The genuine pausescr_cleanup/car-selection group retains all five accepted members.
 * D300 uses direct per-component indexed factors, letting stock IDO hoist the
 * complete row-address expressions while preserving the five selection reads.
 * Two do/while(0) statement regions deliberately establish CFE block boundaries:
 * the first pair assignment and third-factor update coalesce into one store;
 * the next region and final update retain three native stores per component.
 * This is disclosed source shaping under the live block-boundary rule,
 * not a claim that these original source macros or blocks have been recovered.
 * The initial pair's source operand order and compound third-factor update
 * reproduce the native floating operand order without reassociating its stages.
 * No volatile, unused local, capacity change, false signature, artificial caller,
 * assembly, forced register, or compiler modification is used.
 */
/* Overlay A cleanup (8039D05C), complete 676-byte body.
 * , record-context group at d725fbe089ba81b36386b849f492035261af4883.
 * Authentic reconstructed callers/callees are preserved as nonclaiming context.
 * Arcade ancestry is unknown. This is a private-ABI no-argument helper that
 * clobbers s0-s8; its real caller updateCarScreen saves/restores that set.
 * This is MAT3 carscr.c _UninitScreen. Its three inlined statics are now the non-kept helpers
 * clearCarComponents clearCarComponents(int p), carscr_uninitButtons uninitButtons(signed char p) and releaseGridMap
 * releaseGridMap (empty in the N64 build, like createGridMap), called where MAT3 calls them
 * (releaseGridMap under `gNumPlayers == 1 && gRace != 2` right after the first wave unload). Their deleted
 * procedures are the retail stubs 0x8039D044/4C/54 in that order (post-order, last call first). The former
 * shaping device (a code-free `if (player) {}` read in the panel loop) is no longer needed and is removed.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct FileInfo {void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 controller;} FileInfo;
typedef struct Profile Profile;
struct Profile {Profile **next,**previous;FileInfo **file;u32 id0,id1;u8 name[24];u8 **stats;};
typedef struct Player {u32 held,pressed,unknown08,repeated;u8 unknown10[50];u8 paint,unknown43[5];Profile **owner;} Player;
typedef struct RecordedCar {u8 unknown00[8],car,unknown09[3];s8 component;u8 unknown0D[2],red,green;s8 blue,part,unknown13,stat0,stat1;} RecordedCar;
typedef struct Record Record;
/* A ghost (SaveInit' Ghost): +0x24 is its data handle, pointer-sized on the PC (docs/port/CASTS.md). */
struct Record {Record **next;FileInfo **file;s8 track;u8 mode,name[14];u32 id0,id1;f32 time;void *unknown24;RecordedCar **active;};
extern Player gPlayers[];
typedef struct RecordList {u8 indirect,doubly,unknown02[2];s32 count;Record **first, **last;} RecordList;
extern RecordList gGhostFiles;
extern Record **limboGhosts,**gGhosts[4];
extern s16 gLastCar_at_1[3];
extern s32 gRace;
extern s8 gGhostSelect,gTrackNum,gBackwards,expansion_pak_present;
extern s32 game_strcmp(u8 *,u8 *);
extern s32 ReadGhostData(Record **);
extern void PurgeGhostData(void **);
typedef struct Language {s32 unknown0;u8 **text;s32 unknown8;u16 *indices;u8 **indexed;} Language;
typedef struct Camera {f32 uv[3][3],position[3];u8 unknown30[104];} Camera;
typedef struct Slot {s32 kind,handle;f32 angle,matrix[3][3],position[3];u8 alpha,unknown3D[3];} Slot;
typedef struct CarSlot {f32 selection,angle;s8 position,car;u8 unknown0A[2];} CarSlot;
typedef struct FiveParts FiveParts;
typedef struct Blit Blit;
typedef struct MultiBlit {char *name;s16 x,y,width,height,top,bottom,left,right;u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;} MultiBlit;
extern Language gText;
extern s16 gNumPlayers,gNumWindows;
extern Camera gCamera[];
extern s8 allLocked,gCarLocked[],carscr_gActiveOptionEnabled[],gThisCar[],gThisCarIndex[],gCars[][13],gCarOrder[];
extern s16 gNumCarsActive[],gLastCar[];
extern s32 carscr_gActiveOption[],carscr_gFirstOption[],gHornSnd[],gTurntableIdx[][2];
extern void *gCarMatrices[48];
extern u8 gDriveHulk[4];
extern volatile u8 gNumModels;
extern s8 gEnableTireScaling,carscr_inited;
extern f32 HFOV,carscr_gButtonZoom,carscr_gButtonScale,carscr_gButtonSpace,carscr_buttonX,carscr_maxY,carscr_minY,carscr_buttonZ,carscr_cursorZ;
extern f32 gTurntableMat[][2][12],unitmatrix[12];
extern FiveParts *carscr_pane[];
extern s8 gCarTrans_players[][13],gCarHandling_players[][13],gCarEngines_players[][13],gCarTires_players[][13],gCarFrame_players[][13],gCarWings_players[][13];
extern s8 car_color_choice_a_players[][13],car_color_choice_b_players[][13],car_color_choice_c_players[][13],D_80111589[][13],gCarHorn_players[][13];
extern f32 gFrontTireScale[][13],gRearTireScale[][13];
extern CarSlot carscr_cars[][13];
extern MultiBlit carscr_mb[];
extern Blit *carscr_blit;
extern char carscr_turntable_model_name[],carscr_turntable_base_model_name[];
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern void LoadWave(s32,s32,s32,s32,void *);
extern void *ZOID_NewMatrix(void);
extern void SetOptions(void);
extern void func_800A5A40(void);
extern void ZOID_SplitScreen(s16);
extern void audio_interrupt_handler(s32);
extern void InitVisuals(s32);
extern void particle_lifetime_set(void);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern FiveParts *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
extern void CopyUV(void *,void *);
extern s32 MBOX_FindObject_WORK(char *,s8,s8,s32); /* : 4th parameter int (MAT3 MBOX_FindObject_WORK) */
extern s32 MBOX_NewObject(s32,void *,s32,u32);
extern void MBOX_HideObject(s32,s32,u32);
extern void MBOX_ShowObject(s32,s32,u32);
extern s8 GetPlayerCarSetting(Profile **,u8,u8);
extern void SavePlayerCarSetting(Profile **,u8,u8,s8);
extern u8 GetPlayerCar(Profile **);
extern u8 GetPlayerCarTrans(void **);
extern void SavePlayerCar(Profile **,u8);
extern s8 IsEngineUnlocked(s32,s32);
extern s8 IsTiresUnlocked(s32,s32);
extern s8 IsFrameUnlocked(s32,s32);
extern s8 IsCarUnlocked(s32,s32);
extern s8 IsTransUnlocked(s32,s32);
extern Blit *NewMultiBlit(s16,s16,MultiBlit *,s16);
extern void ZeroCameras(void);
extern void UpdateCameras(void);
extern void ClearRushControls(void);
/* Genuine low-image services, reconstructed below with natural source inputs. */
extern void carscr_initButtons(s8);
extern void calcValidOptions(s32);
typedef struct Vec3 {f32 v[3];} Vec3;
typedef struct PreparedCar {u8 unknown0,car;s8 color[3];u8 unknown5,flags,unknown7;} PreparedCar;
extern Record **gLimboGhosts;
extern u32 gSkipToGameState,gGameState;
extern s32 carscr_wait,carscr_gNumOptions[],carscr_gOpts[][19],carscr_buttonsJustInited[],ghosts_enabled;
extern s8 gSoftReset,carscr_gActiveZ[],gCarTeam[],gCarTeamColor[];
extern s16 gCheatState_at_13,gCarSheen[];
extern Slot carscr_buttonInfo[][44];
extern Vec3 carscr_msgPos;
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern u8 carscr_ready_text[],carscr_ghost_width_text_first[],carscr_ghost_width_text_second[];
extern s8 gCarTrans[][13],gCarHandling[][13],gCarEngines[][13],gCarTires[][13],gCarFrame[][13],gCarWings[][13];
extern s8 car_color_choice_a[][13],car_color_choice_b[][13],car_color_choice_c[][13],gTireRims[][13],gCarHorn[][13];
extern s8 PS_PakInserted(s32);
extern s8 PS_PakChanged(s32);
extern void setGameState(s32);
extern void carscr_UninitScreen(void);
extern void showCarSelect(void);
extern void carscr_updateButtons(s8);
extern Record **getNextGhost(Record **,s8);
extern s32 AudPlayEffect(u32,u32,u32,u8);
extern void PlayLRSound(s32);
extern void PlayStartABSound(u32);
extern void AudStopEffect(s32);
extern void SavePlayerCarTrans(Profile **,u8);
extern void SetSheen(s16,s16,s32);
extern void AudSetMusicVol(f32,f32);
extern void ContinueCircuit(void);
extern void SetControls(s32);
extern void ZOID_ProjectWindow(s32,f32 *,Camera *,void *,s16 *);
extern void *HAL_SetFont(s32);
extern s16 TB_GetFontLineHeight(void);
extern u32 HAL_GetTextWidth(u8 *,s16);
extern void ChangePaneSize(FiveParts *,s32,s32,s32,s32);
extern s8 gNumGhosts;

extern s8 gThisCar[];
extern s8 gCarTrans_players[][13];
extern s8 gCarHandling_players[][13];
extern s8 gCarEngines_players[][13];
extern s8 gCarTires_players[][13];
extern s8 gCarFrame_players[][13];
extern float gTransStats[][4];
extern float gHandlingStats[][4];
extern float gEngineStats[][4];
extern float gTireStats[][4];
extern float gFrameStats[][4];
extern float gStats[][4];

void calcStats(s32 player)
{
    s16 component;

    for (component = 0; component < 4; component++) {
        do {
            gStats[player][component] = gHandlingStats[gCarHandling_players[player][gThisCar[player]]][component] * gTransStats[gCarTrans_players[player][gThisCar[player]]][component];
            gStats[player][component] *= gEngineStats[gCarEngines_players[player][gThisCar[player]]][component];
        } while (0);
        do {
            gStats[player][component] *= gTireStats[gCarTires_players[player][gThisCar[player]]][component];
        } while (0);
        gStats[player][component] *= gFrameStats[gCarFrame_players[player][gThisCar[player]]][component];
    }
}
/* setGhosts = setGhosts@carscr.c (MAT3 reference/repos/mat3/decomp/src/GameSource/src/carscr.c, GC 0x80090638,
 * size 0xac0), : direct typed port of the MAT3 body (tGhostFile = Record, gPlayers[0].save = gPlayers[0].owner,
 * gGhostFiles.head = gGhostFiles.first, limboGhosts, gGhosts, gLastCar,
 * gGhostSelect, gTrackNum, gBackwards, strcmp = game_strcmp,
 * ReadGhostData). Private-ABI member of this IPA group (clobbers s0-s8; callers
 * carscr_InitScreen _InitScreen and updateCarScreen updateCarScreen).
 * N64 differences from the GC body (from the retail words): the three clearing loops run gGhosts, nearGhosts,
 * gLastCar (GC: nearGhosts first); the player-id compares are written player-first (operand order of the two
 * lw); the best-branch limbo loop uses one `continue` guard (the reload of limboGhosts+3 is then inserted only
 * after the body's calls, as retail); an N64-only tail purges ghost slots 2 and 3 (PurgeGhostData =
 * PurgeGhostData) when expansion_pak_present is clear. The shift loops have their body on its own source line: as1 breaks
 * scheduling ties by line number, and the one-line form orders the unrolled copies 0,1,2,3 instead of retail's
 * 1,2,3,0. */
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
    player = *gPlayers[0].owner;
    for (i = 0; i < 4; i++) gGhosts[i] = 0;
    for (i = 0; i < 5; i++) nearGhosts[i] = 0;
    for (i = 1; i < 4; i++) gLastCar[i] = -1;
    if (gRace != 2) return;
    if (!gGhostSelect) {
        for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
            ghost = *ghosth;
            nextGhost = ghost->next;
            if (ghost->track == gTrackNum && ghost->mode == gBackwards &&
                !game_strcmp(ghost->name, player->name) &&
                player->id0 == ghost->id0 && player->id1 == ghost->id1) {
                if ((!best || ghost->time < (*best)->time) && ReadGhostData(ghosth)) best = ghosth;
            }
        }
        for (i = 0; i < 3; i++) {
            ghosth = limboGhosts + i;
            ghost = *ghosth;
            if (ghost->active && ghost->track == gTrackNum && ghost->mode == gBackwards &&
                !game_strcmp(ghost->name, player->name) &&
                player->id0 == ghost->id0 && player->id1 == ghost->id1) {
                if ((!best || ghost->time < (*best)->time) && ReadGhostData(ghosth)) best = ghosth;
            }
        }
        if (!best) {
            for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
                ghost = *ghosth;
                nextGhost = ghost->next;
                if (ghost->track == gTrackNum && ghost->mode == gBackwards) {
                    for (i = 3; i >= 1; i--) {
                        if ((!gGhosts[i] || ghost->time > (*gGhosts[i])->time) && ReadGhostData(ghosth)) {
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
                if (ghost->active && ghost->track == gTrackNum && ghost->mode == gBackwards) {
                    for (i = 3; i >= 1; i--) {
                        if ((!gGhosts[i] || ghost->time > (*gGhosts[i])->time) && ReadGhostData(ghosth)) {
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
                if (ghost->track == gTrackNum && ghost->mode == gBackwards) {
                    if (ghosth != best) {
                        if (ghost->time < (*best)->time) {
                            if (!nearGhosts[1] || ghost->time > (*nearGhosts[1])->time) {
                                if (ReadGhostData(ghosth)) {
                                    nearGhosts[0] = nearGhosts[1];
                                    nearGhosts[1] = ghosth;
                                }
                            } else if (!nearGhosts[0] || ghost->time > (*nearGhosts[0])->time) {
                                if (ReadGhostData(ghosth)) nearGhosts[0] = ghosth;
                            }
                        } else {
                            if (!nearGhosts[3] || ghost->time < (*nearGhosts[3])->time) {
                                if (ReadGhostData(ghosth)) {
                                    nearGhosts[4] = nearGhosts[3];
                                    nearGhosts[3] = ghosth;
                                }
                            } else if (!nearGhosts[4] || ghost->time < (*nearGhosts[4])->time) {
                                if (ReadGhostData(ghosth)) nearGhosts[4] = ghosth;
                            }
                        }
                    }
                }
            }
            for (ghosth = limboGhosts; ghosth < limboGhosts + 3; ghosth++) {
                ghost = *ghosth;
                if (!ghost->active || ghost->track != gTrackNum || ghost->mode != gBackwards || ghosth == best) continue;
                if (ghost->time < (*best)->time) {
                    if (!nearGhosts[1] || ghost->time > (*nearGhosts[1])->time) {
                        if (ReadGhostData(ghosth)) {
                            nearGhosts[0] = nearGhosts[1];
                            nearGhosts[1] = ghosth;
                        }
                    } else if (!nearGhosts[0] || ghost->time > (*nearGhosts[0])->time) {
                        if (ReadGhostData(ghosth)) nearGhosts[0] = ghosth;
                    }
                } else {
                    if (!nearGhosts[3] || ghost->time < (*nearGhosts[3])->time) {
                        if (ReadGhostData(ghosth)) {
                            nearGhosts[4] = nearGhosts[3];
                            nearGhosts[3] = ghosth;
                        }
                    } else if (!nearGhosts[4] || ghost->time < (*nearGhosts[4])->time) {
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
            if (ghost->track == gTrackNum && ghost->mode == gBackwards) {
                for (i = 1; i < 4; i++) {
                    if ((!gGhosts[i] || ghost->time < (*gGhosts[i])->time) && ReadGhostData(ghosth)) {
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
            if (ghost->active && ghost->track == gTrackNum && ghost->mode == gBackwards) {
                for (i = 1; i < 4; i++) {
                    if ((!gGhosts[i] || ghost->time < (*gGhosts[i])->time) && ReadGhostData(ghosth)) {
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
void setGhosts(void);
void calcStats(s32);
/* Overlay A 0x803A0FD8, 3796 bytes: GC _InitScreen@carscr.c (MAT3 0x8008f904, 0xbc8 B), static.
 * Source: reference/repos/mat3 decomp/src/GameSource/src/carscr.c _InitScreen (typed port); the N64 build is an earlier
 * revision: 62/80/61/81 audio loads, ghost count by expansion_pak_present, 12 per-car settings (8,9 bytes; 10,11 tire scales),
 * no cars[].alpha init, no SavePlayerCarTrans, 38 blits.  Private-ABI function of the IPA group (unsaved s0-s8/f20-f24).
 * Shaping (all verified on the builder, group cleanup_colortrace):
 *  - the pane loop and the matrix loop use `p`/`i` as written (p out-ranks i for s4/s5);
 *  - gNumModels is volatile u8 in the head;
 *  - `0.0` spelling for the pos[0] stores (zero constant gets $f0, 2.0 gets $f2);
 *  - `for (gNumCarsActive[p] = 0, i = 0; ...)` orders the store after the row-offset arithmetic;
 *  - `Player *pl` (assigned &gPlayers[p], used for ->owner) is the third 4-byte local: it gives the retail 208-byte frame and puts
 *    p's home at 200(sp); it is real, not padding.
 * - `pl = &gPlayers[p];` is also the first statement of the FIRST player loop, where it is dead and compiles to
 *    nothing.  It enters the &gPlayers[p] expression into uopt's expression table ahead of the later loops' webs, so
 *    f_spilltemps (homes in expression-bit order, first fit by block conflict) gives the strength-reduced pointer (s6) the
 *    first home, 188(sp), and pushes the gNumCarsActive pointer to 180(sp), as in retail.  Without it the pointer gets the
 *    fourth home (176) and gNumCarsActive the second (184): 7 words.  Disclosed shaping (a code-free dead assignment);
 *    the original probably assigned a player pointer at the top of each player loop;
 * - the final `for (i.)` loop body is on its own line: as1 then fills the unrolled loop's branch delay slot with
 *    the first byte store (displacement rewritten), not the `addiu` (4 words; source-line tie in the delay-slot filler).
 * Matches in this group (the scorer, strict). Own.rodata verified by hand against decompressed image A (7/7 literal
 * sites, rocheck.py).
 */
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
        pl = &gPlayers[p];
        carscr_gActiveOption[p] = 0;
        carscr_gActiveOptionEnabled[p] = 1;
        carscr_gFirstOption[p] = 1;
        gCarLocked[p] = 0;
        gHornSnd[p] = -1;
        calcStats(p);
    }
    for (p = 0; p < gNumPlayers; p++) {
        for (gNumCarsActive[p] = 0, i = 0; i < 13; i++) {
            if (i < 13) gCars[p][gNumCarsActive[p]++] = gCarOrder[i];
        }
    }
    for (i = 0; i < 48; i++) gCarMatrices[i] = ZOID_NewMatrix();
    SetOptions();
    gDriveHulk[0] = 0;
    gDriveHulk[1] = 0;
    gDriveHulk[2] = 0;
    gDriveHulk[3] = 0;
    HFOV = 90.0f;
    func_800A5A40();
    gNumWindows = gNumPlayers;
    if (gNumWindows == 3) gNumWindows = 4;
    if (gRace == 2) {
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
    for (p = 0; p < 4; p++) carscr_pane[p] = InitPane(-20, 0, -10, 10, 192, 0, 0, 0);
    for (p = 0; p < gNumPlayers; p++) {
        pl = &gPlayers[p];
        CopyUV(unitmatrix, gTurntableMat[p][0]);
        CopyUV(unitmatrix, gTurntableMat[p][1]);
        gTurntableIdx[p][0] = MBOX_NewObject(MBOX_FindObject_WORK(carscr_turntable_model_name, 0, gNumModels - 1, 1), gTurntableMat[p][0], -1, 0x40000);
        gTurntableIdx[p][1] = MBOX_NewObject(MBOX_FindObject_WORK(carscr_turntable_base_model_name, 0, gNumModels - 1, 1), gTurntableMat[p][1], -1, 0);
        MBOX_HideObject(gTurntableIdx[p][0], 1, 15);
        MBOX_HideObject(gTurntableIdx[p][1], 1, 15);
        if (gRace == 2) {
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
        for (i = 0; i < 13; i++) {
            gCarHandling[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 0);
            gCarEngines[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 1);
            while (!IsEngineUnlocked(p, gCarEngines[p + 1][i])) {
                gCarEngines[p + 1][i]--;
                if (gCarEngines[p + 1][i] < 0) { gCarEngines[p + 1][i] = 0; break; }
            }
            SavePlayerCarSetting(pl->owner, i, 1, gCarEngines[p + 1][i]);
            gCarTires[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 2);
            while (!IsTiresUnlocked(p, gCarTires[p + 1][i])) {
                gCarTires[p + 1][i]--;
                if (gCarTires[p + 1][i] < 0) { gCarTires[p + 1][i] = 0; break; }
            }
            SavePlayerCarSetting(pl->owner, i, 2, gCarTires[p + 1][i]);
            gCarFrame[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 3);
            while (!IsFrameUnlocked(p, gCarFrame[p + 1][i])) {
                gCarFrame[p + 1][i]--;
                if (gCarFrame[p + 1][i] < 0) { gCarFrame[p + 1][i] = 0; break; }
            }
            SavePlayerCarSetting(pl->owner, i, 3, gCarFrame[p + 1][i]);
            gCarWings[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 4);
            if (gRace != 4 && gCarWings[p + 1][i] > 1) gCarWings[p + 1][i] = 1;
            car_color_choice_a[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 5);
            car_color_choice_b[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 6);
            car_color_choice_c[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 7);
            gTireRims[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 8);
            gCarHorn[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 9);
            gFrontTireScale[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 10) / 100.0f + 0.75f;
            gRearTireScale[p + 1][i] = GetPlayerCarSetting(pl->owner, i, 11) / 100.0f + 0.75f;
            if (!gEnableTireScaling) {
                gFrontTireScale[p + 1][i] = gFrontTireScale[0][i];
                gRearTireScale[p + 1][i] = gRearTireScale[0][i];
            }
        }
        gThisCar[p] = GetPlayerCar(pl->owner);
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
            SavePlayerCar(pl->owner, gThisCar[p]);
        }
        gLastCar[p] = -1;
        calcValidOptions(p);
        for (i = 0; i < gNumCarsActive[p]; i++) {
            carscr_cars[p][i].position = i - gThisCarIndex[p];
            if (carscr_cars[p][i].position < 0) carscr_cars[p][i].position += gNumCarsActive[p];
            carscr_cars[p][i].car = gCars[p][i] + p * 13;
            carscr_cars[p][i].angle = 3.1415927f;
        }
        if (gRace == 6) gPlayers[p].paint = 6;
        else {
            gPlayers[p].paint = GetPlayerCarTrans((void **)pl->owner);
            while (!IsTransUnlocked(p, gPlayers[p].paint)) {
                gPlayers[p].paint -= 2;
                if ((s32)gPlayers[p].paint < 0) { gPlayers[p].paint += 2; break; }
            }
        }
        for (i = 0; i < 13; i++)
            gCarTrans[p + 1][i] = gPlayers[p].paint;
    }
    carscr_blit = NewMultiBlit(0, 0, carscr_mb, 38);
    ZeroCameras();
    UpdateCameras();
    ClearRushControls();
    carscr_inited = 1;
}
void setGhosts(void);
void carscr_InitScreen(void);
void calcStats(s32);
/* Overlay A 0x803A1EAC, 7104 bytes: GC updateCarScreen@carscr.c (MAT3 carscr.c line ~340), the image-A carscr root (kept).
 * Source: reference/repos/mat3 decomp/src/GameSource/src/carscr.c updateCarScreen (typed port by: MAT3 declaration order,
 * per-iteration block locals at the top of the player loop, [5][13] per-player byte tables indexed p+1, u32 gSkipToGameState /
 * PlayStartABSound argument / HAL_GetTextWidth result, nested ?: for d, s8 gLink colour bytes, own purge-loop variable; see
 *). N64 is an earlier revision: 19 options (cases 0.18), no player-file check,
 * pak loop without the gRace==2 test, unscaled projection coordinates, extra SetControls(0) at the end.
 * The ghost-file loop at the top tests the pak first and `continue`s: `if (ghost->file && !PS_PakInserted(.)) continue;
 * if (ghosth == gGhosts[i]) break;` (the MAT3 single `(!file || PakInserted) && ghosth == gGhosts[i]` condition compiles to the
 * same first-loop code).  This decides the tail: uopt's register candidate for &gGhosts (one lda web over the first loop, the
 * case 16..18 slots and the two ghost-compaction tail loops) is split by f_split from the first eligible occurrence in its
 * live-unit list, which follows uopt's flow-graph chain (a DFS that takes each block's first successor first and visits
 * second-successor edges last).  With the single condition the first loop's compare block (bb19) leads the list, so it is the
 * seed; the carved first-loop piece and the remainder are both unprofitable and every tail use is rematerialised (28 words,
 *). With the `continue` form the compare block is reached through the second-successor edge and moves to the END of the
 * list (13, 388, 394, cases 315..320, 19): the seed is the first tail loop, the piece {388, 394} (nocs 3, totalsave 19) is
 * coloured v1 and hoisted in front of each tail loop as in retail, and the remainder keeps splitting harmlessly.
 * Matches in this group (the scorer, strict). Own.rodata verified against decompressed image A: 5/5 float literal sites
 * (rocheck.py) and 19/19 jump-table entries (case offsets relative to the function start).
 */
/* PC: game.frame_rate 60 (pc_game_fps.h). The car select counts the frames since every player was ready (carscr_wait)
 * and leaves at 10, turning the cars to the front over them: once a 30 Hz frame, and the turn's share of what is
 * left, 1 / (frames left - 1) a frame, taken to the frame's length (PC_EASE_DIV(a, n): a / n at original). */
#define PC_EASE_DIV(a, n) (pc_fps60 ? (a) * (1.0f - pc_fps_filter(1.0f - 1.0f / (n))) : (a) / (n))
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
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) { setGhosts(); setGameState(0x40000); return; }
        gSkipToGameState = 1;
    }
    for (i = 1; i < 4; i++) {
        for (ghosth = gGhostFiles.first; ghosth; ghosth = nextGhost) {
            ghost = *ghosth;
            nextGhost = ghost->next;
            if (ghost->file && !PS_PakInserted((*ghost->file)->controller)) continue;
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
        if ((*gPlayers[i].owner)->file && PS_PakChanged((*(*gPlayers[i].owner)->file)->controller)) {
            carscr_UninitScreen(); setGameState(32); carscr_wait = 0; return;
        }
    }
    if (gSoftReset) { carscr_UninitScreen(); setGameState(4); carscr_wait = 0; return; }
    PC_EACH30(carscr_wait++;)
    for (p = 0; p < gNumPlayers; p++) {
        s32 notReadyCount;
        s32 notReadyIdx;
        s32 pIdx;
        s16 x, y, w, h;
        s16 screenPos[2];
        Vec3 msgPos;
        s32 ghostIdx;
        s16 gx, gy, gw, gh;
        f32 pos[3];
        s16 gscreenPos[2];
        if (!allLocked && (gPlayers[p].pressed & 4)) {
            if (gCarLocked[p] == 1) gCarLocked[p] = 0;
            else { AudPlayEffect(38, 0, 1, 0); carscr_UninitScreen(); setGameState(128); carscr_wait = 0; return; }
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
                w = HAL_GetTextWidth(gText.text[151], -1);
                h = TB_GetFontLineHeight() * 2;
                if (notReadyCount == 1) w = (w > HAL_GetTextWidth((*gPlayers[notReadyIdx].owner)->name, -1) ? w : HAL_GetTextWidth((*gPlayers[notReadyIdx].owner)->name, -1));
                else w = (w > HAL_GetTextWidth(gText.text[31], -1) ? w : HAL_GetTextWidth(gText.text[31], -1));
            }
            y -= TB_GetFontLineHeight() / 2;
            ChangePaneSize(carscr_pane[p], x - w / 2 - 4, y - 4, x + w / 2 + 4, y + h + 4);
            carscr_updateButtons(p);
            continue;
        }
        ChangePaneSize(carscr_pane[p], -20, 0, -10, 10);
        if (gPlayers[p].pressed & 3) {
            PlayStartABSound(gPlayers[p].pressed);
            gCarLocked[p] = 1;
            PC_LINK_FLAGS(p) = 160;
            PC_LINK_CAR(p) = gThisCar[p];
            if (gRace == 6 && gCheatState_at_13 == 0) {
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
        if (gPlayers[p].repeated & 0x400) {
            AudPlayEffect(40, 0, 1, 0);
            carscr_gActiveOption[p]--;
            if (carscr_gActiveOption[p] < 0) carscr_gActiveOption[p] = carscr_gNumOptions[p] - 1;
        } else if (gPlayers[p].repeated & 0x800) {
            AudPlayEffect(39, 0, 1, 0);
            carscr_gActiveOption[p]++;
            if (carscr_gActiveOption[p] > carscr_gNumOptions[p] - 1) carscr_gActiveOption[p] = 0;
        } else if (gPlayers[p].repeated & 0x3080) {
            s32 d = (gPlayers[p].repeated & 0x1000) ? -1 : ((gPlayers[p].repeated & 0x2000) ? 1 : 0);
            if (!carscr_gActiveOptionEnabled[p] || (!d && !carscr_gActiveZ[p])) {
                AudPlayEffect(42, 0, 1, 0);
                goto after_options;
            }
            switch (carscr_gOpts[p][carscr_gActiveOption[p]]) {
            case 0:
                if (d) {
                    do {
                        gThisCarIndex[p] += d;
                        if (gThisCarIndex[p] < 0) gThisCarIndex[p] = gNumCarsActive[p] - 1;
                        else if (gThisCarIndex[p] >= gNumCarsActive[p]) gThisCarIndex[p] = 0;
                    } while (!IsCarUnlocked(p, gCars[p][gThisCarIndex[p]]));
                }
                gThisCar[p] = gCars[p][gThisCarIndex[p]];
                if (gRace == 6) gPlayers[p].paint = 6;
                else gPlayers[p].paint = GetPlayerCarTrans((void **)gPlayers[p].owner);
                for (i = 0; i < 13; i++) gCarTrans[p + 1][i] = gPlayers[p].paint;
                calcValidOptions(p);
                carscr_buttonsJustInited[p] = 1;
                break;
            case 1:
                if (gRace != 6) {
                    if (d) {
                        do {
                            gCarTrans[p + 1][gThisCar[p]] += d;
                            if (gCarTrans[p + 1][gThisCar[p]] < 0) gCarTrans[p + 1][gThisCar[p]] = 7;
                            else if (gCarTrans[p + 1][gThisCar[p]] >= 8) gCarTrans[p + 1][gThisCar[p]] = 0;
                        } while (!IsTransUnlocked(p, gCarTrans[p + 1][gThisCar[p]]) || (gRace == 6 && (gCarTrans[p + 1][gThisCar[p]] & 1)));
                    } else gCarTrans[p + 1][gThisCar[p]] = gCarTrans[0][gThisCar[p]];
                    gPlayers[p].paint = gCarTrans[p + 1][gThisCar[p]];
                    SavePlayerCarTrans(gPlayers[p].owner, gPlayers[p].paint);
                } else sound = 0;
                break;
            case 2:
                if (d) {
                    gCarHandling[p + 1][gThisCar[p]] += d;
                    if (gCarHandling[p + 1][gThisCar[p]] < 0) gCarHandling[p + 1][gThisCar[p]] = 2;
                    else if (gCarHandling[p + 1][gThisCar[p]] > 2) gCarHandling[p + 1][gThisCar[p]] = 0;
                } else gCarHandling[p + 1][gThisCar[p]] = gCarHandling[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 0, gCarHandling[p + 1][gThisCar[p]]);
                break;
            case 3:
                if (d) {
                    do {
                        gCarEngines[p + 1][gThisCar[p]] += d;
                        if (gCarEngines[p + 1][gThisCar[p]] < 0) gCarEngines[p + 1][gThisCar[p]] = 8;
                        else if (gCarEngines[p + 1][gThisCar[p]] > 8) gCarEngines[p + 1][gThisCar[p]] = 0;
                    } while (!IsEngineUnlocked(p, gCarEngines[p + 1][gThisCar[p]]));
                } else gCarEngines[p + 1][gThisCar[p]] = gCarEngines[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 1, gCarEngines[p + 1][gThisCar[p]]);
                break;
            case 4:
                if (d) {
                    do {
                        gCarTires[p + 1][gThisCar[p]] += d;
                        if (gCarTires[p + 1][gThisCar[p]] < 0) gCarTires[p + 1][gThisCar[p]] = 4;
                        else if (gCarTires[p + 1][gThisCar[p]] > 4) gCarTires[p + 1][gThisCar[p]] = 0;
                    } while (!IsTiresUnlocked(p, gCarTires[p + 1][gThisCar[p]]));
                } else gCarTires[p + 1][gThisCar[p]] = gCarTires[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 2, gCarTires[p + 1][gThisCar[p]]);
                break;
            case 5:
                if (d) {
                    do {
                        gCarFrame[p + 1][gThisCar[p]] += d;
                        if (gCarFrame[p + 1][gThisCar[p]] < 0) gCarFrame[p + 1][gThisCar[p]] = 5;
                        else if (gCarFrame[p + 1][gThisCar[p]] > 5) gCarFrame[p + 1][gThisCar[p]] = 0;
                    } while (!IsFrameUnlocked(p, gCarFrame[p + 1][gThisCar[p]]));
                } else gCarFrame[p + 1][gThisCar[p]] = gCarFrame[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 3, gCarFrame[p + 1][gThisCar[p]]);
                break;
            case 6:
                if (d) {
                    gCarWings[p + 1][gThisCar[p]] += d;
                    if (gCarWings[p + 1][gThisCar[p]] < 0) gCarWings[p + 1][gThisCar[p]] = (gRace != 4 ? 1 : 2);
                    else if (gCarWings[p + 1][gThisCar[p]] > (gRace != 4 ? 1 : 2)) gCarWings[p + 1][gThisCar[p]] = 0;
                } else gCarWings[p + 1][gThisCar[p]] = gCarWings[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 4, gCarWings[p + 1][gThisCar[p]]);
                break;
            case 7:
                if (d) {
                    car_color_choice_a[p + 1][gThisCar[p]] += d;
                    if (car_color_choice_a[p + 1][gThisCar[p]] < 0) car_color_choice_a[p + 1][gThisCar[p]] = 31;
                    else if (car_color_choice_a[p + 1][gThisCar[p]] >= 32) car_color_choice_a[p + 1][gThisCar[p]] = 0;
                } else car_color_choice_a[p + 1][gThisCar[p]] = car_color_choice_a[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 5, car_color_choice_a[p + 1][gThisCar[p]]);
                break;
            case 8:
                if (d) {
                    car_color_choice_b[p + 1][gThisCar[p]] += d;
                    if (car_color_choice_b[p + 1][gThisCar[p]] < 0) car_color_choice_b[p + 1][gThisCar[p]] = 31;
                    else if (car_color_choice_b[p + 1][gThisCar[p]] >= 32) car_color_choice_b[p + 1][gThisCar[p]] = 0;
                } else car_color_choice_b[p + 1][gThisCar[p]] = car_color_choice_b[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 6, car_color_choice_b[p + 1][gThisCar[p]]);
                break;
            case 9:
                if (d) {
                    car_color_choice_c[p + 1][gThisCar[p]] += d;
                    if (car_color_choice_c[p + 1][gThisCar[p]] < 0) car_color_choice_c[p + 1][gThisCar[p]] = 31;
                    else if (car_color_choice_c[p + 1][gThisCar[p]] >= 32) car_color_choice_c[p + 1][gThisCar[p]] = 0;
                } else car_color_choice_c[p + 1][gThisCar[p]] = car_color_choice_c[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 7, car_color_choice_c[p + 1][gThisCar[p]]);
                break;
            case 10:
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
            case 11:
                if (d) {
                    gTireRims[p + 1][gThisCar[p]] += d;
                    if (gTireRims[p + 1][gThisCar[p]] < 0) gTireRims[p + 1][gThisCar[p]] = 20;
                    else if (gTireRims[p + 1][gThisCar[p]] > 20) gTireRims[p + 1][gThisCar[p]] = 0;
                } else gTireRims[p + 1][gThisCar[p]] = gTireRims[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 8, gTireRims[p + 1][gThisCar[p]]);
                break;
            case 12:
                if (d) {
                    gCarHorn[p + 1][gThisCar[p]] += d;
                    if (gCarHorn[p + 1][gThisCar[p]] < 0) gCarHorn[p + 1][gThisCar[p]] = 9;
                    else if (gCarHorn[p + 1][gThisCar[p]] > 9) gCarHorn[p + 1][gThisCar[p]] = 0;
                } else gCarHorn[p + 1][gThisCar[p]] = gCarHorn[0][gThisCar[p]];
                sound = 0;
                AudStopEffect(gHornSnd[p]);
                gHornSnd[p] = AudPlayEffect(gCarHorn[p + 1][gThisCar[p]] + 26, 0, 1, 0);
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 9, gCarHorn[p + 1][gThisCar[p]]);
                break;
            case 13:
                if (d) {
                    gFrontTireScale[p + 1][gThisCar[p]] += d * 0.05f;
                    if (gFrontTireScale[p + 1][gThisCar[p]] < 0.74f) gFrontTireScale[p + 1][gThisCar[p]] = 1.5f;
                    else if (gFrontTireScale[p + 1][gThisCar[p]] > 1.6f) gFrontTireScale[p + 1][gThisCar[p]] = 0.75f;
                } else gFrontTireScale[p + 1][gThisCar[p]] = gFrontTireScale[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 10, (s8)(100.0f * gFrontTireScale[p + 1][gThisCar[p]] - 75.0f));
                break;
            case 14:
                if (d) {
                    gRearTireScale[p + 1][gThisCar[p]] += d * 0.05f;
                    if (gRearTireScale[p + 1][gThisCar[p]] < 0.74f) gRearTireScale[p + 1][gThisCar[p]] = 1.5f;
                    else if (gRearTireScale[p + 1][gThisCar[p]] > 1.5f) gRearTireScale[p + 1][gThisCar[p]] = 0.75f;
                } else gRearTireScale[p + 1][gThisCar[p]] = gRearTireScale[0][gThisCar[p]];
                SavePlayerCarSetting(gPlayers[p].owner, gThisCar[p], 11, (s8)(100.0f * gRearTireScale[p + 1][gThisCar[p]] - 75.0f));
                break;
            case 15:
                if (d) {
                    gCarSheen[p] += d;
                    if (gCarSheen[p] < 0) gCarSheen[p] = 10;
                    else if (gCarSheen[p] > 10) gCarSheen[p] = 0;
                } else gCarSheen[p] = 0;
                SetSheen(gThisCar[p], p, gCarSheen[p]);
                break;
            case 16: gGhosts[1] = getNextGhost(gGhosts[1], d); break;
            case 17: gGhosts[2] = getNextGhost(gGhosts[2], d); break;
            case 18: gGhosts[3] = getNextGhost(gGhosts[3], d); break;
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
            if (gLastCar[p] >= 0) SavePlayerCar(gPlayers[p].owner, gThisCar[p]);
            gLastCar[p] = gThisCar[p];
        }
        if (gRace == 2) {
            for (ghostIdx = 1; ghostIdx < gNumGhosts + 1; ghostIdx++) {
                if (carscr_pane[ghostIdx]) {
                    pos[0] = carscr_buttonInfo[0][38].position[0];
                    pos[1] = carscr_buttonInfo[0][38].position[1];
                    pos[2] = carscr_buttonInfo[0][38].position[2];
                    pos[1] = -pos[1] / 2.0f;
                    ZOID_ProjectWindow(ghostIdx, pos, &gCamera[ghostIdx], 0, gscreenPos);
                    gx = gscreenPos[0];
                    gy = gscreenPos[1];
                    HAL_SetFont(12);
                    if (!gGhosts[ghostIdx]) {
                        if (!ghosts_enabled) gw = HAL_GetTextWidth(gText.text[225], -1);
                        else gw = HAL_GetTextWidth(gText.text[224], -1);
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
    if (allLocked && carscr_wait >= 10) {
        AudSetMusicVol(0.0f, 3.0f);
        setGameState(0x40000);
        carscr_UninitScreen();
        if (gRace == 3) ContinueCircuit();
        if (gRace == 2) {
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

extern s8 carscr_color_option_masks[];
s32 carscr_validOption(s32 player, s32 item)
{
    s32 available = 1;
    if ((item == 13 || item == 14) && !gEnableTireScaling) available = 0;
    if (!expansion_pak_present) {
        if (item == 11 && gTrackNum >= 0 && gTrackNum < 6) available = 0;
        if (item == 17 || item == 18) available = 0;
    }
    if (!ghosts_enabled && (item == 16 || item == 17 || item == 18)) available = 0;
    if (item == 6 && (gRace == 2 || gRace == 6)) available = 0;
    if ((item == 7 || item == 8 || item == 9) && gRace == 6 && !gCheatState_at_13) available = 0;
    if (item == 7 && !(carscr_color_option_masks[gThisCar[player]] & 1)) available = 0;
    if (item == 8 && !(carscr_color_option_masks[gThisCar[player]] & 2)) available = 0;
    if (item == 9 && !(carscr_color_option_masks[gThisCar[player]] & 4)) available = 0;
    if (item == 10 && gRace != 6) available = 0;
    if ((item == 16 || item == 17 || item == 18) && gRace != 2) available = 0;
    if (item == 12 || item == 15) available = 0;
    return available;
}

void calcValidOptions(s32 player)
{
    s32 item;
    carscr_gNumOptions[player]=0; for(item=0;item<19;item++) {
        if(carscr_validOption(player,item))carscr_gOpts[player][carscr_gNumOptions[player]++]=item;
    }
}

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
        if(record->track==gTrackNum && record->mode==gBackwards &&
           node!=gGhosts[1] && node!=gGhosts[2] && node!=gGhosts[3] &&
           (!(*node)->file || ReadGhostData(node)) && (*node)->unknown24)break;
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

typedef struct CarComponentSlot {s16 handle,auxiliary;u8 unknown04[48];} CarComponentSlot;
extern CarComponentSlot gCarComponents[][11];
extern s32 gCarPartFrame,carscr_buttonsInited[];
extern void UnloadWaveAll(s32);
extern void sound_handles_array_clear(FiveParts *);
extern void ZOID_RemoveObject(s16);
extern void RemoveBlit(Blit *);
extern void removeBG(void);
extern void ZOID_Reset(void);
void clearCarComponents(s32);
void carscr_uninitButtons(s8);
void releaseGridMap(void);
void carscr_UninitScreen(void)
{
    s32 player,count;
    FiveParts **box;
    UnloadWaveAll(62);
    if(gNumPlayers==1 && gRace!=2) releaseGridMap();
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
    func_800A5A40();ZOID_Reset();
    for(player=0;player<gNumPlayers;player++)AudStopEffect(gHornSnd[player]);
    carscr_inited=0;
}

/* clearCarComponents (MAT3 carscr.c, int p): inlined into _UninitScreen; deleted stub 0x8039D044 */
void clearCarComponents(s32 p)
{
    s32 i;
    if (p > 0) return;
    for(i=0;i!=11;i++) {
        if(gCarComponents[p][i].handle!=-1) {ZOID_RemoveObject(gCarComponents[p][i].handle);gCarComponents[p][i].handle=-1;gCarComponents[p][i].auxiliary=-1;}
    }
    if(gCarPartFrame!=-1) {ZOID_RemoveObject(gCarPartFrame);gCarPartFrame=-1;}
}

/* releaseGridMap (MAT3 carscr.c): empty in the N64 build like createGridMap; deleted stub 0x8039D054 */
void releaseGridMap(void)
{
}

/* uninitButtons@carscr.c (MAT3 signed char p): inlined into _UninitScreen; deleted stub 0x8039D04C */
void carscr_uninitButtons(s8 p)
{
    s32 i;
    Slot *slot;
    for(i=0;i!=44;i++) {
        slot=&carscr_buttonInfo[p][i];
        if(slot->handle!=-1) {ZOID_RemoveObject(slot->handle);slot->handle=-1;}
    }
    carscr_buttonsInited[p]=0;
}

typedef union Color4 {u8 c[4];u32 word;} Color4;
extern char *carscr_buttonNames[];
extern char carscr_title_texture_name[],carscr_cursor_texture_name[],carscr_a_button_texture_name[],carscr_b_button_texture_name[];
extern void *memcpy(void *,const void *,u32);
extern void ZOID_SetColor(s16,Color4 *);
extern void *MBOX_FindTexture_WORK(char *,u16 *,s32,s8,s32);
extern void ZOID_SetTexture(s16,void *,s16);
extern void WPitchUV(f32,void *);
extern void WRollUV(f32,void *);
extern void ScaleUV(void *,void *,f32);
/* carscr_initButtons = GC initButtons@carscr.c (MAT3 0x80093b10, 0x564 B): creates the button models of one player's
 * car-select menu and lays out the cursor/A/B arrow records. Direct typed port of the MAT3 body
 * (reference/repos/mat3/decomp/src/GameSource/src/carscr.c) with N64 offsets from the displacements:
 *   - carscr_buttonInfo = buttonInfo[4][44] (64-byte records: kind +0, handle +4, angle +8, 3x3 matrix +0xC, position +0x30,
 *     alpha +0x3C). MAT3 has 40 records (17+17+6); the N64 has 44 (19+19+6), so GC records 34..39 are 38..43 here.
 *   - GC `4.0f +` offsets on records 36..39's y are absent on the N64; strings are the N64 button/cursor names.
 *   - zoom is GC's function-local `static float zoom = 1.0f` (retail 0x803B3060, next to the two colour initialisers
 *     0x803B305C / 0x803B3064); buttonColor = {255,255,255,255}.
 * Private-ABI member of the group: the s8 parameter arrives in the caller's stack home
 * (carscr_InitScreen stores it), the body clobbers s0-s8/f20-f30 without saving, so it only matches inside the group.
 * Levers (both are real source facts; no compiled-out device, no unused local):
 *   - gNumModels is `volatile u8` in the head: every retail use is `lui; addiu; lbu 0(reg)`.
 *   - the early `zoom = 1.0` store is a double literal while `1.0f / zoom` stays float: the two spellings are
 *     different uopt constants, so the store keeps its own $f4 and the divide's 1.0f is not hoisted to $f30.
 * Own data (zoom, the colour initialiser, the float literals) is not verifiable for image A by the scorer (no retail
 * data tree for ovl_a): code identical, own .data/.rodata unverified.
 */
void carscr_initButtons(s8 p)
{
    Color4 buttonColor={{255,255,255,255}};
    void *tex;
    u16 texIdx;
    s32 buttonIdx;
    static f32 zoom=1.0f;
    if(gNumWindows==1)zoom=1.0;
    for(buttonIdx=0;buttonIdx<44;buttonIdx++) {
        if(carscr_buttonInfo[p][buttonIdx].handle==-1) {
            memcpy(carscr_buttonInfo[p][buttonIdx].matrix,unitmatrix,36);
            carscr_buttonInfo[p][buttonIdx].handle=MBOX_NewObject(
                MBOX_FindObject_WORK(carscr_buttonNames[carscr_buttonInfo[p][buttonIdx].kind],0,gNumModels-1,1),
                carscr_buttonInfo[p][buttonIdx].matrix,-1,
                (carscr_buttonInfo[p][buttonIdx].kind==0 || carscr_buttonInfo[p][buttonIdx].kind==1 || carscr_buttonInfo[p][buttonIdx].kind==2) ? 0x42000 : 0);
            ZOID_SetColor((s16)carscr_buttonInfo[p][buttonIdx].handle,&buttonColor);
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx].handle,1,15);
            MBOX_ShowObject(carscr_buttonInfo[p][buttonIdx].handle,0,1<<p);
        }
    }
    tex=MBOX_FindTexture_WORK(carscr_title_texture_name,&texIdx,0,gNumModels-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][38].handle,tex,-1);
    memcpy(carscr_buttonInfo[p][38].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][38].matrix);
    carscr_buttonInfo[p][38].position[0]=0.0f;
    carscr_buttonInfo[p][38].position[1]=gNumWindows>1 ? 105.0f : 115.0f;
    carscr_buttonInfo[p][38].position[2]=200.0f;
    carscr_buttonInfo[p][38].position[0]=(1.0f/zoom)*carscr_buttonInfo[p][38].position[0];
    carscr_buttonInfo[p][38].position[1]=(1.0f/zoom)*carscr_buttonInfo[p][38].position[1];
    carscr_buttonInfo[p][38].position[2]=(1.0f/zoom)*carscr_buttonInfo[p][38].position[2];
    if(gRace==2)MBOX_ShowObject(carscr_buttonInfo[p][38].handle,0,15);
    tex=MBOX_FindTexture_WORK(carscr_cursor_texture_name,&texIdx,0,gNumModels-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][39].handle,tex,-1);
    tex=MBOX_FindTexture_WORK(carscr_a_button_texture_name,&texIdx,0,gNumModels-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][40].handle,tex,-1);
    memcpy(carscr_buttonInfo[p][40].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][40].matrix);
    carscr_buttonInfo[p][40].position[0]=75.1f;
    carscr_buttonInfo[p][40].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][40].position[2]=100.0f;
    ScaleUV(carscr_buttonInfo[p][40].matrix,carscr_buttonInfo[p][40].matrix,0.5f);
    memcpy(carscr_buttonInfo[p][41].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][41].matrix);
    WRollUV(3.1415927f,carscr_buttonInfo[p][41].matrix);
    carscr_buttonInfo[p][41].position[0]=87.1f;
    carscr_buttonInfo[p][41].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][41].position[2]=100.0f;
    tex=MBOX_FindTexture_WORK(carscr_b_button_texture_name,&texIdx,0,gNumModels-1,1);
    ZOID_SetTexture((s16)carscr_buttonInfo[p][42].handle,tex,-1);
    memcpy(carscr_buttonInfo[p][42].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][42].matrix);
    carscr_buttonInfo[p][42].position[0]=-75.1f;
    carscr_buttonInfo[p][42].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][42].position[2]=100.0f;
    carscr_buttonInfo[p][42].position[0]=(1.0f/zoom)*carscr_buttonInfo[p][42].position[0];
    carscr_buttonInfo[p][42].position[1]=(1.0f/zoom)*carscr_buttonInfo[p][42].position[1];
    carscr_buttonInfo[p][42].position[2]=(1.0f/zoom)*carscr_buttonInfo[p][42].position[2];
    ScaleUV(carscr_buttonInfo[p][42].matrix,carscr_buttonInfo[p][42].matrix,0.5f);
    memcpy(carscr_buttonInfo[p][43].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,carscr_buttonInfo[p][43].matrix);
    carscr_buttonInfo[p][43].position[0]=-87.1f;
    carscr_buttonInfo[p][43].position[1]=gNumWindows>1 ? 50.2f : 55.2f;
    carscr_buttonInfo[p][43].position[2]=100.0f;
    carscr_buttonInfo[p][43].position[0]=(1.0f/zoom)*carscr_buttonInfo[p][43].position[0];
    carscr_buttonInfo[p][43].position[1]=(1.0f/zoom)*carscr_buttonInfo[p][43].position[1];
    carscr_buttonInfo[p][43].position[2]=(1.0f/zoom)*carscr_buttonInfo[p][43].position[2];
    carscr_buttonsInited[p]=1;
    carscr_buttonsJustInited[p]=1;
    ZeroCameras();
}


extern f32 fabsf(f32);
#pragma intrinsic(fabsf)
extern volatile f32 gFrameSeconds;
extern char carscr_select_texture_name[],carscr_button_texture_name[];
/* carscr_updateButtons = GC updateButtons@carscr.c (MAT3 0x80094108, 0x8b8 B): per-frame scroll of the car-select option
 * buttons. Direct typed port of the MAT3 body with the N64 differences read from the retail displacements:
 *   - 19 option records per side (GC 17), right-hand copies at +19; cursor is record 39 (GC 35), A/B arrows 40..43.
 *   - unselected buttons settle to angle 0.0f (GC 0.08726646f); alpha ramps linearly over one button pitch
 *     (255.0f - d * 255 / (gButtonSpace * gButtonScale)) instead of GC's fixed 80.
 *   - validOption is carscr_validOption (member of this group); cursorSpeed / scrollSpeed are the function-local
 *     statics (retail .bss 0x803BA200 / 0x803BA210); gFrameSeconds is `volatile f32` (gFrameSeconds).
 * Shaping that is real source, not a device:
 *   - comparisons with a fabsf() call are written `speed < fabsf(...) * 60 ...` (the call on the right): with the call
 *     on the left uopt reserves a 4-byte temporary in the local area (area 45 -> 41 here) and the colour/texIdx
 *     slots land 4 B too high. Unlike the optionscr sibling the clamps keep the GC `?:` form.
 *   - both alpha ramps spell the second 255 as the int `255` (and the first as 255.0f): that splits the 255.0f
 *     constant into two low-priority webs, so it is rematerialised as in retail instead of taking $f28 and
 *     starving the hoisted 1.5707964f ($f30, shared with the pre-loop WPitchUV call).
 *   - `mask = 1 << p` is the one named local beyond the MAT3 list: the retail local area has one more 4-byte local
 *     than the MAT3 declarations (texIdx at sp+114 and color at sp+108 instead of 118/112, homes unchanged at
 *     area 45). It is used (three times, as the shared model mask); an unused `s32 i` in the same slot gives the
 *     identical object.
 *   - `carscr_buttonInfo[idx+19].alpha = buttonColor.c[3] = carscr_buttonInfo[idx].alpha` (colour byte stored first).
 * Own .bss (the two statics), .data (colour initialiser) and .rodata floats are unverified for image A.
 */
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
    memcpy(prevPos,carscr_buttonInfo[p][39].position,12);
    carscr_buttonInfo[p][39].angle+=6.2831855f*gFrameSeconds;
    if(carscr_buttonInfo[p][39].angle>6.2831855f)carscr_buttonInfo[p][39].angle-=6.2831855f;
    carscr_buttonInfo[p][39].position[0]=carscr_buttonX*carscr_cursorZ/carscr_buttonZ+-170.0f*carscr_gButtonScale;
    carscr_buttonInfo[p][39].position[1]=selectedPos*carscr_cursorZ/carscr_buttonZ;
    carscr_buttonInfo[p][39].position[2]=carscr_cursorZ;
    if(!carscr_buttonsJustInited[p]) {
        if(cursorSpeed[p]<fabsf(carscr_buttonInfo[p][39].position[1]-prevPos[1])*60.0f*carscr_gButtonScale/10.0f) {
            cursorSpeed[p]=fabsf(carscr_buttonInfo[p][39].position[1]-prevPos[1])*60.0f*carscr_gButtonScale/10.0f;
            cursorSpeed[p]=cursorSpeed[p]<540.0f*carscr_gButtonScale ? cursorSpeed[p] : 540.0f*carscr_gButtonScale;
            cursorSpeed[p]=cursorSpeed[p]>180.0f*carscr_gButtonScale ? cursorSpeed[p] : 180.0f*carscr_gButtonScale;
        }
        if(carscr_buttonInfo[p][39].position[1]>prevPos[1]+gFrameSeconds*cursorSpeed[p])
            carscr_buttonInfo[p][39].position[1]=prevPos[1]+gFrameSeconds*cursorSpeed[p];
        else if(carscr_buttonInfo[p][39].position[1]<prevPos[1]-gFrameSeconds*cursorSpeed[p])
            carscr_buttonInfo[p][39].position[1]=prevPos[1]-gFrameSeconds*cursorSpeed[p];
        else cursorSpeed[p]=180.0f*carscr_gButtonScale;
    }
    ScaleUV(unitmatrix,carscr_buttonInfo[p][39].matrix,carscr_gButtonScale*0.4f);
    WRollUV(carscr_buttonInfo[p][39].angle,carscr_buttonInfo[p][39].matrix);
    WPitchUV(1.5707964f,carscr_buttonInfo[p][39].matrix);
    buttonY=selectedPos+carscr_gActiveOption[p]*carscr_gButtonSpace*carscr_gButtonScale;
    for(buttonIdx=0;buttonIdx<19;buttonIdx++) {
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
        carscr_buttonInfo[p][buttonIdx+19].position[0]=120.0f*carscr_gButtonScale+carscr_buttonX;
        carscr_buttonInfo[p][buttonIdx+19].position[1]=carscr_buttonInfo[p][buttonIdx].position[1];
        carscr_buttonInfo[p][buttonIdx+19].position[2]=carscr_buttonInfo[p][buttonIdx].position[2];
        carscr_buttonInfo[p][buttonIdx+19].angle=carscr_buttonInfo[p][buttonIdx].angle;
        ScaleUV(unitmatrix,carscr_buttonInfo[p][buttonIdx].matrix,carscr_gButtonScale);
        ScaleUV(unitmatrix,carscr_buttonInfo[p][buttonIdx+19].matrix,carscr_gButtonScale);
        WPitchUV(carscr_buttonInfo[p][buttonIdx].angle-1.5707964f,carscr_buttonInfo[p][buttonIdx].matrix);
        WPitchUV(carscr_buttonInfo[p][buttonIdx+19].angle-1.5707964f,carscr_buttonInfo[p][buttonIdx+19].matrix);
        if(carscr_buttonInfo[p][buttonIdx].angle>1.5707964f)tex=MBOX_FindTexture_WORK(carscr_select_texture_name,&texIdx,0,gNumModels-1,1);
        else tex=MBOX_FindTexture_WORK(carscr_button_texture_name,&texIdx,0,gNumModels-1,1);
        ZOID_SetTexture((s16)carscr_buttonInfo[p][buttonIdx].handle,tex,-1);
        ZOID_SetTexture((s16)carscr_buttonInfo[p][buttonIdx+19].handle,tex,-1);
        if(carscr_buttonInfo[p][buttonIdx].position[1]>carscr_maxY+carscr_gButtonSpace*carscr_gButtonScale || carscr_buttonInfo[p][buttonIdx].position[1]<carscr_minY-carscr_gButtonSpace*carscr_gButtonScale)carscr_buttonInfo[p][buttonIdx].alpha=0;
        else if(carscr_buttonInfo[p][buttonIdx].position[1]>carscr_maxY)carscr_buttonInfo[p][buttonIdx].alpha=255.0f-(carscr_buttonInfo[p][buttonIdx].position[1]-carscr_maxY)*255/(carscr_gButtonSpace*carscr_gButtonScale);
        else if(carscr_buttonInfo[p][buttonIdx].position[1]<carscr_minY)carscr_buttonInfo[p][buttonIdx].alpha=255.0f-(carscr_minY-carscr_buttonInfo[p][buttonIdx].position[1])*255/(carscr_gButtonSpace*carscr_gButtonScale);
        else carscr_buttonInfo[p][buttonIdx].alpha=255;
        if(!carscr_validOption(p,buttonIdx))carscr_buttonInfo[p][buttonIdx].alpha=0;
        carscr_buttonInfo[p][buttonIdx+19].alpha=buttonColor.c[3]=carscr_buttonInfo[p][buttonIdx].alpha;
        ZOID_SetColor((s16)carscr_buttonInfo[p][buttonIdx].handle,&buttonColor);
        ZOID_SetColor((s16)carscr_buttonInfo[p][buttonIdx+19].handle,&buttonColor);
        if(!carscr_validOption(p,buttonIdx)) {
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx].handle,1,15);
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx+19].handle,1,15);
        } else if(!carscr_buttonInfo[p][buttonIdx].alpha || gCarLocked[p]==1) {
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx].handle,1,15);
            MBOX_HideObject(carscr_buttonInfo[p][buttonIdx+19].handle,1,15);
            buttonY-=carscr_gButtonSpace*carscr_gButtonScale;
        } else {
            mask=1<<p;
            MBOX_ShowObject(carscr_buttonInfo[p][buttonIdx].handle,0,mask);
            MBOX_ShowObject(carscr_buttonInfo[p][buttonIdx+19].handle,0,mask);
            buttonY-=carscr_gButtonSpace*carscr_gButtonScale;
        }
    }
    if(gCarLocked[p]==1) {
        MBOX_HideObject(carscr_buttonInfo[p][40].handle,1,15);
        MBOX_HideObject(carscr_buttonInfo[p][41].handle,1,15);
        carscr_buttonInfo[p][39].position[2]=2000.0f;
    } else {
        mask=1<<p;
        MBOX_ShowObject(carscr_buttonInfo[p][40].handle,0,mask);
        MBOX_ShowObject(carscr_buttonInfo[p][41].handle,0,mask);
    }
    if(allLocked==1) {
        MBOX_HideObject(carscr_buttonInfo[p][42].handle,1,15);
        MBOX_HideObject(carscr_buttonInfo[p][43].handle,1,15);
    } else {
        mask=1<<p;
        MBOX_ShowObject(carscr_buttonInfo[p][42].handle,0,mask);
        MBOX_ShowObject(carscr_buttonInfo[p][43].handle,0,mask);
    }
    if(clearScrollSpeed==1)scrollSpeed[p]=0.0f;
    carscr_buttonsJustInited[p]=0;
}

typedef struct CarHandles {s32 main;u8 unknown04[12];s32 parts[7];u8 unknown2C[20];} CarHandles;
extern CarHandles gCarParts[];
extern Color4 carscr_solidColor,carscr_normalColor;
void showGridMap(s32 enabled)
{
    if(enabled==1) {
        Color4 color=carscr_solidColor;
        if(gCarParts[gThisCar[0]].main) {
            ZOID_SetColor(gCarParts[gThisCar[0]].main,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[0],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[1],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[3],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[4],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[5],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[6],&color);
        }
    } else {
        Color4 color=carscr_normalColor;
        if(gCarParts[gThisCar[0]].main) {
            ZOID_SetColor(gCarParts[gThisCar[0]].main,&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[0],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[1],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[3],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[4],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[5],&color);
            ZOID_SetColor(gCarParts[gThisCar[0]].parts[6],&color);
        }
    }
}

/* updateCarComponents = updateCarComponents@carscr.c (MAT3 reference/repos/mat3/decomp/src/GameSource/src/carscr.c, GC 0x800910F8,
 * size 0x830), : direct typed port of the MAT3 body, replacing context stand-. Private-ABI member: p arrives in s6
 * from its caller showCarSelect; every callee-saved register is clobbered.
 * Names: gCarComponents = gCarComponents ([4][11] x 52 B: idx, level, mat), compLight = compLight (2 x 40 B, Lights2 layout),
 * componentToName = componentToName (s16[11]), carComponentNames, gCarPartFrame, gCarParts
 * (CarHandles; frame = main, tires[k] = parts[3+k]), ObjInstanceList = ObjInstanceList_mat (68 B), MBOX_HideObject MBOX_HideObject,
 * ZOID_SetObjectParent ZOID_SetObjectParent, MBOX_NewObjectCam MBOX_NewObjectCam, MBOX_NewObject MBOX_NewObject, MBOX_FindObject_WORK
 * MBOX_FindObject_WORK, ZOID_FindMatrix ZOID_FindMatrix, TB_GetFlashTextCycle TB_GetFlashTextCycle, BodyVector BodyVector,
 * ZOID_RemoveObject ZOID_RemoveObject, ScaleUV ScaleUV, YawUV YawUV, MBOX_ShowObject MBOX_ShowObject,
 * ZOID_SetObjLights ZOID_SetObjLights, gThisCar gThisCar, gOpts carscr_gOpts, gActiveOption carscr_gActiveOption, gCarLocked gCarLocked.
 * N64 differences from the GC body (all from the retail words):
 *  - compLight is the N64 Lights2 (bytes): the flash writes compLight[1] ambient col[0] = 255.0f * cycle (u8), and both
 *    lights' dir bytes are (s8)(lightVec[k] * 10.0f) computed from lightVec itself; BodyVector still writes a scratch
 *    vector (lightDir, never read: the retail reloads lightVec after the call). No ZOID_SetColor / buttonColor, no printf.
 *  - compLight is DEFINED here (initialiser = image A bytes at 0x803B2858): the shared `lui at` of the six dir-byte stores
 * only appears when the symbol is defined in the unit (rule); as extern every store gets its own lui.
 *  - the early-out is one condition (`locked || option not in 1..5`); the MAT3 goto chain gives other register webs.
 *  - MBOX_FindObject_WORK's 4th parameter is int (MAT3 rush_protos.h): retail passes the shared constant-1 register
 * unnarrowed. This changed the shared prototype of MBOX_FindObject_WORK; all old members still MATCH.
 *  - the case 0/1/10 zero is spelled 0.f (its own constant, retail $f30; the gNumWindows block zero is 0.0f in $f26),
 *    the two YawUV arguments 3.1415927f / 3.14159265f (two literals, neither hoisted, as retail's two rodata words).
 *  - loop top order currentLevel, whichLight, compScale (as1 tie), and `mask = 1 << p;` is a named local assigned just
 *    before MBOX_ShowObject: its declared slot is the one 4-byte local the retail frame has beyond the MAT3 list
 *    (frame 144; without it 136, every home 4/8 bytes low). The variable is used, not padding.
 * Own data checked against image A (tools/owncheck2.py): lightVec template 0x803B303C, compLight 0x803B2858 (80 B),
 * 0.1f 0x803B9628, pi 0x803B966C/0x803B9670, jump tables 0x803B962C (5) and 0x803B9640 (11), all entries equal. */
typedef struct CarComponent { s16 idx, level; f32 uvs[9]; f32 pos[3]; } CarComponent;
#define gCarComponents ((CarComponent (*)[11])gCarComponents)
typedef struct ComponentLight { u8 col[3], pad0, colc[3], pad1; s8 dir[3], pad2; u8 pad3[4]; } ComponentLight;
typedef struct ComponentLights { u8 col[3], pad0, colc[3], pad1; ComponentLight l[2]; } ComponentLights;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * ObjInstanceList_mat is node 0's object slot (N64 +0x08): on the PC, a field of the node. */
typedef struct ObjInstance { u32 flags, w4; void *mat; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } ObjInstance;
#define ObjInstanceList_mat ObjInstanceList
typedef struct ZMatrix { f32 uvs[9]; f32 pos[3]; } ZMatrix;
ComponentLights compLight[2] = {
    {{0, 128, 0}, 0, {0, 128, 0}, 0, {{{192, 192, 192}, 0, {192, 192, 192}, 0, {0, 100, 0}, 0}, {{0}}}},
    {{255, 0, 0}, 0, {255, 0, 0}, 0, {{{192, 192, 192}, 0, {192, 192, 192}, 0, {0, 100, 0}, 0}, {{0}}}},
};
extern s16 componentToName[11];
extern char *carComponentNames[5];
extern char carscr_part_frame_model_name[];
extern ObjInstance ObjInstanceList_mat[];
extern void BodyVector(f32 *, f32 *, f32 *);
extern f32 TB_GetFlashTextCycle(void);
extern s32 MBOX_NewObjectCam(s32, void *, s32, s32, s32);
extern s32 sprintf(char *, const char *, ...);
extern void ZOID_SetObjectParent(s32, s32);
extern void YawUV(f32, void *);
extern void ZOID_SetObjLights(s32, s32, ComponentLights *);
extern ZMatrix *ZOID_FindMatrix(s16);
void updateCarComponents(s32 p)
{
    s32 i;
    f32 lightVec[3] = {-0.57f, 0.57f, -0.57f};
    f32 lightDir[3];
    CarHandles *car;
    s16 currentLevel;
    s32 whichLight;
    f32 compScale;
    char componentName[16];
    s32 mask;
    if (gNumPlayers > 1 || gRace == 2) return;
    if (gCarLocked[p] == 1 || (carscr_gOpts[p][carscr_gActiveOption[p]] != 1 && carscr_gOpts[p][carscr_gActiveOption[p]] != 2 && carscr_gOpts[p][carscr_gActiveOption[p]] != 3 && carscr_gOpts[p][carscr_gActiveOption[p]] != 4 && carscr_gOpts[p][carscr_gActiveOption[p]] != 5)) {
        for (i = 0; i < 11; i++) {
            if (gCarComponents[p][i].idx != -1) {
                MBOX_HideObject(gCarComponents[p][i].idx, 1, 15);
                ZOID_SetObjectParent(gCarComponents[p][i].idx, -1);
            }
        }
        return;
    }
    car = &gCarParts[p * 13 + gThisCar[p]];
    if (gCarPartFrame == -1) {
        gCarPartFrame = MBOX_NewObjectCam(MBOX_FindObject_WORK(carscr_part_frame_model_name, 0, gNumModels - 1, 1), ZOID_FindMatrix((s16)car->main), 15, -1, 0);
        MBOX_HideObject(gCarPartFrame, 1, 15);
    } else {
        ObjInstanceList_mat[gCarPartFrame].mat = ZOID_FindMatrix((s16)car->main);
    }
    compLight[1].col[0] = 255.0f * TB_GetFlashTextCycle();
    BodyVector(lightVec, lightDir, ZOID_FindMatrix((s16)car->main)->uvs);
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
        case 0:
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == 3) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarEngines[p+1][gThisCar[p]] % 5;
            break;
        case 1:
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == 5) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarFrame[p+1][gThisCar[p]] % 3;
            break;
        case 2:
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == 2) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarHandling[p+1][gThisCar[p]] % 3;
            break;
        case 3:
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == 4) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarTires[p+1][gThisCar[p]] % 4;
            break;
        case 4:
            if (carscr_gOpts[p][carscr_gActiveOption[p]] == 1) {
                whichLight = 1;
                compScale = 0.1f * TB_GetFlashTextCycle() + 1.0f;
            }
            currentLevel = gCarTrans[p+1][gThisCar[p]] % 2;
            break;
        }
        if (currentLevel != gCarComponents[p][i].level) {
            sprintf(componentName, carComponentNames[componentToName[i]], currentLevel + 1);
            if (gCarComponents[p][i].idx != -1) {
                ZOID_RemoveObject(gCarComponents[p][i].idx);
                gCarComponents[p][i].idx = -1;
                gCarComponents[p][i].level = -1;
            }
            gCarComponents[p][i].idx = MBOX_NewObject(MBOX_FindObject_WORK(componentName, 0, gNumModels - 1, 1), gCarComponents[p][i].uvs, -1, 0);
            MBOX_HideObject(gCarComponents[p][i].idx, 1, 15);
            if (gNumWindows == 1) {
                gCarComponents[p][i].pos[0] = 0.0f;
                gCarComponents[p][i].pos[1] = 1.0f;
                gCarComponents[p][i].pos[2] = 15.0f;
            } else {
                gCarComponents[p][i].pos[0] = 0.0f;
                gCarComponents[p][i].pos[1] = 2.0f;
                gCarComponents[p][i].pos[2] = 15.0f;
            }
            gCarComponents[p][i].level = currentLevel;
        }
        switch (i) {
        case 0: case 1: case 10:
            ScaleUV(unitmatrix, gCarComponents[p][i].uvs, compScale);
            gCarComponents[p][i].pos[0] = 0.f;
            gCarComponents[p][i].pos[1] = 0.f;
            gCarComponents[p][i].pos[2] = 0.f;
            gCarComponents[p][i].pos[1] += 0.1f;
            break;
        case 2: case 6:
            ScaleUV(ZOID_FindMatrix((s16)car->parts[3])->uvs, gCarComponents[p][i].uvs, compScale);
            gCarComponents[p][i].pos[0] = ZOID_FindMatrix((s16)car->parts[3])->pos[0];
            gCarComponents[p][i].pos[1] = ZOID_FindMatrix((s16)car->parts[3])->pos[1];
            gCarComponents[p][i].pos[2] = ZOID_FindMatrix((s16)car->parts[3])->pos[2];
            YawUV(3.1415927f, gCarComponents[p][i].uvs);
            break;
        case 3: case 7:
            ScaleUV(ZOID_FindMatrix((s16)car->parts[4])->uvs, gCarComponents[p][i].uvs, compScale);
            gCarComponents[p][i].pos[0] = ZOID_FindMatrix((s16)car->parts[4])->pos[0];
            gCarComponents[p][i].pos[1] = ZOID_FindMatrix((s16)car->parts[4])->pos[1];
            gCarComponents[p][i].pos[2] = ZOID_FindMatrix((s16)car->parts[4])->pos[2];
            break;
        case 5: case 9:
            ScaleUV(ZOID_FindMatrix((s16)car->parts[5])->uvs, gCarComponents[p][i].uvs, compScale);
            gCarComponents[p][i].pos[0] = ZOID_FindMatrix((s16)car->parts[5])->pos[0];
            gCarComponents[p][i].pos[1] = ZOID_FindMatrix((s16)car->parts[5])->pos[1];
            gCarComponents[p][i].pos[2] = ZOID_FindMatrix((s16)car->parts[5])->pos[2];
            YawUV(3.1415927f, gCarComponents[p][i].uvs);
            break;
        case 4: case 8:
            ScaleUV(ZOID_FindMatrix((s16)car->parts[6])->uvs, gCarComponents[p][i].uvs, compScale);
            gCarComponents[p][i].pos[0] = ZOID_FindMatrix((s16)car->parts[6])->pos[0];
            gCarComponents[p][i].pos[1] = ZOID_FindMatrix((s16)car->parts[6])->pos[1];
            gCarComponents[p][i].pos[2] = ZOID_FindMatrix((s16)car->parts[6])->pos[2];
            break;
        }
        ZOID_SetObjectParent(gCarComponents[p][i].idx, (s16)gCarPartFrame);
        mask = 1 << p;
        MBOX_ShowObject(gCarComponents[p][i].idx, 0, mask);
        ZOID_SetObjLights(gCarComponents[p][i].idx, 1, &compLight[whichLight]);
    }
}


/* Overlay A 0x8039EF08, 4092 bytes: GC showCarSelect@carscr.c (MAT3 0x8008eddc, 0xb28 B), static.
 * Source: reference/repos/mat3 decomp/src/GameSource/src/carscr.c showCarSelect (typed port).  The MAT3 local order
 * (uv,i,p,carPos,alpha,framesLeft,reflectIdx,reflectUV,reflectPos,ghostIdx,ghostCar,show) reproduces the retail frame offsets
 * uv@212 i@208 p@204 carPos@192 reflectUV@144 reflectPos@132 exactly.  The N64 build is an earlier revision than GC: selection
 * fades (`+= 5.0f * gFrameSeconds`), the "face front" mode is gOpts==11 (GC 9), reflection alpha is alpha/1.5f, no alpha/4,
 * UpdateSelectCar takes 9 args (slot,p,paint,rims,horn,sheen,pos,uv,alpha), showGridMap(boolean) replaces showGridMap and
 * updateCarComponents is updateCarComponents(p) (register parameter $s6, clobbers every callee-saved register).
 * Shaping, all verified on the builder (group cleanup_colortrace):
 *  - the two `gThisCarIndex[p] == i` reads are volatile (cast): retail does not share the load between them, and a shared
 *    load becomes a coloured web that steals $v0 and moves p out of $s6 (nine code-free `if (p) {}` reads gave the same
 *    allocation before this was found; they are gone);
 *  - `if (!gGhosts[ghostIdx]) continue;`: the loop test `n + 1` is then kept in both paths, as in retail (the if-block form
 *    folds it to `n < ghostIdx`);
 *  - `0.f` for the angle>0 compare and `3.14159265f` for the RollUV argument give retail's separate constants/rodata entries;
 * - no unused locals: MAT3 calls createGridMap at the top of the `gNumPlayers == 1 && gRace != 2` block (
 *    `show`, which is block-local as in MAT3).  In this N64 revision createGridMap is EMPTY: createGridMap (the caller-less
 *    `jr ra; nop` right before this function) is it, defined below as a non-static, non-kept function so umerge inlines it
 * here and still emits its 8-byte body with a symbol. The inlined call reserves the 8 frame bytes that had to fill
 *    with two unused locals (xx1, xx2), which are gone; frame 248 as retail.
 * Needs updateCarComponents in the group (stand-in context): without it the caller keeps its registers across the call.
 * - the `angle > 1.5707964f` test and its then-statement are on separate source lines: as1's cross-block pull fills the
 *    stall slots of the block after `angle -= 1.5707964f` with the compare's `lui at` and the then-arm's `&gFrameSeconds`
 *    (`lui t9; addiu t9`), and breaks the aftercycles tie between `lui at` and `addiu t9` by source line.  On one line the
 * tie went to `addiu t9` (2 words off); on two lines `lui at` (the compare, earlier line) goes first, as in retail.
 * Matches in this group (the scorer, strict). Own.rodata verified by hand against decompressed image A (33/33 literal
 * sites, rocheck.py).
 */
#define CAR_MIN(a,b) ((a) < (b) ? (a) : (b))
extern void UpdateSelectCar(s16,s16,u8,u8,u8,s16,f32 *,f32 (*)[3],u32);
extern void func_80090F44(f32,void *);
extern void YawUV(f32,void *);
extern void func_8009EA68(f32,void *);
extern void ScaleUV(void *,void *,f32);
extern s8 gCarTeamColor[];
/* Overlay A 0x8039EF00, 8 bytes: GC createGridMap@carscr.c, empty in this N64 revision (MAT3 builds the grid display list).
 * Called (and inlined) only by showCarSelect; non-static so its deleted-procedure body keeps a symbol (packaging rule). */
void createGridMap(void)
{
}
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
            if (((volatile s8 *)gThisCarIndex)[p] == i) {
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
            } else if (carscr_gOpts[p][carscr_gActiveOption[p]] == 11) {
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
            func_80090F44(0.34906584f, uv);
            YawUV(carscr_cars[p][i].angle, uv);
            if (carscr_cars[p][i].car % 13 == 10) {
                if (gNumWindows == 1) { carPos[0] = 0.0f; carPos[1] = 3.2f; carPos[2] = 40.5f; }
                else { carPos[0] = 0.0f; carPos[1] = 5.9f; carPos[2] = 40.5f; }
            } else {
                if (gNumWindows == 1) { carPos[0] = 0.0f; carPos[1] = 2.6f; carPos[2] = 40.5f; }
                else { carPos[0] = 0.0f; carPos[1] = 5.3f; carPos[2] = 40.5f; }
            }
            alpha = carscr_cars[p][i].selection * 255.0f;
            if (gRace == 6 && gCheatState_at_13 == 0)
                UpdateSelectCar(carscr_cars[p][i].car, p, gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarSheen[p], carPos, uv, alpha);
            else UpdateSelectCar(carscr_cars[p][i].car, p, car_color_choice_a[p + 1][carscr_cars[p][i].car % 13], car_color_choice_b[p + 1][carscr_cars[p][i].car % 13], car_color_choice_c[p + 1][carscr_cars[p][i].car % 13], gCarSheen[p], carPos, uv, alpha);
            if (gNumPlayers < 3 && gRace != 2) {
                reflectIdx = p == 0 ? 2 : 3;
                ScaleUV(uv, reflectUV, 1.2f);
                func_8009EA68(3.14159265f, reflectUV);
                reflectPos[0] = 1.2f * carPos[0];
                reflectPos[1] = 1.2f * carPos[1];
                reflectPos[2] = 1.2f * carPos[2];
                if (carscr_cars[p][i].car % 13 == 10) reflectPos[1] -= 4.0f;
                else reflectPos[1] -= 2.5f;
                carscr_cars[reflectIdx][i].car = carscr_cars[p][i].car + 26;
                if (gRace == 6 && gCheatState_at_13 == 0)
                    UpdateSelectCar(carscr_cars[reflectIdx][i].car, reflectIdx, gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarTeamColor[gCarTeam[p]], gCarSheen[p], reflectPos, reflectUV, alpha / 1.5f);
                else UpdateSelectCar(carscr_cars[reflectIdx][i].car, reflectIdx, car_color_choice_a[p + 1][carscr_cars[p][i].car % 13], car_color_choice_b[p + 1][carscr_cars[p][i].car % 13], car_color_choice_c[p + 1][carscr_cars[p][i].car % 13], gCarSheen[p], reflectPos, reflectUV, alpha / 1.5f);
            }
            if (((volatile s8 *)gThisCarIndex)[p] == i) {
                ScaleUV(unitmatrix, gTurntableMat[p][0], 1.3f);
                ScaleUV(unitmatrix, gTurntableMat[p][1], 1.3f);
                func_80090F44(0.34906584f, gTurntableMat[p][0]);
                func_80090F44(0.34906584f, gTurntableMat[p][1]);
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
                if (gRace == 2) {
                    for (ghostIdx = 1; ghostIdx < gNumGhosts + 1; ghostIdx++) {
                        if (!gGhosts[ghostIdx] || gLastCar[ghostIdx] != (*(*gGhosts[ghostIdx])->active)->car) {
                            if (gLastCar[ghostIdx] != -1) UpdateSelectCar(carscr_cars[ghostIdx][gLastCar[ghostIdx]].car, ghostIdx, 0, 0, 0, 0, carPos, uv, 0);
                        }
                        if (!gGhosts[ghostIdx]) continue;
                        ghostCar = (*(*gGhosts[ghostIdx])->active)->car;
                        gLastCar[ghostIdx] = ghostCar;
                        carscr_cars[ghostIdx][ghostCar].car = ghostCar + ghostIdx * 13;
                        gCarTires[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->active)->component;
                        gTireRims[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->active)->part;
                        gFrontTireScale[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->active)->stat0 / 100.0f + 0.75f;
                        gRearTireScale[ghostIdx + 1][ghostCar] = (*(*gGhosts[ghostIdx])->active)->stat1 / 100.0f + 0.75f;
                        UpdateSelectCar(carscr_cars[ghostIdx][ghostCar].car, ghostIdx, (*(*gGhosts[ghostIdx])->active)->red, (*(*gGhosts[ghostIdx])->active)->green, (*(*gGhosts[ghostIdx])->active)->blue, 0, carPos, uv, 255);
                    }
                }
            }
        }
        if (gNumPlayers == 1 && gRace != 2) {
            s32 show;
            createGridMap();
            show = !gCarLocked[0] && (carscr_gOpts[p][carscr_gActiveOption[p]] == 1 || carscr_gOpts[p][carscr_gActiveOption[p]] == 2 || carscr_gOpts[p][carscr_gActiveOption[p]] == 3 || carscr_gOpts[p][carscr_gActiveOption[p]] == 4 || carscr_gOpts[p][carscr_gActiveOption[p]] == 5);
            showGridMap(show);
            updateCarComponents(p);
        }
    }
}
