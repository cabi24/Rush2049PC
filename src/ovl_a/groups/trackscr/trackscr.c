/* The track select screen (MAT3 trackscr.c), GS_TRACK: a carousel of the tracks of the race type (their pictures
 * over a fog layer), a rotating 3D map of the highlighted track (for the race tracks drawn from their paths, for the
 * others a model), and a scrolling list of 3D buttons with the race options (laps, ghosts, direction, mirror, fog,
 * wind, drones, difficulty, catch-up, death mode and the stunt and battle settings). The options are gOptions[21..40]
 * and are kept in the first player's profile. A or Start goes on to the car select screen, B back to the mode (or
 * circuit) screen. */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef struct Blit Blit;
typedef struct MultiBlit {
    char *name;s16 x,y,width,height,top,bottom,left,right;
    u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;
} MultiBlit;
typedef struct tMenuButton {s32 type,handle;f32 angle,matrix[3][3],position[3];u8 alpha,pad[3];} tMenuButton;
typedef union Color {u32 word;u8 channel[4];} Color;
typedef struct Texture Texture;
/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10). */
typedef struct FileInfo {void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak;} FileInfo;
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): fileh NULL for a guest, stats its
 * save data. */
typedef struct Profile Profile;
struct Profile {Profile **next,**previous;FileInfo **fileh;u32 id0,id1;u8 name[24];u8 **stats;};
/* A player's pad record (MAT3 tPadState, gPlayers): its buttons and its profile. */
typedef struct Player {u8 slot,controller,pad02[2];u32 pressed,levels,repeated;u8 unknown10[56];Profile **save;} Player;
/* A circuit's race (gCircuitTracks): the track and its set direction, mirror, fog and wind. */
typedef struct CircuitTrack {u8 trackNum,unknown1,mirror,fog,wind;} CircuitTrack;

/* trackscr_gOpts[]: the race options, in their order down the list (OptionValid keeps the offered ones). The
 * gOptions entry each changes is in updateTrackScreen; 3-11 are gOptions[32..40]. */
#define TRACK_OPT_TRACK 0
#define TRACK_OPT_GHOSTS 1              /* gOptions[25]: which ghosts (gGhostSelect) */
#define TRACK_OPT_LAPS 2                /* gOptions[21], 1-8 */
#define TRACK_OPT_STUNT_MODE 3          /* gOptions[32]: 0 time, 1 points, 2 death (MAT3 kTimeStuntMode ...) */
#define TRACK_OPT_STUNT_POINTS 5        /* offered in stunt mode 1 */
#define TRACK_OPT_STUNT_TIME 6          /* offered in stunt mode 0 */
#define TRACK_OPT_STUNT_DEATH 7         /* offered in stunt mode 2 */
#define TRACK_OPT_BATTLE_MODE 8         /* gOptions[37]: 0 points, 1 time (MAT3 kPointBattleMode, kTimeBattleMode) */
#define TRACK_OPT_BATTLE_POINTS 9       /* offered in battle mode 0 */
#define TRACK_OPT_BATTLE_TIME 10        /* offered in battle mode 1 */
#define TRACK_OPT_BATTLE_11 11          /* offered in battle mode 0 */
#define TRACK_OPT_BACKWARDS 12          /* gOptions[27]: the race tracks only */
#define TRACK_OPT_MIRROR 13             /* gOptions[28] */
#define TRACK_OPT_FOG 14                /* gOptions[29], 0-3 */
#define TRACK_OPT_WIND 15               /* gOptions[30], 0-4 */
#define TRACK_OPT_DRONES 17             /* gOptions[22] */
#define TRACK_OPT_DIFFICULTY 18         /* gOptions[24], 0-5 */
#define TRACK_OPT_CATCHUP 19            /* gOptions[26], 0-2 */
#define TRACK_OPT_DEATH_MODE 20         /* gOptions[31] */
#define TRACK_OPTS 21

#define RACE_OPTIONS_FIRST 21           /* gOptions[21..40]: the race options, kept per profile */
#define RACE_OPTIONS_END 41
#define MIRROR_SECRET 2                 /* gOptions[28]: the mirror option's hidden third value */
#define SECRET_BUTTONS 0x3C000          /* held with the change, they give MIRROR_SECRET (game button bits 14-17) */

/* The tracks (gTrackNum): the race tracks, the battle arenas, the stunt arenas and the obstacle course. */
#define TRACKS 19
#define TRACK_RACE_FIRST 0
#define TRACK_RACE_LAST 5
#define TRACK_BATTLE_FIRST 6
#define TRACK_BATTLE_LAST 13
#define TRACK_STUNT_FIRST 14
#define TRACK_STUNT_LAST 17
#define TRACK_OBSTACLE 18
#define TRACK_NEEDS_EXPANSION 5         /* this race track can't be raced without an Expansion Pak */
#define TRACK_MAP_SELECTED 19           /* trackMap[]: the highlighted race track's map */
#define MAP_LOD_SELECTED 6              /* mapObj[], saved_map_lods[] ...: the selected map's copy */

/* trackscr_buttonInfo[]: a label button per option, its value button TRACK_VALUE_BUTTON on, then these. */
#define TRACK_VALUE_BUTTON(opt) ((opt) + TRACK_OPTS)
#define TRACK_BUTTON_TITLE 42
#define TRACK_BUTTON_CURSOR 43
#define TRACK_BUTTON_A_ICON 44
#define TRACK_BUTTON_A_ARROW 45
#define TRACK_BUTTON_B_ICON 46
#define TRACK_BUTTON_B_ARROW 47
#define TRACK_BUTTONS 48

#define SND_BACK 38                     /* the menu sounds */
#define SND_MENU_DOWN 39
#define SND_MENU_UP 40
#define BUTTON_DEFAULT_OPTION 0x80      /* with trackscr_gActiveZ, would put an option back to its default */
#define WAVE_PATHS 158                  /* a race track's path data: this + the track */
#define CIRCUIT_DEATH_MODE_BYTE 1785    /* the profile's save data: the circuit's death mode */

extern Player gPlayers[];        /* gPlayers */

VIEW(s8[], gOptions);
#define gOptions gOptions_view
/* PC: the Drones option (gOptions[22], item 17) goes up to the cars a race may hold less one ([game] cars,
 * docs/port/MORECARS.md 1.3, 10.1): 5 with the setting at original, as on the N64. Its reset value and its default
 * (GetDefaultOption, GetDefaultOption) stay 5, so a player who never touches it races 6 cars. */
#define PC_DRONES_MAX (pc_cars_setting() - 1)

VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view
VIEW(u32, gActiveLevels);
#define gActiveLevels gActiveLevels_view
VIEW(u32, gActiveAutoEdges);
#define gActiveAutoEdges gActiveAutoEdges_view

VIEW(CircuitTrack[], gCircuitTracks);
#define gCircuitTracks gCircuitTracks_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(tMenuButton[48], trackscr_buttonInfo);
#define trackscr_buttonInfo trackscr_buttonInfo_view

VIEW(Color, trackscr_init_button_color);
#define trackscr_init_button_color trackscr_init_button_color_view
VIEW(Color, trackscr_update_button_color);
#define trackscr_update_button_color trackscr_update_button_color_view

VIEW(MultiBlit[], trackscr_mblit);
#define trackscr_mblit trackscr_mblit_view

VIEW(s16[21], trackscr_gOpts);
#define trackscr_gOpts trackscr_gOpts_view
VIEW(TrackSlot[19], gTracks);
#define gTracks gTracks_view

VIEW(f32[3], trackscr_InitScreen_fogPos);
#define trackscr_InitScreen_fogPos trackscr_InitScreen_fogPos_view

/* game functions */
extern void *memcpy(void *,const void *,u32);
extern f32 fabsf(f32);
#pragma intrinsic (fabsf)

extern void audio_bus_mix(Profile **,u8,s8);      /* SavePlayerOption */
extern s8 GetPlayerOption(Profile **,u8);

extern void particle_lifetime_set(void);          /* createBG */
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);

extern Texture *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);
extern void ZOID_SetTexture(s16,intptr_t,s32);

typedef union { Gwords words; long long int force_structure_alignment; } Gfx;
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define gSPTexture(pkt, s, t, level, tile, on) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(0xD7, 24, 8) | _SHIFTL(0, 16, 8) | _SHIFTL((level), 11, 3) | _SHIFTL((tile), 8, 3) | _SHIFTL((on), 1, 7)); \
    _g->words.w1 = (_SHIFTL((s), 16, 16) | _SHIFTL((t), 0, 16)); \
}
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, 0xE7)
#define gSPEndDisplayList(pkt) gDPNoParam(pkt, 0xDF)
#define gSPSetOtherMode(pkt, cmd, sft, len, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | _SHIFTL(32 - (sft) - (len), 8, 8) | _SHIFTL((len) - 1, 0, 8)); \
    _g->words.w1 = (u32)(data); \
}
#define gDPSetRenderMode(pkt, c0, c1) gSPSetOtherMode(pkt, 0xE2, 3, 29, (c0) | (c1))
#define gDPSetCombine(pkt, muxs0, muxs1) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(0xFC, 24, 8) | _SHIFTL(muxs0, 0, 24); \
    _g->words.w1 = (u32)(muxs1); \
}
#define gSPVertex(pkt, v, n, v0) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(0x01, 24, 8) | _SHIFTL((n), 12, 8) | _SHIFTL((v0) + (n), 1, 7); \
    _g->words.w1 = (uintptr_t)(v); \
}
#define __gsSP1Triangle_w1(v0, v1, v2) (_SHIFTL((v0) * 2, 16, 8) | _SHIFTL((v1) * 2, 8, 8) | _SHIFTL((v2) * 2, 0, 8))
#define gSP2Triangles(pkt, v00, v01, v02, flag0, v10, v11, v12, flag1) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(0x06, 24, 8) | __gsSP1Triangle_w1(v00, v01, v02)); \
    _g->words.w1 = __gsSP1Triangle_w1(v10, v11, v12); \
}

VIEW(s8[19], trackReady);
#define trackReady trackReady_view

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
struct Blit {const char *Name;void *Image;void *Info;u16 TexIndex;s16 X,Y;u16 Z;s16 Width,Height;u8 Alpha,Flip;s8 Hide,Init;
    s16 Top,Bot,Left,Right,color;u16 reserved;void *AnimFunc;u32 AnimID;intptr_t AnimDTA;u16 resource;};

VIEW(u8[8], trackscr_fogColor);
#define trackscr_fogColor trackscr_fogColor_view

/* PC: the fog blit's palette n64FogPal is a Palette (gfx's draw reads its colour table palBase, at +0x14, as a u32
 * window address), which must stay in the arena at its N64 address. palBase holds the colour pair trackscr_fogColor's
 * address, a window address (FOG_TABLE). As a host pointer it had storage of its own, and the draw read a table of 0
 * (docs/port/BRINGUP.md 7). */
typedef struct FogPalette { u8 head[0x14]; u32 palBase; } FogPalette;
#define FOG_TABLE(p) PC_N64W(p)

/* The fog layer's texture tile (MAT3 BlowFog's static fogTileSize, anon_uls_ult_8). */
typedef struct TileSize {u16 uls,ult,lrs,lrt;} TileSize;
VIEW(volatile s16, gRenderInProgress);
#define gRenderInProgress gRenderInProgress_view

