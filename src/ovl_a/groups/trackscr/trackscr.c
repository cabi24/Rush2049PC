/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image A track-select screen: the whole N64 trackscr.c IPA unit, root updateTrackScreen (updateTrackScreen,
 * kept; called by the game code). Donor: MAT3 GameSource trackscr.c (GC 0x80087abc..0x8008d30c), direct
 * typed ports, N64 offsets/addresses from the retail words. Identities (call graph + MAT3 statement order):
 *   updateTrackScreen updateTrackScreen   trackscr_InitScreen _InitScreen       trackscr_UninitScreen _UninitScreen
 *   showTrackSelect showTrackSelect     TrackValid TrackValid        OptionValid OptionValid
 *   SetFogIntensity SetFogIntensity     unloadTrackScreen unloadTrackScreen buildTrackMap buildTrackMap
 *   clearTrackMap clearTrackMap       trackscr_animTrackMap animTrackMap      buildTrackMaps buildTrackMaps
 *   animTrackMaps animTrackMaps       trackscr_initButtons initButtons       trackscr_updateButtons updateButtons
 * Inlined statics, named by their retail 8-byte deleted stubs (the compiled object emits them at those places):
 *   setCamera setCamera, BlowFog BlowFog, loadTrackScreen loadTrackScreen, unloadPathData unloadPathData,
 *   loadPathData loadPathData, buildRaceTrackMaps buildRaceTrackMaps, buildObstacleTrackMaps buildObstacleTrackMaps,
 *   buildBattleTrackMaps buildBattleTrackMaps, buildStuntTrackMaps buildStuntTrackMaps.
 * Every function except the kept root has the private IPA ABI (no callee saves; the root saves s0-s8 and
 * $f20-$f30), so members match only in this unit. Function order is MAT3's file order (as1 breaks scheduling
 * ties by source line, and the inlined helpers' lines must follow their callers).
 * Bodies: (E5DC, DFEC, BBB4, CD14, ACB0, B834 ports); (AE50, B388, B940, AEA8, AEB0;
 *); (A634); C93C = the installed member
 * runtime_a_c93c_closure, text unchanged. C5EC/B3EC/A820 are stand-ins (context only).
 * Strict in this unit: C93C, BBB4, DFEC, ACB0, B834, A634, AE50, B388, B940 (+ later AEB0, B3EC, C5EC, the stubs and,
 * , E5DC). E5DC case 14's `gOptions[29] = 3` store is absolute in retail (lui at,0x8014; sb 0x6125(at)): it is
 * written through the second, address-named symbol gOptions_fog_intensity (= &gOptions[29]; owner-approved alias 2026-10-25;
 * every spelling on gOptions folds into the case's v0 base). CD14/A820 are context.
 */
typedef signed char s8; typedef unsigned char u8;
typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32; typedef float f32;
typedef struct Blit Blit;
typedef struct MultiBlit {
    char *name;s16 x,y,width,height,top,bottom,left,right;
    u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;
} MultiBlit;
typedef struct tMenuButton {s32 other,handle;f32 angle,matrix[3][3],position[3];u8 alpha,pad[3];} tMenuButton;
typedef union Color {u32 word;u8 channel[4];} Color;
typedef struct Texture Texture;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct FileInfo {void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 controller;} FileInfo;
typedef struct Profile Profile;
struct Profile {Profile **next,**previous;FileInfo **file;u32 id0,id1;u8 name[24];u8 **stats;};
typedef struct Player {u32 held,pressed,levels,repeated;u8 unknown10[56];Profile **save;} Player;
typedef struct CircuitTrack {u8 trackNum,unknown1,mirror,fog,wind;} CircuitTrack;
typedef struct TrackSlot {s8 gTrackNum;f32 pos[3];f32 targetPos;s8 slot;} TrackSlot;   /* anon_gTrackNum_pos_18 */

/* game globals */
extern u32 gGameState;             /* gGameState */
extern u32 gSkipToGameState;             /* gSkipToGameState */
extern s16 gNumPlayers;             /* gNumPlayers */
extern s32 gRace;             /* gRace (0 OneRace 1 Practice 2 Ghost 3 Circuit 4 Stunt 5 Obstacle 6 Battle) */
extern Player gPlayers[];        /* gPlayers */
extern s8 gTrackNum;              /* gTrackNum */
extern s8 gOptions[];            /* gOptions */
extern s8 gOptions_fog_intensity;             /* gOptions[29] under its own name (retail case-14 store) */
/* PC: one name for one place. An optimizing GCC takes two external variables for two objects, so after the store
 * through this name updateTrackScreen could go on with a value of gOptions[29] it had read before
 * (docs/port/OPTIMIZED.md, tools/port/alias_check.py). */
#define gOptions_fog_intensity (gOptions[29])
/* PC: the Drones option (gOptions[22], item 17) goes up to the cars a race may hold less one ([game] cars,
 * docs/port/MORECARS.md 1.3, 10.1): 5 with the setting at original, as on the N64. Its reset value and its default
 * (GetDefaultOption, GetDefaultOption) stay 5, so a player who never touches it races 6 cars. */
#define PC_DRONES_MAX (pc_cars_setting() - 1)
extern s8 gSoftReset;              /* gSoftReset */
extern u32 gActiveEdges;             /* gActiveEdges */
extern u32 gActiveLevels;             /* gActiveLevels */
extern u32 gActiveAutoEdges;             /* gActiveAutoEdges */
extern s8 gMirrorControls;              /* gMirrorControls */
extern s8 gCircuitTrack;              /* gCircuitTrack */
extern u8 gCircuitPlayer;              /* gCircuitPlayer */
extern CircuitTrack gCircuitTracks[];  /* gCircuitTracks */
extern s8 expansion_pak_present;              /* (N64) all-tracks flag: track 5 may be started */
extern s8 gWind;              /* gWind */
extern volatile f32 gFrameSeconds;    /* gFrameSeconds */
extern f32 unitmatrix[12];         /* unitmatrix */
extern volatile u8 gNumModels;     /* gNumModels */

/* trackscr data */
extern tMenuButton trackscr_buttonInfo[48];  /* buttonInfo */
extern s32 trackscr_buttonsInited,trackscr_buttonsJustInited;      /* buttonsInited, buttonsJustInited */
extern f32 trackscr_gButtonScale;                 /* gButtonScale */
extern f32 trackscr_gButtonSpace;                 /* gButtonSpace */
extern f32 trackscr_buttonX;                 /* buttonX */
extern f32 trackscr_maxY;                 /* maxY */
extern f32 trackscr_minY;                 /* minY */
extern f32 trackscr_buttonZ;                 /* buttonZ */
extern f32 trackscr_cursorZ;                 /* cursorZ */
extern Color trackscr_init_button_color;               /* initButtons buttonColor template */
extern Color trackscr_update_button_color;               /* updateButtons buttonColor template */
extern char *trackscr_buttonNames[];             /* buttonNames */
extern char trackscr_title_texture_name[],trackscr_cursor_texture_name[],trackscr_a_button_texture_name[],trackscr_b_button_texture_name[];  /* "BUTTON_TITLE" "CURSOR" "A1" "B1" */
extern char trackscr_left_select_texture_name[],trackscr_left_button_texture_name[],trackscr_right_select_texture_name[],trackscr_right_button_texture_name[];  /* "BUTTON_SELECT" "BUTTON" x2 */
extern f32 yFog[];               /* yFog */
extern MultiBlit trackscr_mblit[];         /* mblit (17 entries on N64) */
extern Blit *trackscr_blit;               /* blit */
extern s8 trackscr_inited;                  /* inited */
extern f32 pathPos;                 /* pathPos */
extern s16 oldTrack;                 /* oldTrack */
extern s16 gNumTracks;                 /* gNumTracks */
extern s16 gTrack;                 /* gTrack */
extern s16 trackscr_gActiveOption;                 /* gActiveOption */
extern s16 trackscr_gFirstOption;                 /* gFirstOption */
extern s16 trackscr_gNumOptions;                 /* gNumOptions */
extern s8 trackscr_gActiveOptionEnabled;                  /* gActiveOptionEnabled */
extern s8 trackscr_gActiveZ;                  /* gActiveZ */
extern s16 trackscr_gOpts[21];             /* gOpts */
extern TrackSlot gTracks[19];       /* gTracks */
extern f32 trackscr_cursorSpeed;                 /* updateButtons static cursorSpeed */
extern f32 trackscr_scrollSpeed;                 /* updateButtons static scrollSpeed */
extern s32 currentTrackMap;                 /* currentTrackMap */
extern s32 desiredTrackMap;                 /* desiredTrackMap */
extern f32 mapTransition;                 /* mapTransition */
extern f32 trackscr_InitScreen_fogPos[3];              /* _InitScreen static fogPos */
extern u16 trackscr_fogTileSize,trackscr_fogTileSize_ult;      /* BlowFog static fogTileSize.uls/.ult */

/* game functions */
extern void *memcpy(void *,const void *,u32);
extern f32 fabsf(f32);
#pragma intrinsic (fabsf)
extern void setGameState(s32);                   /* setGameState */
extern s8 PS_PakChanged(s32);                     /* (N64) controller-pak removed test */
extern void PlayStartABSound(u32);            /* PlayStartABSound */
extern void PlayLRSound(u32);                   /* PlayLRSound */
extern s32 AudPlayEffect(u32,u32,u32,u8);    /* AudPlayEffect */
extern void audio_bus_mix(Profile **,u8,s8);      /* SavePlayerOption */
extern s8 GetPlayerOption(Profile **,u8);            /* GetPlayerOption */
extern void SavePlayerTrack(Profile **,s32,u8);   /* SavePlayerTrack */
extern u8 GetPlayerTrack(Profile **,s32);          /* GetPlayerTrack */
extern void SetOptions(void);               /* SetOptions */
extern void particle_lifetime_set(void);          /* createBG */
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);  /* NewMultiBlit */
extern void ZeroCamera(s16);           /* ZeroCamera */
extern void UpdateCameras(void);         /* UpdateCameras */
extern void ClearRushControls(void);            /* ClearRushControls */
extern s32 IsTrackUnlocked(s32);                    /* IsTrackUnlocked */
extern s32 MBOX_NewObject(u32,uintptr_t,s32,u32);     /* MBOX_NewObject */
extern s32 MBOX_FindObject_WORK(char *,s8,s8,s8);   /* MBOX_FindObject_WORK */
extern Texture *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);  /* MBOX_FindTexture_WORK */
extern void ZOID_SetTexture(s16,intptr_t,s32);      /* ZOID_SetTexture */
extern void ZOID_SetColor(s16,s32 *);             /* ZOID_SetColor */
extern void ScaleUV(f32 [3][3],f32 [3][3],f32);  /* ScaleUV */
extern void WPitchUV(f32,f32 [][3]);         /* WPitchUV */
extern void WRollUV(f32,f32 [][3]);         /* WRollUV */
extern void ZeroCameras(void);          /* ZeroCameras */
extern void MBOX_HideObject(s32,s32,s32);         /* MBOX_HideObject */
extern void MBOX_ShowObject(s32,s32,s32);   /* MBOX_ShowObject */