extern void RemoveBlit(Blit *);   /* RemoveBlit (ZOID_RemovePoly on N64: blits) */

#define MAPLOD_PTR(x) (uintptr_t) x
/* An object definition (MAT3 tModelDef, 88 bytes). */
typedef struct tModelDef {char name[16];f32 radius;s16 unused,lodCount;MapLod lod[4];} RomObjDef;

VIEW(TrackMapEntry[20], trackMap);
#define trackMap trackMap_view

VIEW(MapLod[7], saved_map_lods);
#define saved_map_lods saved_map_lods_view
VIEW(MapLod[7], mapObj);
#define mapObj mapObj_view
VIEW(void *[7], map_display_list_blocks);
#define map_display_list_blocks map_display_list_blocks_view
VIEW(void *[7], map_vertex_blocks);
#define map_vertex_blocks map_vertex_blocks_view

#define N64A0(x) PC_N64A(x)
/* PC: the sprite's host layout (particle_knot's Sprite), as every view has it (docs/port/POINTERS.md 9). */
typedef struct ImageMeta {void *pal,*tex;u16 model;s16 x,y;u16 mode;s16 w,h;u8 color;u8 flags;s8 hidden;u8 pad17;s16 t0,s0,t1,s1;} ImageMeta;
typedef struct DisplayCommand {u32 w0,w1;} DisplayCommand;
VIEW(ImageMeta[], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

VIEW(Color, loadTrackScreen_backColor);
#define loadTrackScreen_backColor loadTrackScreen_backColor_view

/* PC: track-map display lists in host Gfx, 16 bytes a command: 5 per map section plus 7. */
#define MAP_DL_SECTION_BYTES 80
#define MAP_DL_FIXED_BYTES 112
VIEW(f32[12], trackMapMat);
#define trackMapMat trackMapMat_view
VIEW(f32[3], buildTrackMap_mapPos);
#define buildTrackMap_mapPos buildTrackMap_mapPos_view
extern Blit *NewBlit(char *,s16,s16,s16);
#define DISPLAY_WORDS(p,a,b) do {DisplayCommand *command=(p)++; command->w0=(a);command->w1=(b);} while(0)
#define DISPLAY_TEXTURE(p,on) DISPLAY_WORDS(p,0xD7000000u|((on)<<1),0xFFFFFFFFu)
#define DISPLAY_VERTEX(p,v,n,first) DISPLAY_WORDS(p,0x01000000u|((n)<<12)|(((first)+(n))<<1),(u32)(v))
#define DISPLAY_TRIANGLES(p,a,b,c,d,e,f) DISPLAY_WORDS(p,0x06000000u|((a)*2<<16)|((b)*2<<8)|((c)*2),((d)*2<<16)|((e)*2<<8)|((f)*2))

typedef struct ResPath {u16 numPts;s16 (*pts)[3];} ResPath;
typedef struct Checkpoints {s16 startTime,loopPt,finishPt,startPt,numPoints;} Checkpoints;
VIEW(s16[3], gPathMax);
#define gPathMax gPathMax_view
VIEW(s16[3], gPathMin);
#define gPathMin gPathMin_view
VIEW(ResPath, gResPath);
#define gResPath gResPath_view
VIEW(Checkpoints, gCheckpoints);
#define gCheckpoints gCheckpoints_view

VIEW(Color, race_map_vertex_color);
#define race_map_vertex_color race_map_vertex_color_view
VIEW(f32[3], buildRaceTrackMaps_mapPos);
#define buildRaceTrackMaps_mapPos buildRaceTrackMaps_mapPos_view
VIEW(f32[3], buildRaceTrackMaps_selMapPos);
#define buildRaceTrackMaps_selMapPos buildRaceTrackMaps_selMapPos_view
VIEW(f32[3], buildStuntTrackMaps_mapPos);
#define buildStuntTrackMaps_mapPos buildStuntTrackMaps_mapPos_view
VIEW(f32[3], buildBattleTrackMaps_mapPos);
#define buildBattleTrackMaps_mapPos buildBattleTrackMaps_mapPos_view

extern void *audio_dma_sync(s32,s32);  /* AllocateMemoryHeap */

extern f32 sqrtf(f32);
#pragma intrinsic (sqrtf)
#define GFX(p,a,b) do { Gfx *_g = (p)++; _g->words.w0 = (a); _g->words.w1 = (b); } while (0)

/* The screen's frame. After a race with a screen to skip to it passes straight on to the car screen. The first frame
 * sets it up; the carousel moves (showTrackSelect). A profile's pak pulled sends everyone back to the player screen,
 * a soft reset to the title.
 * - Start or A (not on TRACK_NEEDS_EXPANSION without an Expansion Pak) goes on to the car screen, saving the race
 *   options in every player's profile; B goes back to the circuit or the mode screen.
 * - Left or right (auto-repeating; reversed with the mirrored-controls cheat) changes the option: the track (to the
 *   next valid one, loading the profile's options for it), the ghosts, laps, the stunt and battle settings,
 *   direction and mirror (which rebuild the map), fog, wind, drones, difficulty, catch-up and death mode. Not in a
 *   circuit (but its first race's death mode), nor the laps of a ghost race. The options apply (SetOptions) and are
 *   saved in the first player's profile.
 * - Up and down move through the offered options, the list keeping the option within its first rows.
 * A track change saves the track in each profile, by race type. Then the maps, the fog and the camera. */
void updateTrackScreen(void)
{
    s32 i;
    s32 j;
    s32 add;
    if (gSkipToGameState != SKIP_NONE) {
        if (gSkipToGameState != gGameState) { setGameState(GS_CAR); return; }
        gSkipToGameState = SKIP_NONE;
    }
    if (!trackscr_inited) trackscr_InitScreen();
    showTrackSelect();
    for (i = 0; i < gNumPlayers; i++) {
        if ((*gPlayers[i].save)->fileh && PS_PakChanged((*(*gPlayers[i].save)->fileh)->pak)) {
            trackscr_UninitScreen(); setGameState(GS_PLAYER); return;
        }
    }
    if (gSoftReset) { trackscr_UninitScreen(); setGameState(GS_TITLE); return; }
    if ((gActiveEdges & BUTTON_ACCEPT) && (expansion_pak_present || gTrackNum != TRACK_NEEDS_EXPANSION)) {
        PlayStartABSound(gActiveEdges); setGameState(GS_CAR); trackscr_UninitScreen();
        for (j = 0; j < gNumPlayers; j++)
            for (i = RACE_OPTIONS_FIRST; i < RACE_OPTIONS_END; i++) audio_bus_mix(gPlayers[j].save, i, gOptions[i]);
        return;
    }
    if (gActiveEdges & BUTTON_B) {
        AudPlayEffect(SND_BACK, 0, 1, 0); trackscr_UninitScreen();
        if (gRace == kCircuit) setGameState(GS_CIRCUIT);
        else setGameState(GS_MODE);
        return;
    }
    if (gActiveLevels & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_DEFAULT_OPTION)) {
        /* add: -1 left, 1 right, 0 the option's default */
        add = (gActiveAutoEdges & BUTTON_LEFT) ? -1 : (gActiveAutoEdges & BUTTON_RIGHT) ? 1 : 0;
        /* Original game: nothing sets trackscr_gActiveZ but _InitScreen's 0 (MAT3 neither), so a frame without a
         * left or right edge changes nothing, and the defaults (`else gOptions[n] = ...`) are unreachable. */
        if ((gRace == kCircuit && (gCircuitTrack > 0 || trackscr_gOpts[trackscr_gActiveOption] != TRACK_OPT_DEATH_MODE)) || (add == 0 && !trackscr_gActiveZ) || (gRace == kGhost && trackscr_gOpts[trackscr_gActiveOption] == TRACK_OPT_LAPS)) goto controls_done;
        PlayLRSound(gActiveAutoEdges);
        if (gMirrorControls > 1) add = -add;
        switch (trackscr_gOpts[trackscr_gActiveOption]) {
        case TRACK_OPT_TRACK:
            if (add) {
                gTrack += add;
                if (gTrack < 0) gTrack = gNumTracks - 1;
                else if (gTrack >= gNumTracks) gTrack = 0;
                do {
                    gTrackNum += add;
                    if (gTrackNum < 0) gTrackNum = TRACKS - 1;
                    else if (gTrackNum >= TRACKS) gTrackNum = 0;
                } while (!TrackValid(gTrackNum));
                for (i = RACE_OPTIONS_FIRST; i < RACE_OPTIONS_END; i++)
                    gOptions[i] = GetPlayerOption(gPlayers[0].save, i);
                if (!expansion_pak_present) trackscr_buttonsJustInited = 1;
            }
            break;
        case TRACK_OPT_GHOSTS:
            if (add) {
                gOptions[25] += add;
                if (gOptions[25] < 0) gOptions[25] = 1;
                else if (gOptions[25] > 1) gOptions[25] = 0;
            } else gOptions[25] = 0;
            break;
        case TRACK_OPT_LAPS:
            if (add) {
                gOptions[21] += add;
                if (gOptions[21] < 1) gOptions[21] = 8;
                else if (gOptions[21] > 8) gOptions[21] = 1;
            } else gOptions[21] = 3;
            break;
        case TRACK_OPT_STUNT_MODE:
            if (add) {
                gOptions[32] += add;
                if (gOptions[32] < 0) gOptions[32] = 2;
                else if (gOptions[32] > 2) gOptions[32] = 0;
            } else gOptions[32] = 0;
            break;
        case 4:     /* gOptions[33], on or off */
            if (add) {
                if (!gOptions[33]) gOptions[33] = 1;
                else gOptions[33] = 0;
            } else gOptions[33] = 0;
            break;
        case TRACK_OPT_STUNT_POINTS:
            if (add) {
                gOptions[34] += add;
                if (gOptions[34] <= 0) gOptions[34] = 100;
                else if (gOptions[34] > 100) gOptions[34] = 1;
            } else gOptions[34] = 69;
            break;
        case TRACK_OPT_STUNT_TIME:
            if (add) {
                gOptions[35] += add;
                if (gOptions[35] <= 0) gOptions[35] = 20;
                else if (gOptions[35] > 20) gOptions[35] = 1;
            } else gOptions[35] = 5;
            break;
        case TRACK_OPT_STUNT_DEATH:
            if (add) {
                gOptions[36] += add;
                if (gOptions[36] <= 0) gOptions[36] = 50;
                else if (gOptions[36] > 50) gOptions[36] = 1;
            } else gOptions[36] = 30;
            break;
        case TRACK_OPT_BATTLE_MODE:
            if (add) {
                if (!gOptions[37]) gOptions[37] = 1;
                else gOptions[37] = 0;
            } else gOptions[37] = 0;
            break;
        case TRACK_OPT_BATTLE_POINTS:
            if (add) {
                gOptions[38] += 5*add;
                if (gOptions[38] < 5) gOptions[38] = 50;
                else if (gOptions[38] > 50) gOptions[38] = 5;
            } else gOptions[38] = 10;
            break;
        case TRACK_OPT_BATTLE_TIME:
            if (add) {
                gOptions[39] += add;
                if (gOptions[39] <= 0) gOptions[39] = 20;
                else if (gOptions[39] > 20) gOptions[39] = 1;
            } else gOptions[39] = 8;
            break;
        case TRACK_OPT_BATTLE_11:
            if (add) {
                gOptions[40] ^= 1;
            } else gOptions[40] = 0;
            break;
        case TRACK_OPT_BACKWARDS:
            if (add) {
                gOptions[27] ^= 1;
            } else gOptions[27] = 0;
            clearTrackMap(); buildTrackMap(); pathPos = 1.0f - pathPos;
            break;
        case TRACK_OPT_MIRROR:
            if (add) {
                if ((gPlayers[0].levels & SECRET_BUTTONS) == SECRET_BUTTONS) gOptions[28] = MIRROR_SECRET;
                else if (!gOptions[28]) gOptions[28] = 1;
                else gOptions[28] = 0;
            } else gOptions[28] = 0;
            clearTrackMap(); buildTrackMap();
            break;
        case TRACK_OPT_FOG:
            if (add) {
                gOptions[29] += add;
                if (gOptions[29] < 0) gOptions[29] = 3;
                else if (gOptions[29] > 3) gOptions[29] = 0;
            } else gOptions[29] = 0;
            break;
        case TRACK_OPT_WIND:
            if (add) {
                gOptions[30] += add;
                if (gOptions[30] < 0) gOptions[30] = 4;
                else if (gOptions[30] > 4) gOptions[30] = 0;
            } else gOptions[30] = 0;
            break;
        case 16:    /* gOptions[23], 0-3 */
            if (add) {
                gOptions[23] += add;
                if (gOptions[23] < 0) gOptions[23] = 3;
                else if (gOptions[23] > 3) gOptions[23] = 0;
            } else gOptions[23] = 1;
            break;
        case TRACK_OPT_DRONES:
            if (add) {
                gOptions[22] += add;
                if (gOptions[22] > PC_DRONES_MAX) gOptions[22] = 0;
                else if (gOptions[22] < 0) gOptions[22] = PC_DRONES_MAX;
            } else gOptions[22] = 5;
            break;
        case TRACK_OPT_DIFFICULTY:
            if (add) {
                gOptions[24] += add;
                if (gOptions[24] < 0) gOptions[24] = 5;
                else if (gOptions[24] > 5) gOptions[24] = 0;
            } else gOptions[24] = 2;
            break;
        case TRACK_OPT_CATCHUP:
            if (add) {
                gOptions[26] += add;
                if (gOptions[26] < 0) gOptions[26] = 2;
                else if (gOptions[26] > 2) gOptions[26] = 0;
            } else gOptions[26] = 2;
            break;
        case TRACK_OPT_DEATH_MODE:
            if (add) {
                gOptions[31] ^= 1;
            } else gOptions[31] = 0;
            if (gRace == kCircuit) (*(*gPlayers[gCircuitPlayer].save)->stats)[CIRCUIT_DEATH_MODE_BYTE] = gOptions[31];
            break;
        }
        SetOptions();
        for (i = RACE_OPTIONS_FIRST; i < RACE_OPTIONS_END; i++) audio_bus_mix(gPlayers[0].save, i, gOptions[i]);
    } else if (gActiveAutoEdges & BUTTON_UP) {
        if (expansion_pak_present || gTrackNum != TRACK_NEEDS_EXPANSION) {
            AudPlayEffect(SND_MENU_UP, 0, 1, 0);
            if (--trackscr_gActiveOption < 0) trackscr_gActiveOption = trackscr_gNumOptions - 1;
        }
    } else if (gActiveAutoEdges & BUTTON_DOWN) {
        if (expansion_pak_present || gTrackNum != TRACK_NEEDS_EXPANSION) {
            AudPlayEffect(SND_MENU_DOWN, 0, 1, 0);
            trackscr_gActiveOption++;
            if (trackscr_gActiveOption > trackscr_gNumOptions - 1) trackscr_gActiveOption = 0;
        }
    }
    if (trackscr_gFirstOption > trackscr_gActiveOption - 1) trackscr_gFirstOption = trackscr_gActiveOption - 1;
    if (trackscr_gFirstOption < trackscr_gActiveOption - 2) trackscr_gFirstOption = trackscr_gActiveOption - 2;
    if (trackscr_gFirstOption > trackscr_gNumOptions - 4) trackscr_gFirstOption = trackscr_gNumOptions - 4;
    if (trackscr_gFirstOption < 0) trackscr_gFirstOption = 0;
controls_done:
    trackscr_gActiveOptionEnabled = gRace != kCircuit || (gCircuitTrack == 0 && trackscr_gOpts[trackscr_gActiveOption] == TRACK_OPT_DEATH_MODE);
    if (gTrackNum != oldTrack) {
        desiredTrackMap = gTrackNum;
        oldTrack = gTrackNum;
        for (i = 0; i < gNumPlayers; i++) {
            switch (gRace) {
            case kOneRace: case kPractice: case kGhost:
                if (gTrackNum >= TRACK_RACE_FIRST && gTrackNum <= TRACK_RACE_LAST) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum + 0);
                break;
            case kStunt:
                if (gTrackNum >= TRACK_STUNT_FIRST && gTrackNum <= TRACK_STUNT_LAST) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - TRACK_STUNT_FIRST);
                break;
            case kBattle:
                if (gTrackNum >= TRACK_BATTLE_FIRST && gTrackNum <= TRACK_BATTLE_LAST) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - TRACK_BATTLE_FIRST);
                break;
            }
        }
    }
    trackscr_animTrackMap();
    animTrackMaps();
    SetFogIntensity(0);
    setCamera();
}