/* unit functions */
void trackscr_InitScreen(void);
void trackscr_UninitScreen(void);
void showTrackSelect(void);
s32 TrackValid(s32);
s32 OptionValid(s32);
void SetFogIntensity(s32);
void unloadTrackScreen(void);
void buildTrackMap(void);
void clearTrackMap(void);
void trackscr_animTrackMap(void);
void buildTrackMaps(void);
void animTrackMaps(void);
void trackscr_initButtons(void);
void trackscr_updateButtons(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void clearTrackMaps(void); void trackscr_uninitButtons(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);
void BlowFog(void); void setCamera(void); void loadTrackScreen(void);
void buildRaceTrackMaps(void); void buildStuntTrackMaps(void); void buildBattleTrackMaps(void); void buildObstacleTrackMaps(void);
void loadPathData(void); void unloadPathData(void);

/* SDK GBI subset (as) */
typedef struct { u32 w0; uintptr_t w1; } Gwords;
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
typedef struct { s16 ob[3]; u16 flag; s16 tc[2]; u8 cn[4]; } Vtx;

/* ---- context (region): OptionValid, TrackValid, SetFogIntensity, unloadTrackScreen, clearTrackMap ---- */
extern s32 trackscr_validOption[][21];   /* validOption */
extern s8 trackReady[19];      /* TrackValid static trackReady */
extern s32 ghosts_enabled;
/* SetFogIntensity: research body */
/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
struct Blit {const char *Name;void *Image;void *Info;u16 TexIndex;s16 X,Y;u16 Z;s16 Width,Height;u8 Alpha,Flip;s8 Hide,Init;
    s16 Top,Bot,Left,Right,color;u16 reserved;void *AnimFunc;u32 AnimID;intptr_t AnimDTA;u16 resource;};
typedef struct RGB { u8 r, g, b; } RGB;
extern s8 gFogColor;
extern s8 gFogIntensity;
extern RGB gFogColors[];
extern u8 trackscr_fogColor[8];
extern u8 n64FogPal[];
/* PC: the fog blit's palette n64FogPal is a Palette (gfx's draw reads its colour table at +0x14 as a u32 window
 * address): that word is this one, which must stay in the arena at its N64 address. It holds the colour pair
 * trackscr_fogColor's address, a window address (FOG_TABLE). As a host pointer it had storage of its own, and the draw read
 * a table of 0 (docs/port/BRINGUP.md 7). */
extern u32 n64FogPal_palBase;
#define FOG_TABLE(p) PC_N64W(p)
extern Blit *trackScreenPoly,*trackFogPoly;
extern void UpdateBlit(Blit *);
/* BlowFog (inlined into _InitScreen); MAT3 static fogTileSize (anon_uls_ult_8) */
typedef struct TileSize {u16 uls,ult,lrs,lrt;} TileSize;
extern volatile s16 gRenderInProgress;   /* gRenderInProgress */
extern s32 trackMapObj;            /* trackMapObj */
extern void RemoveBlit(Blit *);   /* RemoveBlit (ZOID_RemovePoly on N64: blits) */
extern void ZOID_RemoveObject(s16);  /* ZOID_RemoveObject */
extern void removeBG(void);  /* removeBG */
extern void ZOID_Reset(void);  /* ZOID_Reset */
/* ---- context (region): _UninitScreen, buildTrackMap, animTrackMap: option_parent research bodies ---- */
/* PC: the loaded WAD's OBHD records keep their N64 layout (port/assets/wad.c), so a LOD's display list and
 * vertices are u32 RDRAM-window addresses here too (docs/port/LOADERS.md). */
typedef struct MapLod {u16 texidx,flags;f32 lodRad;u32 displayList;u32 verts;} MapLod;
#define MAPLOD_PTR(x) (uintptr_t) x
typedef struct RomObjDef {char name[16];f32 radius;s16 unused,lodCount;MapLod lod[4];} RomObjDef;   /* 88 B */
typedef struct ModelBank {RomObjDef *romObjDefs;s32 count;} ModelBank;
typedef struct TrackMapEntry {s32 handle;f32 offset_x,offset_y,angle,scale_from,scale_to;f32 matrix[3][3],position[3];} TrackMapEntry;   /* trackMap: anon_objIdx_cityX_48 */
extern TrackMapEntry trackMap[20];
extern s8 trackMapsBuilt;            /* trackMapsBuilt */
extern MapLod saved_map_lods[7];   /* original lod[0] of the seven map objects */
extern MapLod mapObj[7];   /* mapObj */
extern void *map_display_list_blocks[7],*map_vertex_blocks[7];
extern ModelBank gRomObjDefPtrs[];   /* gRomObjDefPtrs */
extern char *trackMapObjNames[];       /* trackMapObjNames */
/* DeallocateMemory's definition (DeallocateMemory) takes the block's 32-bit N64 address: N64A0(p) is a window
 * pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
extern void DeallocateMemory(u32);
#define N64A0(x) PC_N64A(x)
/* PC: the sprite's host layout (particle_knot's Sprite), as every view has it (docs/port/POINTERS.md 9). */
typedef struct ImageMeta {void *pal,*tex;u16 model;s16 x,y;u16 mode;s16 w,h;u8 color;u8 flags;s8 hidden;u8 pad17;s16 t0,s0,t1,s1;} ImageMeta;
typedef struct DisplayCommand {u32 w0,w1;} DisplayCommand;
extern ImageMeta gBlitInstanceList[];
extern char *trackScreenNames[];
extern char track_fog_texture_name[];
extern Color loadTrackScreen_backColor;
extern s32 kNumMapSects;          /* kNumMapSects */
extern u32 mapObj_selDisplayList;             /* mapObj[6].displayList (MapLod) */
/* PC: mapObj[6].displayList under one name, as for gOptions_fog_intensity: clearTrackMaps and buildRaceTrackMaps write it
 * through mapObj and buildTrackMap reads it, and the three may be inlined into updateTrackScreen. */
#define mapObj_selDisplayList (mapObj[6].displayList)
/* PC: track-map display lists in host Gfx, 16 bytes a command: 5 per map section plus 7. */
#define MAP_DL_SECTION_BYTES 80
#define MAP_DL_FIXED_BYTES 112
extern f32 trackMap_selMat[12],trackMapMat[12],buildTrackMap_mapPos[3];
extern Blit *NewBlit(char *,s16,s16,s16);
#define DISPLAY_WORDS(p,a,b) do {DisplayCommand *command=(p)++; command->w0=(a);command->w1=(b);} while(0)
#define DISPLAY_TEXTURE(p,on) DISPLAY_WORDS(p,0xD7000000u|((on)<<1),0xFFFFFFFFu)
#define DISPLAY_VERTEX(p,v,n,first) DISPLAY_WORDS(p,0x01000000u|((n)<<12)|(((first)+(n))<<1),(u32)(v))
#define DISPLAY_TRIANGLES(p,a,b,c,d,e,f) DISPLAY_WORDS(p,0x06000000u|((a)*2<<16)|((b)*2<<8)|((c)*2),((d)*2<<16)|((e)*2<<8)|((f)*2))
extern f32 trackscr_animTrackMap_rotAngle,trackscr_animTrackMap_cityPos[3],trackscr_animTrackMap_mapPos[3];
extern void ScaleUVXYZ(f32 (*)[3],f32 (*)[3],f32,f32,f32);   /* ScaleUVXYZ */
extern void WYawUV(f32,f32 (*)[3]);   /* WYawUV */
/* ---- context (region): showTrackSelect stand-in (MAT3 port), animTrackMaps stand-in ---- */
extern s8 gBackwards;      /* gBackwards */
extern s8 gMirrorWaves;      /* gMirrorWaves */
extern f32 trackscr_showTrackSelect_angle;     /* showTrackSelect static angle */
extern f32 trackscr_showTrackSelect_fogPos[3];  /* showTrackSelect static fogPos */
extern void CopyUV(f32 *,f32 (*)[3]);          /* CopyUV */
extern void YawUV(f32,f32 (*)[3]);           /* YawUV */
extern f32 trackscr_animTrackMaps_mapPos[3];   /* animTrackMaps static mapPos */
/* ---- buildTrackMaps (MAT3 trackscr.c, GC 0x8008ad10) and its inlined statics ---- */
typedef struct Vtx_t {s16 ob[3];u16 flag;s16 tc[2];u8 cn[4];} Vtx_t;
typedef struct ResPath {u16 numPts;s16 (*pts)[3];} ResPath;
typedef struct Checkpoints {s16 startTime,loopPt,finishPt,startPt,numPoints;} Checkpoints;
extern s16 gPathMax[3];          /* gPathMax */
extern s16 gPathMin[3];          /* gPathMin */
extern ResPath gResPath;         /* gResPath */
extern Checkpoints gCheckpoints;     /* gCheckpoints */
extern void *gPathWave;           /* gPathWave */
extern s32 pathWaveIdx;             /* pathWaveIdx */
extern Color race_map_vertex_color;           /* buildRaceTrackMaps static map vertex colour {63,63,63,255} */
extern f32 buildRaceTrackMaps_mapPos[3],buildRaceTrackMaps_selMapPos[3];  /* buildRaceTrackMaps static mapPos (x2) */
extern f32 buildStuntTrackMaps_mapPos[3];          /* buildStuntTrackMaps static mapPos */
extern f32 buildBattleTrackMaps_mapPos[3];          /* buildBattleTrackMaps static mapPos */
extern s32 LoadWave(s32,s32,s32,s32,intptr_t);  /* LoadWave */
extern void *GetWaveInstanceAddr(s32);  /* GetWaveInstanceAddr */
extern void ReadCheckpointData(void);   /* ReadCheckpointData */
extern void ReadPathData(void);  /* ReadPathData */
extern void UnloadWaveInstance(s32);    /* UnloadWaveInstance */
extern void *audio_dma_sync(s32,s32);  /* AllocateMemoryHeap */
extern s32 FindResPathIntersect(s16);     /* FindResPathIntersect */
extern f32 sqrtf(f32);
#pragma intrinsic (sqrtf)
#define GFX(p,a,b) do { Gfx *_g = (p)++; _g->words.w0 = (a); _g->words.w1 = (b); } while (0)

/* updateTrackScreen (MAT3 trackscr.c, GC 0x80087abc). N64 differences read from the retail words: the
 * _InitScreen/showTrackSelect calls come before the player loop, which tests a removed controller pak
 * instead of walking gPlayerFiles; A/B start needs expansion_pak_present or a track other than 5 (the same gate guards
 * the up/down option moves); effect ids 38/40/39; case 0 sets buttonsJustInited when !expansion_pak_present;
 * options are saved for player 0 after SetOptions; ZeroCamera/UpdateCameras = inlined setCamera. */
void updateTrackScreen(void)
{
    s32 i;
    s32 j;
    s32 add;
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) { setGameState(0x100); return; }
        gSkipToGameState = 1;
    }
    if (!trackscr_inited) trackscr_InitScreen();
    showTrackSelect();
    for (i = 0; i < gNumPlayers; i++) {
        if ((*gPlayers[i].save)->file && PS_PakChanged((*(*gPlayers[i].save)->file)->controller)) {
            trackscr_UninitScreen(); setGameState(0x20); return;
        }
    }
    if (gSoftReset) { trackscr_UninitScreen(); setGameState(4); return; }
    if ((gActiveEdges & 3) && (expansion_pak_present || gTrackNum != 5)) {
        PlayStartABSound(gActiveEdges); setGameState(0x100); trackscr_UninitScreen();
        for (j = 0; j < gNumPlayers; j++)
            for (i = 21; i < 41; i++) audio_bus_mix(gPlayers[j].save, i, gOptions[i]);
        return;
    }
    if (gActiveEdges & 4) {
        AudPlayEffect(38, 0, 1, 0); trackscr_UninitScreen();
        if (gRace == 3) setGameState(0x4000);
        else setGameState(0x40);
        return;
    }
    if (gActiveLevels & 0x3080) {
        add = (gActiveAutoEdges & 0x1000) ? -1 : (gActiveAutoEdges & 0x2000) ? 1 : 0;
        if ((gRace == 3 && (gCircuitTrack > 0 || trackscr_gOpts[trackscr_gActiveOption] != 20)) || (add == 0 && !trackscr_gActiveZ) || (gRace == 2 && trackscr_gOpts[trackscr_gActiveOption] == 2)) goto controls_done;
        PlayLRSound(gActiveAutoEdges);
        if (gMirrorControls > 1) add = -add;
        switch (trackscr_gOpts[trackscr_gActiveOption]) {
        case 0:
            if (add) {
                gTrack += add;
                if (gTrack < 0) gTrack = gNumTracks - 1;
                else if (gTrack >= gNumTracks) gTrack = 0;
                do {
                    gTrackNum += add;
                    if (gTrackNum < 0) gTrackNum = 18;
                    else if (gTrackNum >= 19) gTrackNum = 0;
                } while (!TrackValid(gTrackNum));
                for (i = 21; i < 41; i++)
                    gOptions[i] = GetPlayerOption(gPlayers[0].save, i);
                if (!expansion_pak_present) trackscr_buttonsJustInited = 1;
            }
            break;
        case 1:
            if (add) {
                gOptions[25] += add;
                if (gOptions[25] < 0) gOptions[25] = 1;
                else if (gOptions[25] > 1) gOptions[25] = 0;
            } else gOptions[25] = 0;
            break;
        case 2:
            if (add) {
                gOptions[21] += add;
                if (gOptions[21] < 1) gOptions[21] = 8;
                else if (gOptions[21] > 8) gOptions[21] = 1;
            } else gOptions[21] = 3;
            break;
        case 3:
            if (add) {
                gOptions[32] += add;
                if (gOptions[32] < 0) gOptions[32] = 2;
                else if (gOptions[32] > 2) gOptions[32] = 0;
            } else gOptions[32] = 0;
            break;
        case 4:
            if (add) {
                if (!gOptions[33]) gOptions[33] = 1;
                else gOptions[33] = 0;
            } else gOptions[33] = 0;
            break;
        case 5:
            if (add) {
                gOptions[34] += add;
                if (gOptions[34] <= 0) gOptions[34] = 100;
                else if (gOptions[34] > 100) gOptions[34] = 1;
            } else gOptions[34] = 69;
            break;
        case 6:
            if (add) {
                gOptions[35] += add;
                if (gOptions[35] <= 0) gOptions[35] = 20;
                else if (gOptions[35] > 20) gOptions[35] = 1;
            } else gOptions[35] = 5;
            break;
        case 7:
            if (add) {
                gOptions[36] += add;
                if (gOptions[36] <= 0) gOptions[36] = 50;
                else if (gOptions[36] > 50) gOptions[36] = 1;
            } else gOptions[36] = 30;
            break;
        case 8:
            if (add) {
                if (!gOptions[37]) gOptions[37] = 1;
                else gOptions[37] = 0;
            } else gOptions[37] = 0;
            break;
        case 9:
            if (add) {
                gOptions[38] += 5*add;
                if (gOptions[38] < 5) gOptions[38] = 50;
                else if (gOptions[38] > 50) gOptions[38] = 5;
            } else gOptions[38] = 10;
            break;
        case 10:
            if (add) {
                gOptions[39] += add;
                if (gOptions[39] <= 0) gOptions[39] = 20;
                else if (gOptions[39] > 20) gOptions[39] = 1;
            } else gOptions[39] = 8;
            break;
        case 11:
            if (add) {
                gOptions[40] ^= 1;
            } else gOptions[40] = 0;
            break;
        case 12:
            if (add) {
                gOptions[27] ^= 1;
            } else gOptions[27] = 0;
            clearTrackMap(); buildTrackMap(); pathPos = 1.0f - pathPos;
            break;
        case 13:
            if (add) {
                if ((gPlayers[0].levels & 0x3c000) == 0x3c000) gOptions[28] = 2;
                else if (!gOptions[28]) gOptions[28] = 1;
                else gOptions[28] = 0;
            } else gOptions[28] = 0;
            clearTrackMap(); buildTrackMap();
            break;
        case 14:
            if (add) {
                gOptions[29] += add;
                if (gOptions[29] < 0) gOptions_fog_intensity = 3;
                else if (gOptions[29] > 3) gOptions[29] = 0;
            } else gOptions[29] = 0;
            break;
        case 15:
            if (add) {
                gOptions[30] += add;
                if (gOptions[30] < 0) gOptions[30] = 4;
                else if (gOptions[30] > 4) gOptions[30] = 0;
            } else gOptions[30] = 0;
            break;
        case 16:
            if (add) {
                gOptions[23] += add;
                if (gOptions[23] < 0) gOptions[23] = 3;
                else if (gOptions[23] > 3) gOptions[23] = 0;
            } else gOptions[23] = 1;
            break;
        case 17:
            if (add) {
                gOptions[22] += add;
                if (gOptions[22] > PC_DRONES_MAX) gOptions[22] = 0;
                else if (gOptions[22] < 0) gOptions[22] = PC_DRONES_MAX;
            } else gOptions[22] = 5;
            break;
        case 18:
            if (add) {
                gOptions[24] += add;
                if (gOptions[24] < 0) gOptions[24] = 5;
                else if (gOptions[24] > 5) gOptions[24] = 0;
            } else gOptions[24] = 2;
            break;
        case 19:
            if (add) {
                gOptions[26] += add;
                if (gOptions[26] < 0) gOptions[26] = 2;
                else if (gOptions[26] > 2) gOptions[26] = 0;
            } else gOptions[26] = 2;
            break;
        case 20:
            if (add) {
                gOptions[31] ^= 1;
            } else gOptions[31] = 0;
            if (gRace == 3) (*(*gPlayers[gCircuitPlayer].save)->stats)[1785] = gOptions[31];
            break;
        }
        SetOptions();
        for (i = 21; i < 41; i++) audio_bus_mix(gPlayers[0].save, i, gOptions[i]);
    } else if (gActiveAutoEdges & 0x400) {
        if (expansion_pak_present || gTrackNum != 5) {
            AudPlayEffect(40, 0, 1, 0);
            if (--trackscr_gActiveOption < 0) trackscr_gActiveOption = trackscr_gNumOptions - 1;
        }
    } else if (gActiveAutoEdges & 0x800) {
        if (expansion_pak_present || gTrackNum != 5) {
            AudPlayEffect(39, 0, 1, 0);
            trackscr_gActiveOption++;
            if (trackscr_gActiveOption > trackscr_gNumOptions - 1) trackscr_gActiveOption = 0;
        }
    }
    if (trackscr_gFirstOption > trackscr_gActiveOption - 1) trackscr_gFirstOption = trackscr_gActiveOption - 1;
    if (trackscr_gFirstOption < trackscr_gActiveOption - 2) trackscr_gFirstOption = trackscr_gActiveOption - 2;
    if (trackscr_gFirstOption > trackscr_gNumOptions - 4) trackscr_gFirstOption = trackscr_gNumOptions - 4;
    if (trackscr_gFirstOption < 0) trackscr_gFirstOption = 0;