/* Set the screen up: the first option, the offered options, the track maps; the track: a circuit's race, else the
 * first player's last track of this race type (or the type's first if it isn't valid), saved in every profile; the
 * race options from the first player's profile; the buttons and the text blit; the carousel with the track
 * highlighted, the tracks 100 apart around it; the fog and the camera. */
void trackscr_InitScreen(void)
{
    s32 i;
    s32 j;
    f32 pos;
    trackscr_gActiveOption = 0;
    trackscr_gFirstOption = 0;
    trackscr_gActiveZ = 0;
    currentTrackMap = -1;
    trackscr_gNumOptions = 0; for (i = 0; i < TRACK_OPTS; i++) if (OptionValid(i)) trackscr_gOpts[trackscr_gNumOptions++] = i;
    buildTrackMaps();
    if (gRace == kCircuit) oldTrack = gTrackNum = gCircuitTracks[gCircuitTrack].trackNum;
    else {
        if (gRace == kStunt) oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + TRACK_STUNT_FIRST;
        else if (gRace == kObstacle) oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + TRACK_OBSTACLE;
        else if (gRace == kBattle) oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + TRACK_BATTLE_FIRST;
        else oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + 0;
        if (!TrackValid(gTrackNum)) {
            if (gRace == kStunt) oldTrack = gTrackNum = TRACK_STUNT_FIRST;
            else if (gRace == kObstacle) oldTrack = gTrackNum = TRACK_OBSTACLE;
            else if (gRace == kBattle) oldTrack = gTrackNum = TRACK_BATTLE_FIRST;
            else oldTrack = gTrackNum = TRACK_RACE_FIRST;
        }
        for (i = 0; i < gNumPlayers; i++) {
            if (gRace == kStunt) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - TRACK_STUNT_FIRST);
            else if (gRace == kObstacle) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - TRACK_OBSTACLE);
            else if (gRace == kBattle) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - TRACK_BATTLE_FIRST);
            else SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum);
        }
        for (i = RACE_OPTIONS_FIRST; i < RACE_OPTIONS_END; i++)
            gOptions[i] = GetPlayerOption(gPlayers[0].save, i);
    }
    SetOptions();
    particle_lifetime_set();
    trackscr_initButtons();
    trackscr_blit = NewMultiBlit(0, 0, trackscr_mblit, 17);
    i = 0;
    for (j = 0; j < TRACKS; j++) {
        if (TrackValid(j)) {
            if (j == gTrackNum) { gTrack = i; break; }
            i++;
        }
    }
    gNumTracks = TRACKS;
    i = 0;
    pos = 0.0f;
    for (j = 0; j < TRACKS; j++) {
        if (TrackValid(j)) {
            gTracks[i].gTrackNum = j;
            gTracks[i].slot = i - gTrack;
            if (gTracks[i].slot < 0) gTracks[i].slot += gNumTracks;
            if (gTrack == i) gTracks[i].targetPos = 0.0f;
            else if (gTrack >= gNumTracks/2) gTracks[i].targetPos = pos - (gNumTracks - gTrack)*100;
            else gTracks[i].targetPos = pos + gTrack*100;
            gTracks[i].pos[0] = gTracks[i].targetPos;
            gTracks[i].pos[1] = -5.0f;
            gTracks[i].pos[2] = 0.0f;
            pos -= 100.0f;
            if (pos < -(gNumTracks*100/2)) pos += gNumTracks*100;
            i++;
        }
    }
    trackscr_InitScreen_fogPos[0] = 0.0f;
    trackscr_InitScreen_fogPos[1] = yFog[gTracks[gTrack].gTrackNum] - 5.0f;
    trackscr_InitScreen_fogPos[2] = 0.0f;
    SetFogIntensity(1);
    BlowFog();
    setCamera();
    ClearRushControls();
    trackscr_inited = 1;
    currentTrackMap = -1;
    desiredTrackMap = gTrackNum;
    mapTransition = 0;
}

/* Take the screen down: the buttons, the text blit, the background, the track maps and their models. */
void trackscr_UninitScreen(void)
{
    trackscr_uninitButtons();
    if (trackscr_blit) { RemoveBlit(trackscr_blit); trackscr_blit = 0; }
    removeBG();
    clearTrackMap();
    clearTrackMaps();
    ZOID_Reset();
    trackscr_inited = 0;
}

typedef struct UVMatrix { f32 uvs[3][3]; f32 pos[3]; } UVMatrix;   /* tUVMatrix */
/* The carousel's frame (and the buttons'): the offered options listed again; a spin angle that turns a degree a 30 Hz
 * frame (the other way for a backwards or mirrored race); each track sliding a tenth of the way to its place a
 * frame, the places moving by 100 as the highlight moves, wrapping at the ends; and the fog's height easing to the
 * highlighted track's (yFog). MAT3 draws a spinning model with the angle; the N64 computes it and draws nothing. */