controls_done:
    trackscr_gActiveOptionEnabled = gRace != 3 || (gCircuitTrack == 0 && trackscr_gOpts[trackscr_gActiveOption] == 20);
    if (gTrackNum != oldTrack) {
        desiredTrackMap = gTrackNum;
        oldTrack = gTrackNum;
        for (i = 0; i < gNumPlayers; i++) {
            switch (gRace) {
            case 0: case 1: case 2:
                if (gTrackNum >= 0 && gTrackNum <= 5) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum + 0);
                break;
            case 4:
                if (gTrackNum >= 14 && gTrackNum <= 17) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - 14);
                break;
            case 6:
                if (gTrackNum >= 6 && gTrackNum <= 13) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - 6);
                break;
            }
        }
    }
    trackscr_animTrackMap();
    animTrackMaps();
    SetFogIntensity(0);
    setCamera();
}

/* _InitScreen (MAT3 trackscr.c, GC 0x800887ac). N64: 17-entry multiblit; BlowFog and setCamera inlined. */
void trackscr_InitScreen(void)
{
    s32 i;
    s32 j;
    f32 pos;
    trackscr_gActiveOption = 0;
    trackscr_gFirstOption = 0;
    trackscr_gActiveZ = 0;
    currentTrackMap = -1;
    trackscr_gNumOptions = 0; for (i = 0; i < 21; i++) if (OptionValid(i)) trackscr_gOpts[trackscr_gNumOptions++] = i;
    buildTrackMaps();
    if (gRace == 3) oldTrack = gTrackNum = gCircuitTracks[gCircuitTrack].trackNum;
    else {
        if (gRace == 4) oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + 14;
        else if (gRace == 5) oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + 18;
        else if (gRace == 6) oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + 6;
        else oldTrack = gTrackNum = GetPlayerTrack(gPlayers[0].save, gRace) + 0;
        if (!TrackValid(gTrackNum)) {
            if (gRace == 4) oldTrack = gTrackNum = 14;
            else if (gRace == 5) oldTrack = gTrackNum = 18;
            else if (gRace == 6) oldTrack = gTrackNum = 6;
            else oldTrack = gTrackNum = 0;
        }
        for (i = 0; i < gNumPlayers; i++) {
            if (gRace == 4) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - 14);
            else if (gRace == 5) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - 18);
            else if (gRace == 6) SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum - 6);
            else SavePlayerTrack(gPlayers[i].save, gRace, gTrackNum);
        }
        for (i = 21; i < 41; i++)
            gOptions[i] = GetPlayerOption(gPlayers[0].save, i);
    }
    SetOptions();
    particle_lifetime_set();
    trackscr_initButtons();
    trackscr_blit = NewMultiBlit(0, 0, trackscr_mblit, 17);
    i = 0;
    for (j = 0; j < 19; j++) {
        if (TrackValid(j)) {
            if (j == gTrackNum) { gTrack = i; break; }
            i++;
        }
    }
    gNumTracks = 19;
    i = 0;
    pos = 0.0f;
    for (j = 0; j < 19; j++) {
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

/* body, merged 2026-10-21.
 * : MAT3 _UninitScreen calls uninitButtons and clearTrackMaps; both are inlined statics whose deleted
 * procedures are the retail stubs trackscr_uninitButtons (uninitButtons) and clearTrackMaps (clearTrackMaps), emitted in the
 * post-order of the call graph (clearTrackMaps is visited first: last call first). Bodies moved verbatim from here. */
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

/* Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
typedef struct UVMatrix { f32 uvs[3][3]; f32 pos[3]; } UVMatrix;   /* tUVMatrix */
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
    for (trackscr_gNumOptions = 0, i = 0; i < 21; i++) if (OptionValid(i)) trackscr_gOpts[trackscr_gNumOptions++] = i;
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

/* TrackValid: MAT3 trackscr.c TrackValid (GC 0x8008a248) direct port; IDO rewrites the range tests as pointer compares on &trackReady[track]. */
s32 TrackValid(s32 track)
{
    if (!trackReady[track]) return 0;
    if (!IsTrackUnlocked(track)) return 0;
    if (gRace == 4) return track >= 14 && track <= 17;
    if (gRace == 5) return track >= 18 && track <= 18;
    if (gRace == 6) return track >= 6 && track <= 13;
    return track >= 0 && track <= 5;
}

/* OptionValid: MAT3 trackscr.c OptionValid (GC 0x8008a324) direct port; N64 adds the track-5 and ghosts_enabled tests. */
s32 OptionValid(s32 opt)
{
    s32 valid;
    valid = trackscr_validOption[gRace][opt];
    if (gRace == 0 && opt == 17) valid = gNumPlayers < 3;
    if (gRace == 4) {
        if (opt == 5) valid = gOptions[32] == 1;
        if (opt == 6) valid = gOptions[32] == 0;
        if (opt == 7) valid = gOptions[32] == 2;
    }
    if (gRace == 6) {
        if (opt == 9) valid = gOptions[37] == 0;
        if (opt == 11) valid = gOptions[37] == 0;
        if (opt == 10) valid = gOptions[37] == 1;
    }
    if (opt == 19 && gNumPlayers == 1) valid = 0;
    if (opt == 12) valid = gTrackNum >= 0 && gTrackNum <= 5;
    if (!expansion_pak_present && gTrackNum == 5 && opt != 0) valid = 0;
    if (!ghosts_enabled && opt == 1) valid = 0;
    return valid;
}

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
        n64FogPal_palBase = FOG_TABLE(trackscr_fogColor);
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

/* setCamera (inlined into _InitScreen and updateTrackScreen) */
void setCamera(void)
{
    ZeroCamera(1);
    UpdateCameras();
}

/* body, merged 2026-10-21 */
/* loadTrackScreen (MAT3 trackscr.c, GC 0x8008a634): inlined into buildTrackMap (deleted stub 0x8038AEA8).
 * The GC poly locals stay declared (shared GameSource declarations); the N64 body makes blits. */
/* Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
void loadTrackScreen(void)
{
    f32 polyVerts[4][3];
    u16 texIdx;
    Color backColor = loadTrackScreen_backColor;
    unloadTrackScreen();
    if (!((gTrackNum >= 0 && gTrackNum <= 5) ||
        (gTrackNum >= 6 && gTrackNum <= 13) ||
        (gTrackNum >= 14 && gTrackNum <= 17) ||
        (gTrackNum >= 18 && gTrackNum <= 18))) return;
    trackScreenPoly = NewBlit(trackScreenNames[gTrackNum],176,32,0);
    trackScreenPoly->Z = 0x7F00;
    UpdateBlit(trackScreenPoly);
    gBlitInstanceList[trackScreenPoly->resource].flags |= 1;
    if (OptionValid(14) == 1) {
        trackFogPoly = NewBlit(track_fog_texture_name,176,32,0);
        trackFogPoly->Z = 0x7E00;
        UpdateBlit(trackFogPoly);
    }
}

/* body, merged 2026-10-21 */
/* unloadTrackScreen: option_parent research body */
void unloadTrackScreen(void)
{
    Blit *b;
    if ((b = trackScreenPoly)) { RemoveBlit(b); trackScreenPoly = 0; }
    if ((b = trackFogPoly)) { RemoveBlit(b); trackFogPoly = 0; }
}

/* body, merged 2026-10-21 */
/* Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
void buildTrackMap(void)
{
    Gfx *gfx;
    Vtx *vtx;
    s32 i, vertex;
    u32 vtxSize, dlSize;
    s32 trackMBIdx;
    loadTrackScreen();
    if (desiredTrackMap >= 0 && desiredTrackMap <= 5) {
        if (trackMap[19].handle != -1) {
            vtxSize = (kNumMapSects + 1) * 4 * sizeof(Vtx);
            memcpy(map_vertex_blocks[6], map_vertex_blocks[desiredTrackMap], vtxSize);
            dlSize = (kNumMapSects * 5 + 7) * sizeof(Gfx);
            memcpy(map_display_list_blocks[6], map_display_list_blocks[desiredTrackMap], dlSize);
            gfx = mapObj_selDisplayList;
            if ((gRace == 3 && gCircuitTracks[gCircuitTrack].mirror == 1)
                || (gRace != 3 && gOptions[28])) {
                gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 0, 0);
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, 0xC8112230, 0);
                gDPSetCombine(gfx++, 0xFFFFFF, 0xFFFE7C38);
                for (i = 0, vertex = 0; i < kNumMapSects; i++, vertex += 4) {
                    if (i + 1 == kNumMapSects) {
                        if (vertex) {}
                        gSPVertex(gfx++, &((Vtx *)map_vertex_blocks[6])[vertex], 4, 0);
                        gSPVertex(gfx++, &((Vtx *)map_vertex_blocks[6])[0], 4, 4);
                    } else {
                        gSPVertex(gfx++, &((Vtx *)map_vertex_blocks[6])[vertex], 8, 0);
                    }
                    gSP2Triangles(gfx++, 1, 5, 0, 0, 4, 0, 5, 0);
                    gSP2Triangles(gfx++, 0, 4, 3, 0, 7, 3, 4, 0);
                    gSP2Triangles(gfx++, 3, 7, 2, 0, 6, 2, 7, 0);
                    gSP2Triangles(gfx++, 2, 6, 1, 0, 5, 1, 6, 0);
                }
                gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, 0, 1);
                gSPEndDisplayList(gfx++);
            }
            ScaleUV((f32 (*)[3])unitmatrix, (f32 (*)[3])trackMap_selMat, 2.0f);
            WPitchUV(-0.785398185f, (f32 (*)[3])trackMap_selMat);
            trackMap[19].position[0] = buildTrackMap_mapPos[0];
            trackMap[19].position[1] = buildTrackMap_mapPos[1];
            trackMap[19].position[2] = buildTrackMap_mapPos[2];
        }
    } else if ((desiredTrackMap >= 6 && desiredTrackMap <= 13) || (desiredTrackMap >= 14 && desiredTrackMap <= 17)) {
        if (trackMapObj == -1) {
            trackMapObj = MBOX_NewObject(MBOX_FindObject_WORK(trackMapObjNames[desiredTrackMap], 0, gNumModels - 1, 1), (uintptr_t)trackMapMat, -1, 0);
            ScaleUV((f32 (*)[3])unitmatrix, (f32 (*)[3])trackMapMat, 2.0f);
            WPitchUV(-0.785398185f, (f32 (*)[3])trackMapMat);
            trackMapMat[9] = buildTrackMap_mapPos[0];
            trackMapMat[10] = buildTrackMap_mapPos[1];
            trackMapMat[11] = buildTrackMap_mapPos[2];
        }
    }
    currentTrackMap = desiredTrackMap;
}

/* body, merged 2026-10-21 */
/* clearTrackMap: option_parent research body */
void clearTrackMap(void)
{
    unloadTrackScreen();
    if (trackMapObj != -1) { ZOID_RemoveObject((s16)trackMapObj); trackMapObj = -1; }
    while (gRenderInProgress) { pc_os_spin_yield("clearTrackMap"); }
}

/* Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
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
    trackMap[19].position[0] = cityPos[0];
    trackMap[19].position[1] = cityPos[1];
    trackMap[19].position[2] = cityPos[2];
    trackMap[19].position[0] += trackMap[currentTrackMap].offset_x;
    trackMap[19].position[1] += trackMap[currentTrackMap].offset_y;
    trackMap[19].position[0] *= 1.0f - mapTransition;
    trackMap[19].position[1] *= 1.0f - mapTransition;
    trackMap[19].position[2] *= 1.0f - mapTransition;
    trackMap[19].position[0] += mapPos[0] * mapTransition;
    trackMap[19].position[1] += mapPos[1] * mapTransition;
    trackMap[19].position[2] += mapPos[2] * mapTransition;
    trackMapMat[9] = trackMap[19].position[0];
    trackMapMat[10] = trackMap[19].position[1];
    trackMapMat[11] = trackMap[19].position[2];
    ScaleUV((f32 (*)[3])unitmatrix, (f32 (*)[3])trackMapMat,
        mapTransition * trackMap[currentTrackMap].scale_to + trackMap[currentTrackMap].scale_from * (1.0f - mapTransition));
    ScaleUV((f32 (*)[3])unitmatrix, (f32 (*)[3])trackMap_selMat,
        mapTransition * trackMap[currentTrackMap].scale_to + trackMap[currentTrackMap].scale_from * (1.0f - mapTransition));
    if ((gRace == 3 && gCircuitTracks[gCircuitTrack].mirror == 1)
        || (gRace != 3 && gRace != 4 && gRace != 6 && gRace != 5 && gOptions[28])) {
        ScaleUVXYZ((f32 (*)[3])trackMapMat, (f32 (*)[3])trackMapMat, -1.0f, 1.0f, 1.0f);
        ScaleUVXYZ((f32 (*)[3])trackMap_selMat, (f32 (*)[3])trackMap_selMat, -1.0f, 1.0f, 1.0f);
    }
    WYawUV(rotAngle * mapTransition + trackMap[currentTrackMap].angle * (1.0f - mapTransition), (f32 (*)[3])trackMapMat);
    WPitchUV(-0.5235987901687622f * mapTransition + (1.0f - mapTransition) * -1.5707963705062866f, (f32 (*)[3])trackMapMat);
    WYawUV(rotAngle * mapTransition + trackMap[currentTrackMap].angle * (1.0f - mapTransition), (f32 (*)[3])trackMap_selMat);
    WPitchUV(-0.5235987901687622f * mapTransition + (1.0f - mapTransition) * -1.5707963705062866f, (f32 (*)[3])trackMap_selMat);
}

void buildTrackMaps(void)
{
    if (gRace == 4) buildStuntTrackMaps();
    else if (gRace == 6) buildBattleTrackMaps();
    else if (gRace == 5) buildObstacleTrackMaps();
    else buildRaceTrackMaps();
    trackMapsBuilt = 1;
    pathPos = 0.0f;
}

/* buildRaceTrackMaps (inlined). N64: the GC PS2 packet code is an N64 display list per map: kNumMapSects
 * sections of 4 vertices (8 for the last, which wraps to the first) and four G_TRI2 commands each. */
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
    f32 u0;
    f32 xoffset, yoffset, zoffset;
    s32 u1, u2;
    s32 trackMBIdx;
    RomObjDef *trackRomObjDef;
    Gfx *dl;
    Vtx_t *vtx;
    s32 vtxSize, dlSize;
    s32 i;
    s32 u3, u4, u5, u6, u7;
    f32 sectVerts[8][3];
    saveTrackNum = gTrackNum;
    for (gTrackNum = 0; gTrackNum <= 5; gTrackNum++) {
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
                ScaleUV((f32 (*)[3])unitmatrix, trackMap[gTrackNum].matrix, 1.0f);
                WPitchUV(-1.5707963705062866f, trackMap[gTrackNum].matrix);
                trackMap[gTrackNum].position[0] = buildRaceTrackMaps_mapPos[0];
                trackMap[gTrackNum].position[1] = buildRaceTrackMaps_mapPos[1];
                trackMap[gTrackNum].position[2] = buildRaceTrackMaps_mapPos[2];
            }
            unloadPathData();
        }
    }
    gTrackNum = saveTrackNum;
    trackMBIdx = MBOX_FindObject_WORK(trackMapObjNames[19], 0, gNumModels - 1, 1);
    trackRomObjDef = &gRomObjDefPtrs[trackMBIdx >> 10].romObjDefs[trackMBIdx & 1023];
    saved_map_lods[6] = trackRomObjDef->lod[0];
    map_vertex_blocks[6] = audio_dma_sync(0, kNumMapSects * 64 + 64 + 7);
    mapObj[6].verts = (void *)(((uintptr_t)map_vertex_blocks[6] + 7) & ~7);
    map_display_list_blocks[6] = audio_dma_sync(0, kNumMapSects * MAP_DL_SECTION_BYTES + MAP_DL_FIXED_BYTES + 7);
    mapObj[6].displayList = (u32 *)(((uintptr_t)map_display_list_blocks[6] + 7) & ~7);
    mapObj[6].flags = 0;
    mapObj[6].texidx = 0;
    mapObj[6].lodRad = 0.0f;
    trackRomObjDef->lod[0] = mapObj[6];
    if (trackMap[19].handle == -1) {
        trackMap[19].handle = MBOX_NewObject(trackMBIdx, (uintptr_t)trackMap[19].matrix, -1, 0);
        ScaleUV((f32 (*)[3])unitmatrix, trackMap[19].matrix, 1.0f);
        WPitchUV(-1.5707963705062866f, trackMap[19].matrix);
        trackMap[19].position[0] = buildRaceTrackMaps_selMapPos[0];
        trackMap[19].position[1] = buildRaceTrackMaps_selMapPos[1];
        trackMap[19].position[2] = buildRaceTrackMaps_selMapPos[2];
    }
}