void showTrackSelect(void)
{
    static f32 angle = 0.0f;
    static f32 fogPos[3];
    UVMatrix fogMat;
    f32 uv[3][3];
    f32 speed;
    f32 frames;
    s32 i;
    s32 slot;
    f32 fogY, fogDelta;
    trackscr_updateButtons();
    for (trackscr_gNumOptions = 0, i = 0; i < TRACK_OPTS; i++) if (OptionValid(i)) trackscr_gOpts[trackscr_gNumOptions++] = i;
    frames = gFrameSeconds * 30.0f;
    if ((gBackwards && !gMirrorWaves) || (!gBackwards && gMirrorWaves)) {
        angle -= 0.01745329238474369f * frames;
        if (angle < 0.0f) angle += 6.2831854820251465f;
    } else {
        angle += 0.01745329238474369f * frames;
        if (angle >= 6.2831854820251465f) angle -= 6.2831854820251465f;
    }
    speed = frames * ((gTracks[gTrack].targetPos - gTracks[gTrack].pos[0]) / 10.0f);
    for (i = 0; i < gNumTracks; i++) {
        slot = i - gTrack;
        if (slot < 0) slot += gNumTracks;
        if (slot != gTracks[i].slot) {
            if (gTracks[i].slot == (slot-1 < 0 ? gNumTracks-1 : slot-1)) gTracks[i].targetPos -= 100.0f;
            else gTracks[i].targetPos += 100.0f;
            gTracks[i].slot = slot;
        }
        if (speed != 0.0f) {
            gTracks[i].pos[0] += speed;
            if (gTracks[i].pos[0] < -(gNumTracks*100/2)) {
                gTracks[i].pos[0] += gNumTracks*100;
                gTracks[i].targetPos += gNumTracks*100;
            } else if (gTracks[i].pos[0] > gNumTracks*100/2) {
                gTracks[i].pos[0] -= gNumTracks*100;
                gTracks[i].targetPos -= gNumTracks*100;
            }
        }
    }
    fogPos[1] += ((yFog[gTracks[gTrack].gTrackNum] - 5.0f - fogPos[1]) / 10.0f) * frames;
    CopyUV(unitmatrix, uv);
    YawUV(angle, uv);
    ScaleUVXYZ(uv, uv, 2.0f, 1.0f, 2.5f);
}

/* Can track be raced in this race type? Its data must be ready (trackReady) and it unlocked; then the stunt arenas
 * for a stunt race, the obstacle course for an obstacle race, the battle arenas for a battle, else the race tracks.
 * 1 yes, 0 no. */
s32 TrackValid(s32 track)
{
    if (!trackReady[track]) return 0;
    if (!IsTrackUnlocked(track)) return 0;
    if (gRace == kStunt) return track >= TRACK_STUNT_FIRST && track <= TRACK_STUNT_LAST;
    if (gRace == kObstacle) return track >= TRACK_OBSTACLE && track <= TRACK_OBSTACLE;
    if (gRace == kBattle) return track >= TRACK_BATTLE_FIRST && track <= TRACK_BATTLE_LAST;
    return track >= TRACK_RACE_FIRST && track <= TRACK_RACE_LAST;
}

/* Is race option opt (TRACK_OPT_*) offered? By race type (trackscr_validOption), and: drones in a single race only
 * for one or two players; the stunt settings by the stunt mode, the battle ones by the battle mode; catch-up not for
 * one player; direction only on the race tracks; nothing but the track on TRACK_NEEDS_EXPANSION without an
 * Expansion Pak; the ghosts only with ghosts on. 1 yes, 0 no. */
s32 OptionValid(s32 opt)
{
    s32 valid;
    valid = trackscr_validOption[gRace][opt];
    if (gRace == kOneRace && opt == TRACK_OPT_DRONES) valid = gNumPlayers < 3;
    if (gRace == kStunt) {
        if (opt == TRACK_OPT_STUNT_POINTS) valid = gOptions[32] == 1;
        if (opt == TRACK_OPT_STUNT_TIME) valid = gOptions[32] == 0;
        if (opt == TRACK_OPT_STUNT_DEATH) valid = gOptions[32] == 2;
    }
    if (gRace == kBattle) {
        if (opt == TRACK_OPT_BATTLE_POINTS) valid = gOptions[37] == 0;
        if (opt == TRACK_OPT_BATTLE_11) valid = gOptions[37] == 0;
        if (opt == TRACK_OPT_BATTLE_TIME) valid = gOptions[37] == 1;
    }
    if (opt == TRACK_OPT_CATCHUP && gNumPlayers == 1) valid = 0;
    if (opt == TRACK_OPT_BACKWARDS) valid = gTrackNum >= TRACK_RACE_FIRST && gTrackNum <= TRACK_RACE_LAST;
    if (!expansion_pak_present && gTrackNum == TRACK_NEEDS_EXPANSION && opt != TRACK_OPT_TRACK) valid = 0;
    if (!ghosts_enabled && opt == TRACK_OPT_GHOSTS) valid = 0;
    return valid;
}

/* The fog layer over the carousel: its colour moves a tenth of the way to the highlighted track's fog colour a 30 Hz
 * frame (at once when initialize), its alpha follows the fog setting, and the fog blit (if there is one) is drawn
 * with that colour through the fog palette n64FogPal. */
void SetFogIntensity(s32 initialize)
{
    u8 color[3];
    if (gFogColor < 19)
        gFogColor = gTrackNum;
    color[0] = gFogColors[gFogColor].r;
    color[1] = gFogColors[gFogColor].g;
    color[2] = gFogColors[gFogColor].b;
    if (initialize) {
        trackscr_fogColor[0] = color[0];
        trackscr_fogColor[1] = color[1];
        trackscr_fogColor[2] = color[2];
    } else {
        PC_EACH30(trackscr_fogColor[0] += (color[0] - trackscr_fogColor[0]) / 10;)
        PC_EACH30(trackscr_fogColor[1] += (color[1] - trackscr_fogColor[1]) / 10;)
        PC_EACH30(trackscr_fogColor[2] += (color[2] - trackscr_fogColor[2]) / 10;)
    }
    trackscr_fogColor[3] = gFogIntensity * 192 / 3;
    if (trackFogPoly) {
        ((FogPalette *) n64FogPal)->palBase = FOG_TABLE(trackscr_fogColor);
        trackscr_fogColor[4] = 0;
        trackscr_fogColor[5] = 0;
        trackscr_fogColor[6] = 0;
        trackscr_fogColor[7] = 0;
        trackFogPoly->Z = 0x7E00;
        trackFogPoly->Alpha = trackscr_fogColor[3];
        trackFogPoly->Image = n64FogPal;
        UpdateBlit(trackFogPoly);
    }
}

/* Scroll the fog layer's texture sideways by the wind (5 a step of wind, 1 without), wrapping at 2048. The tile
 * isn't given to anything here (MAT3 sets the fog's tile size with it). */
void BlowFog(void)
{
    static TileSize fogTileSize;
    TileSize *tile = &fogTileSize;
    s32 s;
    s32 speed;
    speed = gWind ? gWind * 5 : 1;
    s = tile->uls + speed;
    if (s < 0) s += 2048;
    if (s >= 2048) s -= 2048;
    tile->uls = s;
    tile->ult = 0;
}

/* The screen's camera: camera 1 reset, the cameras updated. */
void setCamera(void)
{
    ZeroCamera(1);
    UpdateCameras();
}

/* The highlighted track's picture (a blit; MAT3 a polygon), and the fog layer over it where fog is offered. */
void loadTrackScreen(void)
{
    f32 polyVerts[4][3];
    u16 texIdx;
    Color backColor = loadTrackScreen_backColor;
    unloadTrackScreen();
    if (!((gTrackNum >= TRACK_RACE_FIRST && gTrackNum <= TRACK_RACE_LAST) ||
        (gTrackNum >= TRACK_BATTLE_FIRST && gTrackNum <= TRACK_BATTLE_LAST) ||
        (gTrackNum >= TRACK_STUNT_FIRST && gTrackNum <= TRACK_STUNT_LAST) ||
        (gTrackNum >= TRACK_OBSTACLE && gTrackNum <= TRACK_OBSTACLE))) return;
    trackScreenPoly = NewBlit(trackScreenNames[gTrackNum],176,32,0);
    trackScreenPoly->Z = 0x7F00;
    UpdateBlit(trackScreenPoly);
    gBlitInstanceList[trackScreenPoly->resource].flags |= 1;
    if (OptionValid(TRACK_OPT_FOG) == 1) {
        trackFogPoly = NewBlit(track_fog_texture_name,176,32,0);
        trackFogPoly->Z = 0x7E00;
        UpdateBlit(trackFogPoly);
    }
}

/* Remove the track picture and the fog layer. */
void unloadTrackScreen(void)
{
    Blit *b;
    if ((b = trackScreenPoly)) { RemoveBlit(b); trackScreenPoly = 0; }
    if ((b = trackFogPoly)) { RemoveBlit(b); trackFogPoly = 0; }
}

/* The 3D map of the highlighted track (desiredTrackMap): the picture, and for a race track its map copied into the
 * selected map's buffers (mirrored maps get the triangles again the other way round, so both faces show), for an
 * arena its model. */
void buildTrackMap(void)
{
    Gfx *gfx;
    Vtx_t *vtx;
    s32 i, vertex;
    u32 vtxSize, dlSize;
    s32 trackMBIdx;
    loadTrackScreen();
    if (desiredTrackMap >= TRACK_RACE_FIRST && desiredTrackMap <= TRACK_RACE_LAST) {
        if (trackMap[TRACK_MAP_SELECTED].handle != -1) {
            vtxSize = (kNumMapSects + 1) * 4 * sizeof(Vtx_t);
            memcpy(map_vertex_blocks[MAP_LOD_SELECTED], map_vertex_blocks[desiredTrackMap], vtxSize);
            dlSize = (kNumMapSects * 5 + 7) * sizeof(Gfx);
            memcpy(map_display_list_blocks[MAP_LOD_SELECTED], map_display_list_blocks[desiredTrackMap], dlSize);
            gfx = mapObj[MAP_LOD_SELECTED].displayList;   /* the selected map's display list */
            if ((gRace == kCircuit && gCircuitTracks[gCircuitTrack].mirror == 1)
                || (gRace != kCircuit && gOptions[28])) {
                gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 0, 0);
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, 0xC8112230, 0);
                gDPSetCombine(gfx++, 0xFFFFFF, 0xFFFE7C38);
                for (i = 0, vertex = 0; i < kNumMapSects; i++, vertex += 4) {
                    if (i + 1 == kNumMapSects) {
                        gSPVertex(gfx++, &((Vtx_t *)map_vertex_blocks[MAP_LOD_SELECTED])[vertex], 4, 0);
                        gSPVertex(gfx++, &((Vtx_t *)map_vertex_blocks[MAP_LOD_SELECTED])[0], 4, 4);
                    } else {
                        gSPVertex(gfx++, &((Vtx_t *)map_vertex_blocks[MAP_LOD_SELECTED])[vertex], 8, 0);
                    }
                    gSP2Triangles(gfx++, 1, 5, 0, 0, 4, 0, 5, 0);
                    gSP2Triangles(gfx++, 0, 4, 3, 0, 7, 3, 4, 0);
                    gSP2Triangles(gfx++, 3, 7, 2, 0, 6, 2, 7, 0);
                    gSP2Triangles(gfx++, 2, 6, 1, 0, 5, 1, 6, 0);
                }
                gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 0, 1);
                gSPEndDisplayList(gfx++);
            }
            ScaleUV(unitmatrix, trackMap[TRACK_MAP_SELECTED].matrix, 2.0f);
            WPitchUV(-0.785398185f, trackMap[TRACK_MAP_SELECTED].matrix);
            trackMap[TRACK_MAP_SELECTED].position[0] = buildTrackMap_mapPos[0];
            trackMap[TRACK_MAP_SELECTED].position[1] = buildTrackMap_mapPos[1];
            trackMap[TRACK_MAP_SELECTED].position[2] = buildTrackMap_mapPos[2];
        }
    } else if ((desiredTrackMap >= TRACK_BATTLE_FIRST && desiredTrackMap <= TRACK_BATTLE_LAST) || (desiredTrackMap >= TRACK_STUNT_FIRST && desiredTrackMap <= TRACK_STUNT_LAST)) {
        if (trackMapObj == -1) {
            trackMapObj = MBOX_NewObject(MBOX_FindObject_WORK(trackMapObjNames[desiredTrackMap], 0, gNumModels - 1, 1), (uintptr_t)trackMapMat, -1, 0);
            ScaleUV(unitmatrix, (f32 (*)[3])trackMapMat, 2.0f);
            WPitchUV(-0.785398185f, (f32 (*)[3])trackMapMat);
            trackMapMat[9] = buildTrackMap_mapPos[0];
            trackMapMat[10] = buildTrackMap_mapPos[1];
            trackMapMat[11] = buildTrackMap_mapPos[2];
        }
    }
    currentTrackMap = desiredTrackMap;
}

/* Remove the highlighted track's picture and arena model, and wait for the frame being drawn to finish with them. */
void clearTrackMap(void)
{
    unloadTrackScreen();
    if (trackMapObj != -1) { ZOID_RemoveObject((s16)trackMapObj); trackMapObj = -1; }
    while (gRenderInProgress) { pc_os_spin_yield("clearTrackMap"); }
}

/* The highlighted track's map, each frame: when the highlight moves the map shrinks away (4 a second) and the new
 * one is built and grows back; it spins once in 10 seconds, moving from the carousel's place to its own as it grows,
 * mirrored for a mirrored race. */
void trackscr_animTrackMap(void)
{
    static f32 cityPos[3] = { -100.0f, 30.0f, 200.0f };
    static f32 mapPos[3] = { 50.0f, -15.0f, 100.0f };
    static f32 rotAngle = 0.0f;
    rotAngle += gFrameSeconds * 6.2831854820251465f / 10.0f;
    if (rotAngle > 6.2831854820251465f) rotAngle -= 6.2831854820251465f;
    if (currentTrackMap != desiredTrackMap) {
        mapTransition -= gFrameSeconds * 4.0f;
        if (mapTransition < 0.0f) {
            mapTransition = 0.0f;
            clearTrackMap();
            buildTrackMap();
        }
    } else {
        mapTransition += gFrameSeconds * 4.0f;
        if (mapTransition > 1.0f) mapTransition = 1.0f;
    }
    trackMap[TRACK_MAP_SELECTED].position[0] = cityPos[0];
    trackMap[TRACK_MAP_SELECTED].position[1] = cityPos[1];
    trackMap[TRACK_MAP_SELECTED].position[2] = cityPos[2];
    trackMap[TRACK_MAP_SELECTED].position[0] += trackMap[currentTrackMap].offset_x;
    trackMap[TRACK_MAP_SELECTED].position[1] += trackMap[currentTrackMap].offset_y;
    trackMap[TRACK_MAP_SELECTED].position[0] *= 1.0f - mapTransition;
    trackMap[TRACK_MAP_SELECTED].position[1] *= 1.0f - mapTransition;
    trackMap[TRACK_MAP_SELECTED].position[2] *= 1.0f - mapTransition;
    trackMap[TRACK_MAP_SELECTED].position[0] += mapPos[0] * mapTransition;
    trackMap[TRACK_MAP_SELECTED].position[1] += mapPos[1] * mapTransition;
    trackMap[TRACK_MAP_SELECTED].position[2] += mapPos[2] * mapTransition;
    trackMapMat[9] = trackMap[TRACK_MAP_SELECTED].position[0];
    trackMapMat[10] = trackMap[TRACK_MAP_SELECTED].position[1];
    trackMapMat[11] = trackMap[TRACK_MAP_SELECTED].position[2];
    ScaleUV(unitmatrix, (f32 (*)[3])trackMapMat,
        mapTransition * trackMap[currentTrackMap].scale_to + trackMap[currentTrackMap].scale_from * (1.0f - mapTransition));
    ScaleUV(unitmatrix, trackMap[TRACK_MAP_SELECTED].matrix,
        mapTransition * trackMap[currentTrackMap].scale_to + trackMap[currentTrackMap].scale_from * (1.0f - mapTransition));
    if ((gRace == kCircuit && gCircuitTracks[gCircuitTrack].mirror == 1)
        || (gRace != kCircuit && gRace != kStunt && gRace != kBattle && gRace != kObstacle && gOptions[28])) {
        ScaleUVXYZ((f32 (*)[3])trackMapMat, (f32 (*)[3])trackMapMat, -1.0f, 1.0f, 1.0f);
        ScaleUVXYZ(trackMap[TRACK_MAP_SELECTED].matrix, trackMap[TRACK_MAP_SELECTED].matrix, -1.0f, 1.0f, 1.0f);
    }
    WYawUV(rotAngle * mapTransition + trackMap[currentTrackMap].angle * (1.0f - mapTransition), (f32 (*)[3])trackMapMat);
    WPitchUV(-0.5235987901687622f * mapTransition + (1.0f - mapTransition) * -1.5707963705062866f, (f32 (*)[3])trackMapMat);
    WYawUV(rotAngle * mapTransition + trackMap[currentTrackMap].angle * (1.0f - mapTransition), trackMap[TRACK_MAP_SELECTED].matrix);
    WPitchUV(-0.5235987901687622f * mapTransition + (1.0f - mapTransition) * -1.5707963705062866f, trackMap[TRACK_MAP_SELECTED].matrix);
}

/* Build the maps of the race type's tracks. */
void buildTrackMaps(void)
{
    if (gRace == kStunt) buildStuntTrackMaps();
    else if (gRace == kBattle) buildBattleTrackMaps();
    else if (gRace == kObstacle) buildObstacleTrackMaps();
    else buildRaceTrackMaps();
    trackMapsBuilt = 1;
    pathPos = 0.0f;
}

/* The race tracks' maps, drawn from their paths: for each unlocked race track, load its path, and make a ribbon of
 * kNumMapSects sections along the path (each section's corners 100 to either side of the path at its ends, the top
 * edge the path's height and the bottom the track's lowest point), scaled to fit, as an N64 display list of four
 * vertices a section and two triangles each side (MAT3 builds PS2 packets); it replaces the map model's first LOD
 * (the old one kept in saved_map_lods). Then the buffers of the selected map's copy (MAP_LOD_SELECTED). */