/* buildStuntTrackMaps (inlined) */
void buildStuntTrackMaps(void)
{
    s32 saveTrackNum;
    saveTrackNum = gTrackNum;
    for (gTrackNum = 14; gTrackNum <= 17; gTrackNum++) {
        if (!IsTrackUnlocked(gTrackNum)) continue;
        if (trackMap[gTrackNum].handle == -1) {
            trackMap[gTrackNum].handle = MBOX_NewObject(MBOX_FindObject_WORK(trackMapObjNames[gTrackNum], 0, gNumModels - 1, 1), (uintptr_t)trackMap[gTrackNum].matrix, -1, 0);
            ScaleUV((f32 (*)[3])unitmatrix, trackMap[gTrackNum].matrix, 1.0f);
            WPitchUV(-1.5707963705062866f, trackMap[gTrackNum].matrix);
            trackMap[gTrackNum].position[0] = buildStuntTrackMaps_mapPos[0];
            trackMap[gTrackNum].position[1] = buildStuntTrackMaps_mapPos[1];
            trackMap[gTrackNum].position[2] = buildStuntTrackMaps_mapPos[2];
        }
    }
    gTrackNum = saveTrackNum;
}

/* buildBattleTrackMaps (inlined) */
void buildBattleTrackMaps(void)
{
    s32 saveTrackNum;
    saveTrackNum = gTrackNum;
    for (gTrackNum = 6; gTrackNum <= 13; gTrackNum++) {
        if (!IsTrackUnlocked(gTrackNum)) continue;
        if (trackMap[gTrackNum].handle == -1) {
            trackMap[gTrackNum].handle = MBOX_NewObject(MBOX_FindObject_WORK(trackMapObjNames[gTrackNum], 0, gNumModels - 1, 1), (uintptr_t)trackMap[gTrackNum].matrix, -1, 0);
            ScaleUV((f32 (*)[3])unitmatrix, trackMap[gTrackNum].matrix, 1.0f);
            WPitchUV(-1.5707963705062866f, trackMap[gTrackNum].matrix);
            trackMap[gTrackNum].position[0] = buildBattleTrackMaps_mapPos[0];
            trackMap[gTrackNum].position[1] = buildBattleTrackMaps_mapPos[1];
            trackMap[gTrackNum].position[2] = buildBattleTrackMaps_mapPos[2];
        }
    }
    gTrackNum = saveTrackNum;
}