void buildRaceTrackMaps(void)
{
    s32 saveTrackNum;
    s32 pointIdx;
    s32 vertIdx;
    s32 prevPt;
    s32 currStartPt;
    s32 currEndPt;
    s32 nextPt;
    f32 x0, y0, x1, y1, x2, y2, x3, y3;
    f32 dx0, dy0, len0, dx1, dy1, len1, dx2, dy2, len2;
    f32 xoffset, yoffset, zoffset;
    s32 trackMBIdx;
    RomObjDef *trackRomObjDef;
    Gfx *dl;
    Vtx_t *vtx;
    s32 vtxSize, dlSize;
    s32 i;
    f32 sectVerts[8][3];
    saveTrackNum = gTrackNum;
    for (gTrackNum = TRACK_RACE_FIRST; gTrackNum <= TRACK_RACE_LAST; gTrackNum++) {
        if (!IsTrackUnlocked(gTrackNum)) continue;
        {
            loadPathData();
            xoffset = -(gPathMin[0] + gPathMax[0]) / 2;
            yoffset = -(gPathMin[2] + gPathMax[2]) / 2;
            zoffset = -(gPathMin[1] + gPathMax[1]) / 2;
            trackMap[gTrackNum].scale_to = 5000.0f / ((gPathMax[0] - gPathMin[0] > gPathMax[2] - gPathMin[2]) ? gPathMax[0] - gPathMin[0] : gPathMax[2] - gPathMin[2]);
            trackMBIdx = MBOX_FindObject_WORK(trackMapObjNames[gTrackNum], 0, gNumModels - 1, 1);
            trackRomObjDef = &gRomObjDefPtrs[trackMBIdx >> 10].romObjDefs[trackMBIdx & 1023];
            saved_map_lods[gTrackNum] = trackRomObjDef->lod[0];
            vtxSize = kNumMapSects * 64 + 64;
            map_vertex_blocks[gTrackNum] = audio_dma_sync(0, vtxSize + 7);
            mapObj[gTrackNum].verts = vtx = (Vtx_t *)(((uintptr_t)map_vertex_blocks[gTrackNum] + 7) & ~7);
            dlSize = kNumMapSects * MAP_DL_SECTION_BYTES + MAP_DL_FIXED_BYTES;
            map_display_list_blocks[gTrackNum] = audio_dma_sync(0, dlSize + 7);
            mapObj[gTrackNum].displayList = (u32 *)(dl = (Gfx *)(((uintptr_t)map_display_list_blocks[gTrackNum] + 7) & ~7));
            dl[0].words.w0 = 0xD7000000; dl[0].words.w1 = 0xFFFFFFFF;
            dl[1].words.w0 = 0xE7000000; dl[1].words.w1 = 0;
            dl[2].words.w0 = 0xE200001C; dl[2].words.w1 = 0xC8112230;
            dl[3].words.w0 = 0xFCFFFFFF; dl[3].words.w1 = 0xFFFE7C38;
            dl += 4;
            for (pointIdx = 0; pointIdx < kNumMapSects; pointIdx++) {
                if (pointIdx > 0) prevPt = (gResPath.numPts * (pointIdx - 1)) / kNumMapSects; else prevPt = -1;
                currStartPt = gResPath.numPts * pointIdx / kNumMapSects;
                currEndPt = pointIdx < kNumMapSects - 1 ? (gResPath.numPts * (pointIdx + 1)) / kNumMapSects : FindResPathIntersect(gCheckpoints.loopPt);
                if (pointIdx < kNumMapSects - 2) nextPt = (gResPath.numPts * (pointIdx + 2)) / kNumMapSects; else nextPt = -1;
                x1 = xoffset + gResPath.pts[currStartPt][0];
                y1 = yoffset + gResPath.pts[currStartPt][2];
                x2 = xoffset + gResPath.pts[currEndPt][0];
                y2 = yoffset + gResPath.pts[currEndPt][2];
                if (prevPt != -1) {
                    x0 = xoffset + gResPath.pts[prevPt][0];
                    y0 = yoffset + gResPath.pts[prevPt][2];
                } else {
                    x0 = x1 - (x2 - x1);
                    y0 = y1 - (y2 - y1);
                }
                if (nextPt != -1) {
                    x3 = xoffset + gResPath.pts[nextPt][0];
                    y3 = yoffset + gResPath.pts[nextPt][2];
                } else {
                    x3 = x2 + (x2 - x1);
                    y3 = y2 + (y2 - y1);
                }
                dx0 = x0 - x1; dy0 = y1 - y0;
                dx1 = x1 - x2; dy1 = y2 - y1;
                dx2 = x2 - x3; dy2 = y3 - y2;
                len0 = sqrtf(dx0 * dx0 + dy0 * dy0);
                len1 = sqrtf(dx1 * dx1 + dy1 * dy1);
                len2 = sqrtf(dx2 * dx2 + dy2 * dy2);
                dx0 *= 100.0f; dy0 *= 100.0f; dx1 *= 100.0f; dy1 *= 100.0f; dx2 *= 100.0f; dy2 *= 100.0f;
                dx0 /= len0; dy0 /= len0; dx1 /= len1; dy1 /= len1; dx2 /= len2; dy2 /= len2;
                sectVerts[0][0] = x1 + (dy0 + dy1) / 2;
                sectVerts[0][2] = y1 + (dx0 + dx1) / 2;
                sectVerts[0][1] = zoffset + gResPath.pts[currStartPt][1] + 50.0f;
                sectVerts[1][0] = x1 - (dy0 + dy1) / 2;
                sectVerts[1][2] = y1 - (dx0 + dx1) / 2;
                sectVerts[1][1] = zoffset + gResPath.pts[currStartPt][1] + 50.0f;
                sectVerts[2][0] = x1 - (dy0 + dy1) / 2;
                sectVerts[2][2] = y1 - (dx0 + dx1) / 2;
                sectVerts[2][1] = gPathMin[1] + zoffset - 50.0f;
                sectVerts[3][0] = x1 + (dy0 + dy1) / 2;
                sectVerts[3][2] = y1 + (dx0 + dx1) / 2;
                sectVerts[3][1] = gPathMin[1] + zoffset - 50.0f;
                sectVerts[4][0] = x2 + (dy1 + dy2) / 2;
                sectVerts[4][2] = y2 + (dx1 + dx2) / 2;
                sectVerts[4][1] = zoffset + gResPath.pts[currEndPt][1] + 50.0f;
                sectVerts[5][0] = x2 - (dy1 + dy2) / 2;
                sectVerts[5][2] = y2 - (dx1 + dx2) / 2;
                sectVerts[5][1] = zoffset + gResPath.pts[currEndPt][1] + 50.0f;
                sectVerts[6][0] = x2 - (dy1 + dy2) / 2;
                sectVerts[6][2] = y2 - (dx1 + dx2) / 2;
                sectVerts[6][1] = gPathMin[1] + zoffset - 50.0f;
                sectVerts[7][0] = x2 + (dy1 + dy2) / 2;
                sectVerts[7][2] = y2 + (dx1 + dx2) / 2;
                sectVerts[7][1] = gPathMin[1] + zoffset - 50.0f;
                if (pointIdx == kNumMapSects - 1) {
                    sectVerts[0][1] += 0.1f; sectVerts[1][1] += 0.1f;
                    sectVerts[2][1] += 0.1f; sectVerts[3][1] += 0.1f;
                }
                sectVerts[0][0] *= 0.01f; sectVerts[0][1] *= 0.01f; sectVerts[0][2] *= 0.01f;
                sectVerts[1][0] *= 0.01f; sectVerts[1][1] *= 0.01f; sectVerts[1][2] *= 0.01f;
                sectVerts[2][0] *= 0.01f; sectVerts[2][1] *= 0.01f; sectVerts[2][2] *= 0.01f;
                sectVerts[3][0] *= 0.01f; sectVerts[3][1] *= 0.01f; sectVerts[3][2] *= 0.01f;
                sectVerts[4][0] *= 0.01f; sectVerts[4][1] *= 0.01f; sectVerts[4][2] *= 0.01f;
                sectVerts[5][0] *= 0.01f; sectVerts[5][1] *= 0.01f; sectVerts[5][2] *= 0.01f;
                sectVerts[6][0] *= 0.01f; sectVerts[6][1] *= 0.01f; sectVerts[6][2] *= 0.01f;
                sectVerts[7][0] *= 0.01f; sectVerts[7][1] *= 0.01f; sectVerts[7][2] *= 0.01f;
                for (i = 0; i < (pointIdx == kNumMapSects - 1 ? 8 : 4); i++) {
                    vtx->ob[0] = sectVerts[i][0] * 16.0f;
                    vtx->ob[1] = sectVerts[i][1] * 16.0f;
                    vtx->ob[2] = sectVerts[i][2] * 16.0f;
                    vtx->flag = 0x8000;
                    vtx->tc[0] = 0;
                    vtx->tc[1] = 0;
                    vtx->cn[0] = race_map_vertex_color.channel[0];
                    vtx->cn[1] = race_map_vertex_color.channel[1];
                    vtx->cn[2] = race_map_vertex_color.channel[2];
                    vtx->cn[3] = race_map_vertex_color.channel[3];
                    vtx++;
                }
                if (pointIdx == kNumMapSects - 1) {
                    GFX(dl, 0x01004008, (uintptr_t)((Vtx_t *)map_vertex_blocks[gTrackNum] + pointIdx * 4));
                    GFX(dl, 0x01004010, (uintptr_t)map_vertex_blocks[gTrackNum]);
                } else {
                    GFX(dl, 0x01008010, (uintptr_t)((Vtx_t *)map_vertex_blocks[gTrackNum] + pointIdx * 4));
                }
                GFX(dl, 0x06000802, 0x000A0208);
                GFX(dl, 0x06020A04, 0x000C040A);
                GFX(dl, 0x06040C06, 0x000E060C);
                GFX(dl, 0x06060E00, 0x0008000E);
            }
            GFX(dl, 0xD7000002, 0xFFFFFFFF);
            GFX(dl, 0xDF000000, 0);
            mapObj[gTrackNum].flags = 0;
            mapObj[gTrackNum].texidx = 0;
            mapObj[gTrackNum].lodRad = 0.0f;
            trackRomObjDef->lod[0] = mapObj[gTrackNum];
            if (trackMap[gTrackNum].handle == -1) {
                trackMap[gTrackNum].handle = MBOX_NewObject(trackMBIdx, (uintptr_t)trackMap[gTrackNum].matrix, -1, 0);
                ScaleUV(unitmatrix, trackMap[gTrackNum].matrix, 1.0f);
                WPitchUV(-1.5707963705062866f, trackMap[gTrackNum].matrix);
                trackMap[gTrackNum].position[0] = buildRaceTrackMaps_mapPos[0];
                trackMap[gTrackNum].position[1] = buildRaceTrackMaps_mapPos[1];
                trackMap[gTrackNum].position[2] = buildRaceTrackMaps_mapPos[2];
            }
            unloadPathData();
        }
    }
    gTrackNum = saveTrackNum;
    trackMBIdx = MBOX_FindObject_WORK(trackMapObjNames[TRACK_MAP_SELECTED], 0, gNumModels - 1, 1);
    trackRomObjDef = &gRomObjDefPtrs[trackMBIdx >> 10].romObjDefs[trackMBIdx & 1023];
    saved_map_lods[MAP_LOD_SELECTED] = trackRomObjDef->lod[0];
    map_vertex_blocks[MAP_LOD_SELECTED] = audio_dma_sync(0, kNumMapSects * 64 + 64 + 7);
    mapObj[MAP_LOD_SELECTED].verts = (void *)(((uintptr_t)map_vertex_blocks[MAP_LOD_SELECTED] + 7) & ~7);
    map_display_list_blocks[MAP_LOD_SELECTED] = audio_dma_sync(0, kNumMapSects * MAP_DL_SECTION_BYTES + MAP_DL_FIXED_BYTES + 7);
    mapObj[MAP_LOD_SELECTED].displayList = (u32 *)(((uintptr_t)map_display_list_blocks[MAP_LOD_SELECTED] + 7) & ~7);
    mapObj[MAP_LOD_SELECTED].flags = 0;
    mapObj[MAP_LOD_SELECTED].texidx = 0;
    mapObj[MAP_LOD_SELECTED].lodRad = 0.0f;
    trackRomObjDef->lod[0] = mapObj[MAP_LOD_SELECTED];
    if (trackMap[TRACK_MAP_SELECTED].handle == -1) {
        trackMap[TRACK_MAP_SELECTED].handle = MBOX_NewObject(trackMBIdx, (uintptr_t)trackMap[TRACK_MAP_SELECTED].matrix, -1, 0);
        ScaleUV(unitmatrix, trackMap[TRACK_MAP_SELECTED].matrix, 1.0f);
        WPitchUV(-1.5707963705062866f, trackMap[TRACK_MAP_SELECTED].matrix);
        trackMap[TRACK_MAP_SELECTED].position[0] = buildRaceTrackMaps_selMapPos[0];
        trackMap[TRACK_MAP_SELECTED].position[1] = buildRaceTrackMaps_selMapPos[1];
        trackMap[TRACK_MAP_SELECTED].position[2] = buildRaceTrackMaps_selMapPos[2];
    }
}

/* The stunt arenas' maps: their models. */
void buildStuntTrackMaps(void)
{
    s32 saveTrackNum;
    saveTrackNum = gTrackNum;
    for (gTrackNum = TRACK_STUNT_FIRST; gTrackNum <= TRACK_STUNT_LAST; gTrackNum++) {
        if (!IsTrackUnlocked(gTrackNum)) continue;
        if (trackMap[gTrackNum].handle == -1) {
            trackMap[gTrackNum].handle = MBOX_NewObject(MBOX_FindObject_WORK(trackMapObjNames[gTrackNum], 0, gNumModels - 1, 1), (uintptr_t)trackMap[gTrackNum].matrix, -1, 0);
            ScaleUV(unitmatrix, trackMap[gTrackNum].matrix, 1.0f);
            WPitchUV(-1.5707963705062866f, trackMap[gTrackNum].matrix);
            trackMap[gTrackNum].position[0] = buildStuntTrackMaps_mapPos[0];
            trackMap[gTrackNum].position[1] = buildStuntTrackMaps_mapPos[1];
            trackMap[gTrackNum].position[2] = buildStuntTrackMaps_mapPos[2];
        }
    }
    gTrackNum = saveTrackNum;
}

/* The battle arenas' maps: their models. */
void buildBattleTrackMaps(void)
{
    s32 saveTrackNum;
    saveTrackNum = gTrackNum;
    for (gTrackNum = TRACK_BATTLE_FIRST; gTrackNum <= TRACK_BATTLE_LAST; gTrackNum++) {
        if (!IsTrackUnlocked(gTrackNum)) continue;
        if (trackMap[gTrackNum].handle == -1) {
            trackMap[gTrackNum].handle = MBOX_NewObject(MBOX_FindObject_WORK(trackMapObjNames[gTrackNum], 0, gNumModels - 1, 1), (uintptr_t)trackMap[gTrackNum].matrix, -1, 0);
            ScaleUV(unitmatrix, trackMap[gTrackNum].matrix, 1.0f);
            WPitchUV(-1.5707963705062866f, trackMap[gTrackNum].matrix);
            trackMap[gTrackNum].position[0] = buildBattleTrackMaps_mapPos[0];
            trackMap[gTrackNum].position[1] = buildBattleTrackMaps_mapPos[1];
            trackMap[gTrackNum].position[2] = buildBattleTrackMaps_mapPos[2];
        }
    }
    gTrackNum = saveTrackNum;
}

/* The obstacle course has no map. */
void buildObstacleTrackMaps(void)
{
}