/* buildObstacleTrackMaps (inlined, empty) */
void buildObstacleTrackMaps(void)
{
}

/* clearTrackMaps (MAT3 trackscr.c, GC 0x8008bfac): inlined into _UninitScreen; deleted stub 0x8038B930.
 * Body moved verbatim from _UninitScreen. */
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
            model = MBOX_FindObject_WORK(trackMapObjNames[i < 6 ? i : 19],0,gNumModels - 1,1);
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

/* animTrackMaps stand-in: the GC transform loop and pathPos advance (the N64 vertex-colour fade is omitted). */
/* Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
typedef union MapColor { u8 c[4]; u32 w; } MapColor;
typedef struct MapVtx { s16 ob[3]; u16 flag; s16 tc[2]; u8 cn[4]; } MapVtx;
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
        for (trackIdx = 0; trackIdx <= 19; trackIdx++) {
            if (trackMap[trackIdx].handle != -1) {
                if (trackIdx < 19) {
                    ScaleUV((f32 (*)[3])unitmatrix, trackMap[trackIdx].matrix, trackMap[trackIdx].scale_from);
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
                mapIdx = trackIdx == 19 ? 6 : trackIdx;
                if (trackIdx < 6 || trackIdx == 19) {
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

/* loadPathData (inlined) */
void loadPathData(void)
{
    if (gTrackNum >= 0 && gTrackNum <= 5) {
        pathWaveIdx = LoadWave(gTrackNum + 158, 0, 0, 0, 0);
        gPathWave = GetWaveInstanceAddr(pathWaveIdx);
        ReadCheckpointData();
        ReadPathData();
    } else pathWaveIdx = -1;
}