/* Remove every track map's model, wait for the frame being drawn, give the map models their first LODs back and
 * free the race maps' display lists and vertices. */
void clearTrackMaps(void)
{
    s32 i,model;
    RomObjDef *def;
    if (trackMapsBuilt) {
        for (i = 0; i < 20; i++) {
            if (trackMap[i].handle != -1) {
                ZOID_RemoveObject((s16)trackMap[i].handle);
                trackMap[i].handle = -1;
            }
        }
        while (gRenderInProgress) { pc_os_spin_yield("clearTrackMaps"); }
        for (i = 0; i < 7; i++) {
            model = MBOX_FindObject_WORK(trackMapObjNames[i < MAP_LOD_SELECTED ? i : TRACK_MAP_SELECTED],0,gNumModels - 1,1);
            def = &gRomObjDefPtrs[model >> 10].romObjDefs[model & 0x3FF];
            def->lod[0] = saved_map_lods[i];
            if (map_display_list_blocks[i]) DeallocateMemory(N64A0(map_display_list_blocks[i]));
            map_display_list_blocks[i] = 0;
            if (map_vertex_blocks[i]) DeallocateMemory(N64A0(map_vertex_blocks[i]));
            map_vertex_blocks[i] = 0;
            mapObj[i].displayList = 0;
            mapObj[i].verts = 0;
        }
        trackMapsBuilt = 0;
    }
}

typedef union MapColor { u8 c[4]; u32 w; } MapColor;
typedef struct MapVtx { s16 ob[3]; u16 flag; s16 tc[2]; u8 cn[4]; } MapVtx;
/* The track maps, each frame: each placed by its offset and scale (the highlighted one a little smaller), and the
 * race maps' sections coloured: the other tracks in their own dim colour, the highlighted one bright with a darker
 * band that runs along the track once in 5 seconds (the other way for a backwards race). */
void animTrackMaps(void)
{
    static f32 mapPos[3] = { -100.0f, 30.0f, 200.0f };
    s32 trackIdx;
    s32 pointIdx;
    MapVtx *pStrip;
    s32 fade;
    MapColor col;
    s32 i;
    s32 mapIdx;
    if (trackMapsBuilt) {
        for (trackIdx = 0; trackIdx <= TRACK_MAP_SELECTED; trackIdx++) {
            if (trackMap[trackIdx].handle != -1) {
                if (trackIdx < TRACK_MAP_SELECTED) {
                    ScaleUV(unitmatrix, trackMap[trackIdx].matrix, trackMap[trackIdx].scale_from);
                    WPitchUV(-1.5707963705062866f, trackMap[trackIdx].matrix);
                    WRollUV(-trackMap[trackIdx].angle, trackMap[trackIdx].matrix);
                    trackMap[trackIdx].position[0] = mapPos[0];
                    trackMap[trackIdx].position[1] = mapPos[1];
                    trackMap[trackIdx].position[2] = mapPos[2];
                    trackMap[trackIdx].position[0] += trackMap[trackIdx].offset_x;
                    trackMap[trackIdx].position[1] += trackMap[trackIdx].offset_y;
                    if (trackIdx == gTrackNum) {
                        ScaleUV(trackMap[trackIdx].matrix, trackMap[trackIdx].matrix, 0.8f);
                        trackMap[trackIdx].position[0] = trackMap[trackIdx].position[0] * 0.8f;
                        trackMap[trackIdx].position[1] = trackMap[trackIdx].position[1] * 0.8f;
                        trackMap[trackIdx].position[2] = trackMap[trackIdx].position[2] * 0.8f;
                    }
                }
                mapIdx = trackIdx == TRACK_MAP_SELECTED ? MAP_LOD_SELECTED : trackIdx;
                if (trackIdx < MAP_LOD_SELECTED || trackIdx == TRACK_MAP_SELECTED) {
                    if (mapObj[mapIdx].displayList) {
                        for (pointIdx = 0; pointIdx < kNumMapSects; pointIdx++) {
                            pStrip = (MapVtx *)MAPLOD_PTR(mapObj[mapIdx].verts);
                            pStrip += pointIdx * 4;
                            if (gOptions[27] == 0) fade = pathPos * kNumMapSects - pointIdx;
                            else fade = pathPos * kNumMapSects + pointIdx;
                            if (fade < 0) fade += kNumMapSects;
                            if (fade > kNumMapSects - 1) fade -= kNumMapSects;
                            fade *= 2;
                            if (mapIdx != gTrackNum && mapIdx < 6) {
                                col.c[0] = (mapIdx + 1) & 1 ? 127 : 31;
                                col.c[1] = (mapIdx + 1) & 2 ? 127 : 31;
                                if ((mapIdx + 1) & 4) col.c[2] = 127; else col.c[2] = 31;
                            } else if (mapIdx < 6) {
                                if (fade > 63) fade = 64;
                                if ((gTrackNum + 1) & 1) col.c[0] = 255 - fade; else col.c[0] = 255 - fade * 3;
                                if ((gTrackNum + 1) & 2) col.c[1] = 255 - fade; else col.c[1] = 255 - fade * 3;
                                if ((gTrackNum + 1) & 4) col.c[2] = 255 - fade; else col.c[2] = 255 - fade * 3;
                            } else {
                                if (fade > 63) fade = 64;
                                if ((currentTrackMap + 1) & 1) col.c[0] = 255 - fade; else col.c[0] = 255 - fade * 3;
                                if ((currentTrackMap + 1) & 2) col.c[1] = 255 - fade; else col.c[1] = 255 - fade * 3;
                                if ((currentTrackMap + 1) & 4) col.c[2] = 255 - fade; else col.c[2] = 255 - fade * 3;
                            }
                            if (pointIdx == kNumMapSects - 1) col.w = 0xFFFFFFFF;
                            for (i = 0; i < (pointIdx == kNumMapSects - 1 ? 8 : 4); i++) {
                                if (i & 2) col.w = 255;
                                pStrip->cn[0] = col.c[2];
                                pStrip->cn[1] = col.c[1];
                                pStrip->cn[2] = col.c[0];
                                pStrip->cn[3] = col.c[3];
                                pStrip++;
                            }
                        }
                    }
                }
            }
        }
        pathPos += gFrameSeconds / 5.0f;
        while (pathPos > 1.0f) pathPos -= 1.0f;
    }
}

/* Load a race track's path (its checkpoints and its path data, WAVE_PATHS + the track); pathWaveIdx -1 for the
 * others. */
void loadPathData(void)
{
    if (gTrackNum >= TRACK_RACE_FIRST && gTrackNum <= TRACK_RACE_LAST) {
        pathWaveIdx = LoadWave(gTrackNum + WAVE_PATHS, 0, 0, 0, 0);
        gPathWave = GetWaveInstanceAddr(pathWaveIdx);
        ReadCheckpointData();
        ReadPathData();
    } else pathWaveIdx = -1;
}

/* Unload the path loadPathData loaded. */
void unloadPathData(void)
{
    if (pathWaveIdx != -1) UnloadWaveInstance(pathWaveIdx);
    pathWaveIdx = -1;
}

/* Make the buttons that don't exist yet (types 0-2 see-through), then the title, the cursor, and the A and B icons
 * with their arrows. */