/* unloadPathData (inlined) */
void unloadPathData(void)
{
    if (pathWaveIdx != -1) UnloadWaveInstance(pathWaveIdx);
    pathWaveIdx = -1;
}

void trackscr_initButtons(void)
{
    Color color = trackscr_init_button_color;
    s32 i;
    s16 texture_index;
    f32 yaw;
    for (i = 0; i < 48; i++) {
        if (trackscr_buttonInfo[i].handle == -1) {
            memcpy(trackscr_buttonInfo[i].matrix, unitmatrix, 36);
            trackscr_buttonInfo[i].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(trackscr_buttonNames[trackscr_buttonInfo[i].other], 0, gNumModels - 1, 1),
                (uintptr_t)trackscr_buttonInfo[i].matrix, -1,
                (trackscr_buttonInfo[i].other == 0 || trackscr_buttonInfo[i].other == 1 || trackscr_buttonInfo[i].other == 2) ? 0x42000 : 0);
            ZOID_SetColor((s16)trackscr_buttonInfo[i].handle, (s32 *)&color);
        }
    }
    ZOID_SetTexture((s16)trackscr_buttonInfo[42].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_title_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(trackscr_buttonInfo[42].matrix, unitmatrix, 36);
    yaw = -1.5707963705062866f;
    WPitchUV(yaw, trackscr_buttonInfo[42].matrix);
    trackscr_buttonInfo[42].position[0] = 0.0f;
    trackscr_buttonInfo[42].position[1] = 115.0f;
    trackscr_buttonInfo[42].position[2] = 200.0f;
    ZOID_SetTexture((s16)trackscr_buttonInfo[43].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_cursor_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    ZOID_SetTexture((s16)trackscr_buttonInfo[44].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_a_button_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(trackscr_buttonInfo[44].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[44].matrix);
    trackscr_buttonInfo[44].position[0] = 75.1f;
    trackscr_buttonInfo[44].position[1] = 55.2f;
    trackscr_buttonInfo[44].position[2] = 100.0f;
    ScaleUV(trackscr_buttonInfo[44].matrix, trackscr_buttonInfo[44].matrix, 0.5f);
    memcpy(trackscr_buttonInfo[45].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[45].matrix);
    WRollUV(3.1415927410125732f, trackscr_buttonInfo[45].matrix);
    trackscr_buttonInfo[45].position[0] = 87.1f;
    trackscr_buttonInfo[45].position[1] = 55.2f;
    trackscr_buttonInfo[45].position[2] = 100.0f;
    ZOID_SetTexture((s16)trackscr_buttonInfo[46].handle,
        (intptr_t)MBOX_FindTexture_WORK(trackscr_b_button_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(trackscr_buttonInfo[46].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[46].matrix);
    trackscr_buttonInfo[46].position[0] = -75.1f;
    trackscr_buttonInfo[46].position[1] = 55.20f;
    trackscr_buttonInfo[46].position[2] = 100.0f;
    ScaleUV(trackscr_buttonInfo[46].matrix, trackscr_buttonInfo[46].matrix, 0.5f);
    memcpy(trackscr_buttonInfo[47].matrix, unitmatrix, 36);
    WPitchUV(yaw, trackscr_buttonInfo[47].matrix);
    trackscr_buttonInfo[47].position[0] = -87.1f;
    trackscr_buttonInfo[47].position[1] = 55.20f;
    trackscr_buttonInfo[47].position[2] = 100.0f;
    trackscr_buttonsInited = 1;
    trackscr_buttonsJustInited = 1;
    ZeroCameras();
}

/* uninitButtons@trackscr.c (MAT3 GC 0x8008cac4): inlined into _UninitScreen; deleted stub 0x8038B938.
 * Body moved verbatim from _UninitScreen. */
void trackscr_uninitButtons(void)
{
    s32 i;
    for (i = 0; i < 48; i++) {
        if (trackscr_buttonInfo[i].handle != -1) {
            if (gGameState & 0x7C03FFFE) ZOID_RemoveObject((s16)trackscr_buttonInfo[i].handle);
            trackscr_buttonInfo[i].handle = -1;
        }
    }
    trackscr_buttonsInited = 0;
}

/* updateButtons (MAT3 trackscr.c, GC 0x8008cb54). N64: single-precision speed clamps (60.0f/10.0f),
 * WPitchUV(angle - pi/2) (GC adds pi), and the out-of-range buttons fade over one button space
 * (alpha 255 - d*255/(space*scale)) instead of GC's fixed 80. The two GC function-local statics stay
 * function-local (own .bss: scrollSpeed 0x803BA87C, cursorSpeed 0x803BA85C); as externs uopt does not keep them
 * in registers across the clamp. Literal spellings: two distinct 12.566371 rodata words (0x803B92F4/F8).
 * Frame residual, disclosed (rule): retail frame 120; without the three unused f32 locals declared
 * first the frame is 112 and every local sits 4 bytes high (prevPos sp+96 vs retail sp+92). Three 4-byte slots
 * reproduce retail exactly; same 3-slot residual as startscr updateButtons. Their original use is unknown. */
void trackscr_updateButtons(void)
{
    static f32 scrollSpeed;
    static f32 cursorSpeed;
    f32 unused0, unused1, unused2;
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
        memcpy(prevPos, trackscr_buttonInfo[43].position, 12);
        trackscr_buttonInfo[43].angle += 6.2831854820251465f * gFrameSeconds;
        if (trackscr_buttonInfo[43].angle > 6.2831854820251465f) trackscr_buttonInfo[43].angle -= 6.2831854820251465f;
        trackscr_buttonInfo[43].position[0] = trackscr_buttonX * trackscr_cursorZ / trackscr_buttonZ + -180.0f * trackscr_gButtonScale;
        trackscr_buttonInfo[43].position[1] = selectedPos * trackscr_cursorZ / trackscr_buttonZ;
        trackscr_buttonInfo[43].position[2] = trackscr_cursorZ;
        if (!trackscr_buttonsJustInited) {
            if (60.0f * fabsf(trackscr_buttonInfo[43].position[1] - prevPos[1]) / 10.0f > cursorSpeed) {
                cursorSpeed = 60.0f * fabsf(trackscr_buttonInfo[43].position[1] - prevPos[1]) / 10.0f;
                cursorSpeed = cursorSpeed < 540.0f * trackscr_gButtonScale ? cursorSpeed : 540.0f * trackscr_gButtonScale;
                cursorSpeed = cursorSpeed > 180.0f * trackscr_gButtonScale ? cursorSpeed : 180.0f * trackscr_gButtonScale;
            }
            if (trackscr_buttonInfo[43].position[1] > prevPos[1] + cursorSpeed * gFrameSeconds)
                trackscr_buttonInfo[43].position[1] = prevPos[1] + cursorSpeed * gFrameSeconds;
            else if (trackscr_buttonInfo[43].position[1] < prevPos[1] - cursorSpeed * gFrameSeconds)
                trackscr_buttonInfo[43].position[1] = prevPos[1] - cursorSpeed * gFrameSeconds;
            else cursorSpeed = 180.0f * trackscr_gButtonScale;
        }
        ScaleUV((f32 (*)[3])unitmatrix, trackscr_buttonInfo[43].matrix, trackscr_gButtonScale * 0.4f);
        WRollUV(trackscr_buttonInfo[43].angle, trackscr_buttonInfo[43].matrix);
        WPitchUV(1.5707963705062866f, trackscr_buttonInfo[43].matrix);
        buttonY = selectedPos + trackscr_gButtonScale * (trackscr_gActiveOption * trackscr_gButtonSpace);
        for (buttonIdx = 0; buttonIdx < 21; buttonIdx++) {
            if (buttonIdx == trackscr_gOpts[trackscr_gActiveOption]) {
                if (trackscr_buttonInfo[buttonIdx].angle < 3.1415927410125732f) trackscr_buttonInfo[buttonIdx].angle += 12.566370964050293f * gFrameSeconds;
                if (trackscr_buttonInfo[buttonIdx].angle > 3.1415927410125732f) trackscr_buttonInfo[buttonIdx].angle = 3.1415927410125732f;
                if (trackscr_buttonsJustInited) trackscr_buttonInfo[buttonIdx].angle = 3.1415927410125732f;
            } else {
                if (trackscr_buttonInfo[buttonIdx].angle > 0.0f) trackscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
                if (trackscr_buttonInfo[buttonIdx].angle < 0.0f) trackscr_buttonInfo[buttonIdx].angle = 0.0f;
                if (trackscr_buttonsJustInited) trackscr_buttonInfo[buttonIdx].angle = 0.0f;
            }
            if ((gRace != 3 || (gCircuitTrack == 0 && buttonIdx == 20)) && (gRace != 2 || buttonIdx != 2)) trackscr_buttonInfo[buttonIdx+21].angle = trackscr_buttonInfo[buttonIdx].angle;
            else trackscr_buttonInfo[buttonIdx+21].angle = 0.0f;
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
            trackscr_buttonInfo[buttonIdx+21].position[0] = trackscr_buttonX + 50.0f * trackscr_gButtonScale;
            trackscr_buttonInfo[buttonIdx+21].position[1] = trackscr_buttonInfo[buttonIdx].position[1];
            trackscr_buttonInfo[buttonIdx+21].position[2] = trackscr_buttonInfo[buttonIdx].position[2];
            ScaleUV((f32 (*)[3])unitmatrix, trackscr_buttonInfo[buttonIdx].matrix, trackscr_gButtonScale);
            ScaleUV((f32 (*)[3])unitmatrix, trackscr_buttonInfo[buttonIdx+21].matrix, trackscr_gButtonScale);
            WPitchUV(trackscr_buttonInfo[buttonIdx].angle - 1.5707963705062866f, trackscr_buttonInfo[buttonIdx].matrix);
            WPitchUV(trackscr_buttonInfo[buttonIdx+21].angle - 1.5707963705062866f, trackscr_buttonInfo[buttonIdx+21].matrix);
            if (trackscr_buttonInfo[buttonIdx].angle > 1.5707963705062866f) tex = MBOX_FindTexture_WORK(trackscr_left_select_texture_name, &texIdx, 0, gNumModels - 1, 1);
            else tex = MBOX_FindTexture_WORK(trackscr_left_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(trackscr_buttonInfo[buttonIdx].handle, (intptr_t)tex, -1);
            if (trackscr_buttonInfo[buttonIdx+21].angle > 1.5707963705062866f) tex = MBOX_FindTexture_WORK(trackscr_right_select_texture_name, &texIdx, 0, gNumModels - 1, 1);
            else tex = MBOX_FindTexture_WORK(trackscr_right_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(trackscr_buttonInfo[buttonIdx+21].handle, (intptr_t)tex, -1);
            if (trackscr_buttonInfo[buttonIdx].position[1] > trackscr_maxY + trackscr_gButtonSpace * trackscr_gButtonScale || trackscr_buttonInfo[buttonIdx].position[1] < trackscr_minY - trackscr_gButtonSpace * trackscr_gButtonScale) trackscr_buttonInfo[buttonIdx].alpha = 0;
            else if (trackscr_buttonInfo[buttonIdx].position[1] > trackscr_maxY) trackscr_buttonInfo[buttonIdx].alpha = 255 - (trackscr_buttonInfo[buttonIdx].position[1] - trackscr_maxY) * 255.0f / (trackscr_gButtonSpace * trackscr_gButtonScale);
            else if (trackscr_buttonInfo[buttonIdx].position[1] < trackscr_minY) trackscr_buttonInfo[buttonIdx].alpha = 255 - (trackscr_minY - trackscr_buttonInfo[buttonIdx].position[1]) * 255.0f / (trackscr_gButtonSpace * trackscr_gButtonScale);
            else trackscr_buttonInfo[buttonIdx].alpha = 255;
            if (!OptionValid(buttonIdx)) trackscr_buttonInfo[buttonIdx].alpha = 0;
            trackscr_buttonInfo[buttonIdx+21].alpha = trackscr_buttonInfo[buttonIdx].alpha;
            buttonColor.channel[3] = trackscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor(trackscr_buttonInfo[buttonIdx].handle, (s32 *)&buttonColor);
            ZOID_SetColor(trackscr_buttonInfo[buttonIdx+21].handle, (s32 *)&buttonColor);
            if (!OptionValid(buttonIdx)) {
                MBOX_HideObject(trackscr_buttonInfo[buttonIdx].handle, 0, 15);
                MBOX_HideObject(trackscr_buttonInfo[buttonIdx+21].handle, 0, 15);
            } else if (trackscr_buttonInfo[buttonIdx].alpha == 0) {
                MBOX_HideObject(trackscr_buttonInfo[buttonIdx].handle, 0, 15);
                MBOX_HideObject(trackscr_buttonInfo[buttonIdx+21].handle, 0, 15);
                buttonY -= trackscr_gButtonSpace * trackscr_gButtonScale;
            } else {
                MBOX_ShowObject(trackscr_buttonInfo[buttonIdx].handle, 0, 15);
                MBOX_ShowObject(trackscr_buttonInfo[buttonIdx+21].handle, 0, 15);
                buttonY -= trackscr_gButtonSpace * trackscr_gButtonScale;
            }
        }
        if (clearScrollSpeed == 1) scrollSpeed = 0.0f;
        trackscr_buttonsJustInited = 0;
    }
}