void trackscr_initButtons(void)
{
    Color color = trackscr_init_button_color;
    s32 i;
    s16 texture_index;
    f32 yaw;
    for (i = 0; i < TRACK_BUTTONS; i++) {
        if (trackscr_buttonInfo[i].handle == -1) {
            memcpy(trackscr_buttonInfo[i].matrix, unitmatrix, 36);
            trackscr_buttonInfo[i].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(trackscr_buttonNames[trackscr_buttonInfo[i].type], 0, gNumModels - 1, 1),
                (uintptr_t)trackscr_buttonInfo[i].matrix, -1,
                (trackscr_buttonInfo[i].type == 0 || trackscr_buttonInfo[i].type == 1 || trackscr_buttonInfo[i].type == 2) ? 0x42000 : 0);
            ZOID_SetColor((s16)trackscr_buttonInfo[i].handle, (s32 *)&color);
        }
    }
    ZOID_SetTexture((s16)trackscr_buttonInfo[TRACK_BUTTON_TITLE].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_title_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(trackscr_buttonInfo[TRACK_BUTTON_TITLE].matrix, unitmatrix, 36);
    yaw = -1.5707963705062866f;
    WPitchUV(yaw, trackscr_buttonInfo[TRACK_BUTTON_TITLE].matrix);
    trackscr_buttonInfo[TRACK_BUTTON_TITLE].position[0] = 0.0f;
    trackscr_buttonInfo[TRACK_BUTTON_TITLE].position[1] = 115.0f;
    trackscr_buttonInfo[TRACK_BUTTON_TITLE].position[2] = 200.0f;
    ZOID_SetTexture((s16)trackscr_buttonInfo[TRACK_BUTTON_CURSOR].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_cursor_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    ZOID_SetTexture((s16)trackscr_buttonInfo[TRACK_BUTTON_A_ICON].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_a_button_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(trackscr_buttonInfo[TRACK_BUTTON_A_ICON].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[TRACK_BUTTON_A_ICON].matrix);
    trackscr_buttonInfo[TRACK_BUTTON_A_ICON].position[0] = 75.1f;
    trackscr_buttonInfo[TRACK_BUTTON_A_ICON].position[1] = 55.2f;
    trackscr_buttonInfo[TRACK_BUTTON_A_ICON].position[2] = 100.0f;
    ScaleUV(trackscr_buttonInfo[TRACK_BUTTON_A_ICON].matrix, trackscr_buttonInfo[TRACK_BUTTON_A_ICON].matrix, 0.5f);
    memcpy(trackscr_buttonInfo[TRACK_BUTTON_A_ARROW].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[TRACK_BUTTON_A_ARROW].matrix);
    WRollUV(3.1415927410125732f, trackscr_buttonInfo[TRACK_BUTTON_A_ARROW].matrix);
    trackscr_buttonInfo[TRACK_BUTTON_A_ARROW].position[0] = 87.1f;
    trackscr_buttonInfo[TRACK_BUTTON_A_ARROW].position[1] = 55.2f;
    trackscr_buttonInfo[TRACK_BUTTON_A_ARROW].position[2] = 100.0f;
    ZOID_SetTexture((s16)trackscr_buttonInfo[TRACK_BUTTON_B_ICON].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_b_button_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(trackscr_buttonInfo[TRACK_BUTTON_B_ICON].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[TRACK_BUTTON_B_ICON].matrix);
    trackscr_buttonInfo[TRACK_BUTTON_B_ICON].position[0] = -75.1f;
    trackscr_buttonInfo[TRACK_BUTTON_B_ICON].position[1] = 55.20f;
    trackscr_buttonInfo[TRACK_BUTTON_B_ICON].position[2] = 100.0f;
    ScaleUV(trackscr_buttonInfo[TRACK_BUTTON_B_ICON].matrix, trackscr_buttonInfo[TRACK_BUTTON_B_ICON].matrix, 0.5f);
    memcpy(trackscr_buttonInfo[TRACK_BUTTON_B_ARROW].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[TRACK_BUTTON_B_ARROW].matrix);
    trackscr_buttonInfo[TRACK_BUTTON_B_ARROW].position[0] = -87.1f;
    trackscr_buttonInfo[TRACK_BUTTON_B_ARROW].position[1] = 55.20f;
    trackscr_buttonInfo[TRACK_BUTTON_B_ARROW].position[2] = 100.0f;
    trackscr_buttonsInited = 1;
    trackscr_buttonsJustInited = 1;
    ZeroCameras();
}

/* Forget the buttons; their models are removed only in the front end (in a race the scene was reset already). */
void trackscr_uninitButtons(void)
{
    s32 i;
    for (i = 0; i < TRACK_BUTTONS; i++) {
        if (trackscr_buttonInfo[i].handle != -1) {
            if (gGameState & GS_FRONTEND_MASK) ZOID_RemoveObject((s16)trackscr_buttonInfo[i].handle);
            trackscr_buttonInfo[i].handle = -1;
        }
    }
    trackscr_buttonsInited = 0;
}

/* Lay the buttons out for this frame (once they exist): as carscr_updateButtons, with the option's value button
 * turning with its label (but not for an option that can't be changed: a circuit's, or a ghost race's laps), and
 * the buttons fading over one button's height past the list's ends. */
void trackscr_updateButtons(void)
{
    static f32 scrollSpeed;
    static f32 cursorSpeed;
    s32 buttonIdx;
    f32 prevPos[3];
    f32 buttonY;
    f32 selectedPos;
    s8 clearScrollSpeed = 1;
    Texture *tex;
    s16 texIdx;
    Color buttonColor = trackscr_update_button_color;
    if (trackscr_buttonsInited) {
        if (trackscr_gButtonScale * ((trackscr_gNumOptions - 1) * trackscr_gButtonSpace) > trackscr_maxY - trackscr_minY)
            selectedPos = trackscr_maxY - trackscr_gActiveOption * (trackscr_maxY - trackscr_minY) / (trackscr_gNumOptions - 1);
        else selectedPos = trackscr_maxY - trackscr_gButtonScale * (trackscr_gActiveOption * trackscr_gButtonSpace);
        memcpy(prevPos, trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position, 12);
        trackscr_buttonInfo[TRACK_BUTTON_CURSOR].angle += 6.2831854820251465f * gFrameSeconds;
        if (trackscr_buttonInfo[TRACK_BUTTON_CURSOR].angle > 6.2831854820251465f) trackscr_buttonInfo[TRACK_BUTTON_CURSOR].angle -= 6.2831854820251465f;
        trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[0] = trackscr_buttonX * trackscr_cursorZ / trackscr_buttonZ + -180.0f * trackscr_gButtonScale;
        trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] = selectedPos * trackscr_cursorZ / trackscr_buttonZ;
        trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[2] = trackscr_cursorZ;
        if (!trackscr_buttonsJustInited) {
            if (60.0f * fabsf(trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] - prevPos[1]) / 10.0f > cursorSpeed) {
                cursorSpeed = 60.0f * fabsf(trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] - prevPos[1]) / 10.0f;
                cursorSpeed = cursorSpeed < 540.0f * trackscr_gButtonScale ? cursorSpeed : 540.0f * trackscr_gButtonScale;
                cursorSpeed = cursorSpeed > 180.0f * trackscr_gButtonScale ? cursorSpeed : 180.0f * trackscr_gButtonScale;
            }
            if (trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] > prevPos[1] + cursorSpeed * gFrameSeconds)
                trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] = prevPos[1] + cursorSpeed * gFrameSeconds;
            else if (trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] < prevPos[1] - cursorSpeed * gFrameSeconds)
                trackscr_buttonInfo[TRACK_BUTTON_CURSOR].position[1] = prevPos[1] - cursorSpeed * gFrameSeconds;
            else cursorSpeed = 180.0f * trackscr_gButtonScale;
        }
        ScaleUV(unitmatrix, trackscr_buttonInfo[TRACK_BUTTON_CURSOR].matrix, trackscr_gButtonScale * 0.4f);
        WRollUV(trackscr_buttonInfo[TRACK_BUTTON_CURSOR].angle, trackscr_buttonInfo[TRACK_BUTTON_CURSOR].matrix);
        WPitchUV(1.5707963705062866f, trackscr_buttonInfo[TRACK_BUTTON_CURSOR].matrix);
        buttonY = selectedPos + trackscr_gButtonScale * (trackscr_gActiveOption * trackscr_gButtonSpace);
        for (buttonIdx = 0; buttonIdx < TRACK_OPTS; buttonIdx++) {
            if (buttonIdx == trackscr_gOpts[trackscr_gActiveOption]) {
                if (trackscr_buttonInfo[buttonIdx].angle < 3.1415927410125732f) trackscr_buttonInfo[buttonIdx].angle += 12.566370964050293f * gFrameSeconds;
                if (trackscr_buttonInfo[buttonIdx].angle > 3.1415927410125732f) trackscr_buttonInfo[buttonIdx].angle = 3.1415927410125732f;
                if (trackscr_buttonsJustInited) trackscr_buttonInfo[buttonIdx].angle = 3.1415927410125732f;
            } else {
                if (trackscr_buttonInfo[buttonIdx].angle > 0.0f) trackscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
                if (trackscr_buttonInfo[buttonIdx].angle < 0.0f) trackscr_buttonInfo[buttonIdx].angle = 0.0f;
                if (trackscr_buttonsJustInited) trackscr_buttonInfo[buttonIdx].angle = 0.0f;
            }
            if ((gRace != kCircuit || (gCircuitTrack == 0 && buttonIdx == TRACK_OPT_DEATH_MODE)) && (gRace != kGhost || buttonIdx != TRACK_OPT_LAPS)) trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].angle = trackscr_buttonInfo[buttonIdx].angle;
            else trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].angle = 0.0f;
            memcpy(prevPos, trackscr_buttonInfo[buttonIdx].position, 12);
            trackscr_buttonInfo[buttonIdx].position[0] = trackscr_buttonX + -155.0f * trackscr_gButtonScale;
            trackscr_buttonInfo[buttonIdx].position[1] = buttonY;
            trackscr_buttonInfo[buttonIdx].position[2] = trackscr_buttonZ;
            if (!trackscr_buttonsJustInited) {
                if (60.0f * fabsf(trackscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) / 10.0f > scrollSpeed) {
                    scrollSpeed = 60.0f * fabsf(trackscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) / 10.0f;
                    scrollSpeed = scrollSpeed < 540.0f * trackscr_gButtonScale ? scrollSpeed : 540.0f * trackscr_gButtonScale;
                    scrollSpeed = scrollSpeed > 180.0f * trackscr_gButtonScale ? scrollSpeed : 180.0f * trackscr_gButtonScale;
                }
                if (trackscr_buttonInfo[buttonIdx].position[1] > prevPos[1] + scrollSpeed * gFrameSeconds) {
                    trackscr_buttonInfo[buttonIdx].position[1] = prevPos[1] + scrollSpeed * gFrameSeconds;
                    clearScrollSpeed = 0;
                } else if (trackscr_buttonInfo[buttonIdx].position[1] < prevPos[1] - scrollSpeed * gFrameSeconds) {
                    trackscr_buttonInfo[buttonIdx].position[1] = prevPos[1] - scrollSpeed * gFrameSeconds;
                    clearScrollSpeed = 0;
                }
            }
            trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].position[0] = trackscr_buttonX + 50.0f * trackscr_gButtonScale;
            trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].position[1] = trackscr_buttonInfo[buttonIdx].position[1];
            trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].position[2] = trackscr_buttonInfo[buttonIdx].position[2];
            ScaleUV(unitmatrix, trackscr_buttonInfo[buttonIdx].matrix, trackscr_gButtonScale);
            ScaleUV(unitmatrix, trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].matrix, trackscr_gButtonScale);
            WPitchUV(trackscr_buttonInfo[buttonIdx].angle - 1.5707963705062866f, trackscr_buttonInfo[buttonIdx].matrix);
            WPitchUV(trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].angle - 1.5707963705062866f, trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].matrix);
            if (trackscr_buttonInfo[buttonIdx].angle > 1.5707963705062866f) tex = MBOX_FindTexture_WORK(trackscr_left_select_texture_name, &texIdx, 0, gNumModels - 1, 1);
            else tex = MBOX_FindTexture_WORK(trackscr_left_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(trackscr_buttonInfo[buttonIdx].handle, (intptr_t)tex, -1);
            if (trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].angle > 1.5707963705062866f) tex = MBOX_FindTexture_WORK(trackscr_right_select_texture_name, &texIdx, 0, gNumModels - 1, 1);
            else tex = MBOX_FindTexture_WORK(trackscr_right_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].handle, (intptr_t)tex, -1);
            if (trackscr_buttonInfo[buttonIdx].position[1] > trackscr_maxY + trackscr_gButtonSpace * trackscr_gButtonScale || trackscr_buttonInfo[buttonIdx].position[1] < trackscr_minY - trackscr_gButtonSpace * trackscr_gButtonScale) trackscr_buttonInfo[buttonIdx].alpha = 0;
            else if (trackscr_buttonInfo[buttonIdx].position[1] > trackscr_maxY) trackscr_buttonInfo[buttonIdx].alpha = 255 - (trackscr_buttonInfo[buttonIdx].position[1] - trackscr_maxY) * 255.0f / (trackscr_gButtonSpace * trackscr_gButtonScale);
            else if (trackscr_buttonInfo[buttonIdx].position[1] < trackscr_minY) trackscr_buttonInfo[buttonIdx].alpha = 255 - (trackscr_minY - trackscr_buttonInfo[buttonIdx].position[1]) * 255.0f / (trackscr_gButtonSpace * trackscr_gButtonScale);
            else trackscr_buttonInfo[buttonIdx].alpha = 255;
            if (!OptionValid(buttonIdx)) trackscr_buttonInfo[buttonIdx].alpha = 0;
            trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].alpha = trackscr_buttonInfo[buttonIdx].alpha;
            buttonColor.channel[3] = trackscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor(trackscr_buttonInfo[buttonIdx].handle, (s32 *)&buttonColor);
            ZOID_SetColor(trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].handle, (s32 *)&buttonColor);
            if (!OptionValid(buttonIdx)) {
                MBOX_HideObject(trackscr_buttonInfo[buttonIdx].handle, 0, 15);
                MBOX_HideObject(trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].handle, 0, 15);
            } else if (trackscr_buttonInfo[buttonIdx].alpha == 0) {
                MBOX_HideObject(trackscr_buttonInfo[buttonIdx].handle, 0, 15);
                MBOX_HideObject(trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].handle, 0, 15);
                buttonY -= trackscr_gButtonSpace * trackscr_gButtonScale;
            } else {
                MBOX_ShowObject(trackscr_buttonInfo[buttonIdx].handle, 0, 15);
                MBOX_ShowObject(trackscr_buttonInfo[TRACK_VALUE_BUTTON(buttonIdx)].handle, 0, 15);
                buttonY -= trackscr_gButtonSpace * trackscr_gButtonScale;
            }
        }
        if (clearScrollSpeed == 1) scrollSpeed = 0.0f;
        trackscr_buttonsJustInited = 0;
    }
}

