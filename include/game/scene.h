/* include/game/scene.h: the scene objects (mbox and zoid), models, textures, windows, the sky and the world */
#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "game/types.h"

struct tCamera;   /* a window's camera (camera.h), which the scene draw looks through */

/* Constants */

/* tPaletteAnim.state: what a track's palette animation does each period (TextureAnimation). The colours of 4-8 are
 * RGBA5551. */
#define PALANIM_ROTATE_UP     0     /* entries start..end move up one, end wraps to start */
#define PALANIM_ROTATE_DOWN   1     /* entries start..end move down one, start wraps to end */
#define PALANIM_SWAP2         2     /* the block start..end swaps with the block after it (offset entries apart) */
#define PALANIM_CYCLE3        3     /* three such blocks rotate */
#define PALANIM_LIGHT_RED     4     /* a traffic light (entries start, +1, +2: red, yellow, green): red, 10.5 s */
#define PALANIM_LIGHT_YELLOW  5     /* yellow, 0.5 s */
#define PALANIM_LIGHT_GREEN   6     /* green, 10 s */
#define PALANIM_CHASE10       7     /* ten lamps light one a period, then all go dark */
#define PALANIM_CHASE7        8     /* two rows of seven lamps, likewise */
#define PALANIM_SCROLL_S      9     /* from here on ptr is a texture's load display list: scroll it in s */
#define PALANIM_SCROLL_T      10    /* scroll it in t */

/* ObjInstanceList[].flags: how a scene node is drawn (ZOID_AddObject's `flags`; the scene draw, Traverse). */
#define OBJ_LOD_MASK          0x3         /* with OBJ_FORCE_LOD: the level of detail to draw */
#define OBJ_FORCE_LOD         0x4         /* draw at level flags & 3, whatever the distance */
#define OBJ_PARENT_DISTANCE   0x8         /* its children choose their levels by its distance, not their own */
#define OBJ_CULL_BOX_A        0x10        /* culled by the box cull_box_vertices_a */
#define OBJ_CULL_BOX_B        0x20        /* culled by the box cull_box_vertices_b */
#define OBJ_NO_MATRIX         0x80        /* drawn with its parent's matrix: it has none of its own */
#define OBJ_HIDDEN_VIEW0      0x100       /* bits 8-11: not drawn in views 0-3 (0x100 << view) */
#define OBJ_VIEW_VERTEX       0x1000      /* loads the view's vertex (frame_dynamic->vtx[view]) as vertex 8 first */
#define OBJ_PRIM_COLOR        0x2000      /* drawn with its colour as the primitive colour */
#define OBJ_ENV_COLOR         0x4000      /* drawn with its environment colour */
#define OBJ_UPRIGHT           0x8000      /* turned to face the camera about the vertical only (zoid_upright_matrix) */
#define OBJ_FACE_CAMERA       0x10000     /* turned to face the camera (a billboard), scaled by its scale */
#define OBJ_LATE_A            0x40000     /* drawn in the first pass after the flat polygons */
#define OBJ_ENV_MAPPED        0x80000     /* drawn with a reflection: its texture is mapped by the view direction */
#define OBJ_FADE              0x200000    /* fades out from 24 to 80 feet away (the cars' sheen) */
#define OBJ_FAR               0x400000    /* far scenery: its matrix is built in the far range (build_far_node_matrix) */
#define OBJ_LATE_B            0x800000    /* drawn in the second pass after the flat polygons */
#define OBJ_HIDDEN            0x80000000  /* not drawn in any view */

/* tModelDef.lod[].flags: what a level of detail of an object needs before its display list. */
#define LOD_TEXTURED          0x1         /* load its texture (the node's, or the one ZOID_ChangeTexture set) */
#define LOD_RESET_TLUT        0x2         /* its list changes the palette: load the node's again after it */
#define LOD_PRIM_COLOR        0x4         /* set the node's colour as the primitive colour */
#define LOD_LIGHTING          0x10        /* lit by the scene's lights */
#define LOD_TEXTURE_SET       0x8000      /* ZOID_ChangeTexture gave it the texture texidx */

/* tTexture.flags: how a texture is loaded and sampled. */
#define TEX_CLAMP_S           0x00010000  /* clamped in s (else it wraps) */
#define TEX_CLAMP_T           0x00020000  /* clamped in t */
#define TEX_MIRROR_S          0x00040000  /* mirrored in s */
#define TEX_MIRROR_T          0x00080000  /* mirrored in t */
#define TEX_IMAGE_DIRECT           0x08000000  /* texBase is the image (else a display list that loads it) */
#define TEX_NOPALETTE         0x20000000  /* has no palette of its own */

/* MBOX_FindObject_WORK, MBOX_FindTexture_WORK: warn, the last argument, complains when the name isn't found. */
#define MBOX_WARN             1

/* PolyInstanceList[].flags: a flat polygon's (ZOID_NewPoly, the scene draw). */
#define POLY_VIEW0            0x1         /* bits 0-3: drawn in views 0-3 (1 << view) */
#define POLY_CAMERA_FRAME     0x10        /* its vertices are in the camera's axes, relative to the camera */
#define POLY_MIRRORED         0x80        /* its corners are taken mirrored on a mirrored track (ZOID_UpdatePoly) */
#define POLY_NO_FOG           0x100       /* drawn without fog */
#define POLY_TEXTURED         0x400       /* textured with texidx */
#define POLY_TWO_SIDED        0x1000      /* both its faces are drawn */
#define POLY_FIXED_DEPTH      0x2000      /* drawn at depth z (the primitive depth) */
#define POLY_SCREEN           0x4000      /* a filled rectangle in screen coordinates */
#define POLY_HIDDEN           0x8000      /* not drawn (ZOID_HidePoly) */

/* Types */

/* A scene node (ObjInstanceList; MAT3's mbox and zoid objects): an object definition drawn by a matrix relative to
 * its parent's, in a tree of first children and next siblings that the scene draw walks (Traverse), with what it
 * draws in place of its model's (lights, textures, a tile, a palette) and its colours. */
typedef struct ObjectInstance {
    u32 flags;                    /* OBJ_* */
    u32 culling;                  /* its culling box: (first cull vertex << 3) | 7 (ZOID_BuildCullingList), 0 none */
    tUVMatrix *mat;               /* its matrix, relative to its parent */
    f32 scale;
    f32 w;                        /* the matrix's w */
    u16 index;                    /* its object, (model << 10) | definition; 0xFFFF a free node */
    s16 child;                    /* its first child, -1 none */
    s16 next;                     /* its next sibling, -1 none */
    s16 numLights;                /* its own lights' count, -1 the scene's */
    struct Lights2 *lights;       /* its own lights */
    struct tTexture *texture[4];  /* a texture per level of detail in place of the model's, NULL none */
    struct TexTile *tile;         /* the part of the texture to load in place of the model's */
    struct TexTile *tileSize;     /* the part of it to draw */
    struct tPaletteDef *palette;  /* a palette in place of the model's */
    tRGBA color;                  /* the primitive colour */
    tRGBA envcolor;               /* the environment colour */
} ObjectInstance;

/* A window of the scene (MAT3 tWindowInfo; ZoidWinInfo, one a player in split screen): where on the screen it
 * draws, its projection (ZOID_SetWindow), its clip planes and its fog (ZOID_SetFog, WorldFogDensity). The scene
 * draw (zoid_draw_window) builds each view from it; ZOID_ProjectWindow and ZOID_WindowTo3D map between the world
 * and its pixels. */
typedef struct tWindowInfo {
    PcViewport *vp;     /* its viewport (vp0 ...) */
    u16 *s;             /* its scissor rectangle (scissor0 ...): left, top, right, bottom, pixels */
    u32 flags;          /* 1: drawn orthographic (ZOID_SetWindow was given no field of view), 0 in perspective */
    f32 whviewang;      /* the horizontal field of view, radians */
    f32 wvviewang;      /* the vertical one */
    f32 whvangtan;      /* the tangent of half the horizontal one */
    f32 wvvangtan;      /* the tangent of half the vertical one */
    f32 wihvangtan;     /* 0.5 / whvangtan: a point's screen x is wwidth * wihvangtan * x / z + whcenter */
    f32 wivvangtan;     /* 0.5 / wvvangtan */
    f32 wwidth;         /* its size, pixels */
    f32 wheight;
    f32 whcenter;       /* its centre on the screen, pixels */
    f32 wvcenter;
    f32 aspect;         /* wwidth / wheight (half the height in the N64's interlaced modes) */
    f32 frontclip;      /* the near clip plane, feet */
    f32 backclip;       /* the far one */
    u16 fogmin;         /* the fog's range in the RDP's depth units (0-1000): it starts at fogmin and is full at */
    u16 fogmax;         /* fogmax. MAT3 has fogmin signed; every reader takes it unsigned */
    u8 fogcolor[4];     /* the fog's colour: red, green, blue, alpha */
} tWindowInfo;

/* A flat polygon's corner (MAT3 anon_ob_tc_18, whose texture coordinates are floats). */
typedef struct tPolyVertex {
    f32 ob[3];          /* where it is: feet times 16, the scene's units (a POLY_SCREEN polygon's: pixels, x and y) */
    s16 tc[2];          /* its texture coordinates s and t, in 32nds of a texel */
    tRGBA cn;           /* its colour (a POLY_SCREEN polygon's is its first corner's) */
} tPolyVertex;

/* A flat polygon of the scene (MAT3 tPoly; PolyInstanceList, ZOID_NewPoly): a triangle or a quad drawn after the
 * scene's models, a car's shadow or arrow, a skid mark, a scorch mark, the countdown, the front end's background.
 * A POLY_SCREEN one is a filled rectangle on the screen, from its first corner's x and y to its second's. */
typedef struct tPoly {
    s16 nverts;         /* how many corners it has (3 or 4), 0 a free polygon */
    u16 flags;          /* POLY_* */
    u16 texidx;         /* its texture's handle with POLY_TEXTURED: (model << 10) | index */
    s16 z;              /* its depth with POLY_FIXED_DEPTH (ZOID_SetPolyZ) */
    tPolyVertex verts[4];
} tPoly;

/* A blast sprite's matrix (MAT3 tBlastMat; gBlastMats, 50 taken in turn by StartBlast). */
typedef struct tBlastMat {
    tUVMatrix *mat;     /* from the scene's persistent pool (ZOID_NewMatrix, InitBlast) */
} tBlastMat;

/* A skid mark (MAT3 tSkid; gSkidArray, listed by gSkidList): a quad laid along a tire's track while it skids
 * (UpdateSkids), which fades once the skid has stopped (StopSkid, AnimateSkids). */
typedef struct tSkid {
    struct tSkid *next;     /* the pool's links (MAT3 tStatDLLE: GetStatLLE, FreeStatLLE) */
    struct tSkid *prev;
    f32 lastTime;           /* gSeconds when the skid last grew: its fade starts from it */
    tPoly *poly;            /* its quad (ZOID_NewPoly) */
    f32 pos[3];             /* its middle once it has stopped (StopSkid): the far-mark search takes the farthest */
    tRGBA color;            /* its colour; its alpha fades */
    s32 active;             /* set once the skid has stopped: the mark fades */
} tSkid;

/* A smoke puff (MAT3 smoke.c's anon_ll_mat_58; gSmokeObjs, listed by gSmokeList): tire smoke, a wreck's smoke, a
 * blast's flames, the wing trails, a water spray (StartSmoke, StartWater), which UpdateSmoke moves, grows, fades
 * and animates until it is done. */
typedef struct SmokePuff {
    struct SmokePuff *next; /* the pool's link (MAT3 tStatLLE) */
    tUVMatrix mat;          /* its orientation and size, and its position: its scene object draws by it */
    union {
        s32 mbIdx;          /* its scene object's handle (MBOX_NewObject) */
        s16 mbNode;         /* the handle's low half, the node, as the PC (little-endian) holds it */
    };
    f32 tlifeTime;          /* MAT3's whole life; nothing reads it */
    f32 lifeTime;           /* seconds left (fire: the size it grows to) */
    f32 swTime;             /* seconds to its next animation frame */
    f32 vel[3];             /* its drift, feet a second (fire, a water spray: its offset from the car) */
    u32 data;               /* PUFF_FORWARD, PUFF_WATER, its type << PUFF_TYPE_SHIFT (cars.h) */
    s16 slot;               /* the car it came from */
    s16 tIdx;               /* its animation frame, 0-7 (fire: -1 while growing, then 0-14) */
} SmokePuff;

/* A track's palette animation (MAT3 tPaletteAnim; PaletteLists, InitTextureAnimation, TextureAnimation): a palette
 * rotation or swap, a traffic light, a lamp chaser, or a texture scroll. */
typedef struct tPaletteAnim {
    char *name;         /* the palette's name (a texture's for the scrolls); NULL ends a track's list */
    s16 start, end;     /* the entries it works on; a scroll's position and its wrap */
    s8 offset;          /* the swaps' gap, a scroll's step, the chasers' count */
    s8 state;           /* what it does: PALANIM_* (0-10) */
    s16 speed;          /* its period, in 30 Hz frames */
    f32 time;           /* the seconds to its next step */
    union {
        u16 *palette;   /* the palette's colours (PALANIM_ROTATE_UP to PALANIM_CHASE7) */
        Gfx *commands;  /* the texture's load display list (the scrolls) */
    } ptr;
} tPaletteAnim;

/* A frame of a texture flipbook (MAT3 tTextureBase; texture_swap_shared_frames, a flipbook's swapTexs). */
typedef struct tTextureBase {
    char *name;         /* its texture's name */
    tTexture *tex;      /* the texture, once found (InitTextureAnimation) */
    void *texBase;      /* the image it shows: the texture's texBase, or the image its load display list loads */
} tTextureBase;

/* A texture flipbook (MAT3 tTextureSwap; TextureSwapLists): the texture of frame startnum shows frame idx's image,
 * idx stepping every initTime seconds. */
typedef struct tTextureSwap {
    s16 num;            /* the frames; 0 ends a track's list */
    s16 startnum;       /* the frame whose texture is the one drawn */
    s16 fd;             /* 1 steps forward, 0 backward */
    s16 idx;            /* the frame shown */
    f32 time;           /* the seconds to its next step */
    f32 initTime;       /* its period, seconds */
    tTextureBase *swapTexs;
} tTextureSwap;

/* Data */
extern ModelDefTable gRomObjDefPtrs[EXTENT(64)];
extern tTextureBase texture_swap_shared_frames[EXTENT(400)];   /* frames several flipbooks share */
extern tTextureSwap *TextureSwapLists[EXTENT(19)];   /* each track's flipbooks */
extern tPaletteAnim *PaletteLists[EXTENT(19)];       /* each track's palette animations */
extern u32 track1Parts[EXTENT(117)][4];
extern u32 track2Parts[EXTENT(117)][4];
extern u32 track3Parts[EXTENT(117)][4];
extern u32 track4Parts[EXTENT(121)][4];
extern u32 track5Parts[EXTENT(103)][4];
extern u32 track6Parts[EXTENT(122)][4];
extern u32 track7Parts[EXTENT(1)][4];
extern u32 track8Parts[EXTENT(1)][4];
extern u32 track9Parts[EXTENT(1)][4];
extern u32 track10Parts[EXTENT(1)][4];
extern u32 track11Parts[EXTENT(14)][4];
extern u32 track12Parts[EXTENT(1)][4];
extern u32 track13Parts[EXTENT(1)][4];
extern u32 track14Parts[EXTENT(1)][4];
extern u32 track15Parts[EXTENT(1)][4];
extern u32 track16Parts[EXTENT(1)][4];
extern u32 track17Parts[EXTENT(1)][4];
extern u32 track18Parts[EXTENT(1)][4];
extern u32 track19Parts[EXTENT(25)][4];
extern u8 trackParts[EXTENT(20)];
extern u32 showAllParts[EXTENT(4)];
extern struct Region *trackFog[EXTENT(15)];
extern f32 fog_viewable_distance[EXTENT(4)][4];
extern s16 fogDistance[EXTENT(4)][4];
extern s16 fogMin[EXTENT(5)][4];
extern s32 default_lights;
extern PcViewport vp0;
extern PcViewport vp1;
extern PcViewport vp2;
extern PcViewport vpul;
extern PcViewport vpur;
extern PcViewport vpll;
extern PcViewport vplr;
extern u8 sky_object_name[EXTENT(2888)];
extern f32 tan_max_arg;
extern f32 tan_two_over_pi;
extern f32 tan_half_pi_hi;
extern f32 tan_half_pi_lo;
extern f32 tan_epsilon;
extern f32 tan_p3;
extern f32 tan_p2;
extern f32 tan_p1;
extern f32 tan_q4;
extern f32 tan_q3;
extern f32 tan_q2;
extern f32 tan_q1;
extern u32 gBgColor;
/* Each window's sky dome matrix (SkyInit; SkyUpdate keeps it on the camera), NULL for none: a matrix of the
 * persistent pool (ZOID_NewMatrix). MAT3 sky.c's tUVMatrix *skyMatrices[4]; the N64 kept them as 32-bit addresses. */
extern tUVMatrix *skyMatrices[EXTENT(4)];
extern ObjectInstance ObjInstanceList[EXTENT(600)];
extern PaletteDefTable gPalDefPtrs[EXTENT(64)];
extern s8 gFogIntensity;
extern s16 gNumLights;
extern s8 gInvisibleTrack;
extern struct Lights2 *gLights;
/* How many wave instances the model searches look through (MAT3 unsigned char gNumModels): gNumModels[0]. The
 * data loader lays the datum out as 4 bytes (to the next one), so it stays an array until the layout may change
 * (wave 3). Volatile: the background loader raises it (SetWaveInstanceModel) while the game reads it. */
extern volatile u8 gNumModels[EXTENT(4)];
extern s32 gMaxPolyInsts;
extern u8 gNumZoidWindows;
extern s32 *gWorldData;                 /* *gWorldData: how many world trees WorldTable has (LoadTrack) */
extern struct NameEntry *WorldTable;    /* the track's world trees by name (struct pc_world_records) */
extern s16 scissor0[EXTENT(4)];
extern s16 scissor1[EXTENT(4)];
extern s16 scissor2[EXTENT(4)];
extern s16 scissorul[EXTENT(4)];
extern s16 scissorur[EXTENT(4)];
extern s16 scissorll[EXTENT(4)];
extern s16 scissorlr[EXTENT(4)];
extern struct tModelPart *WorldBase;   /* the track's world parts (FindWorld), linked by index */
extern s16 Worlddlidx;
extern struct tModelPart *cur_world_obj;
extern f32 gViewableDistance;
extern TextureDefTable gTexDefPtrs[EXTENT(64)];
extern OSMesgQueue audio_retrace_queue;
extern OSMesg audio_retrace_msgs[EXTENT(8)];
extern OSMesgQueue sample_dma_pi_queue;
extern OSMesg sample_dma_pi_msg;
extern f32 HFOV;
extern s32 gNumObjects;
extern s32 gMaxObjects;
extern s32 gNumMatrices;
extern s32 gMaxMatrices;
extern s32 cull_vertex_count;
extern s32 cull_vertex_max;
extern volatile s16 ObjHead;
extern tPoly PolyInstanceList[EXTENT(39)];   /* the flat polygons: gNumPolyInsts in use (200 at most, ZOID_NewPoly) */
extern s32 gMatrices[EXTENT(560)];
extern s32 gNumPolyInsts;
extern f32 gGlobalCameraFacingScale;
extern tWindowInfo ZoidWinInfo[EXTENT(4)];   /* the scene's windows (gNumZoidWindows in use) */
extern s8 gSplitScreen;

/* Functions */
s32 anim_show_on_high_score(void *blit);
void audio_retrace_thread_entry(void *arg);
void *displaylist_get_texture_image(Gfx *dl);
void displaylist_set_texture_image(Gfx *dl, void *image);
void displaylist_set_tile_size(Gfx *commands, s32 uls, s32 ult, s32 lrs, s32 lrt);
s32 FindWorldGroup(f32 *position);
s32 flip_triangle_winding(Gfx *gfx);
s32 game_strcmp(u8 *s1, u8 *s2);
s32 game_strncmp(const u8 *s1, const u8 *s2, u32 n);
u32 get_world_part_flags(struct tModelPart *n);
void InitTextureAnimation(void);
tPaletteDef *MBOX_FindPalette_WORK(char *name, u16 *handle, s8 firstModel, s8 lastModel);
s32 MBOX_FindObject_WORK(char *name, s8 firstModel, s8 lastModel, s8 complain);
struct tTexture *MBOX_FindTexture_WORK(char *name, s16 *handle, s8 firstModel, s8 lastModel, s32 flag);
s16 MBOX_GetChild(s32 node);
s16 MBOX_GetNext(s32 node);
f32 MBOX_GetObjectRadius(s32 handle);
void MBOX_GetPosition(void *pos, s32 node);
void MBOX_HideObject(s32 node, s32 mode, u32 views);
s32 MBOX_NewObject(u32 obj, void *mat, s32 parent, u32 flags);
s32 MBOX_NewObjectCam(s32 obj, void *mat, s16 camlink, s32 parent, s32 flags);
void MBOX_ShowObject(s32 node, s32 mode, u32 views);
s16 NewWorld(u8 *world, s16 parent);
void NewWorldObject(struct tModelPart *wnode, s16 parent);
s32 objcmp(u8 *obj1, u8 *obj2);
void RemoveObjectTree(s16 objnum, s32 removeChildren, s32 removeNext);
s32 sample_dma_next(void);
void sample_dma_thread_entry(void *arg);
void SkyClear(void);
void SkyInit(void);
void SkyUpdate(s32 winidx);
f32 tan_cody_waite(f32 x);
s32 texcmp(u8 *ta, u8 *tb);
void TextureAnimation(void);
s32 Traverse(Gfx **gp, ObjectInstance *parent, ObjectInstance *node, s32 view, s32 depth,
             s32 more_after, s32 use_parent_distance, struct tPaletteDef *palette);
tWindowInfo *view_bind(s32 k, void *vp, s16 *r);
s32 world_name_compare(void *name, void *entry);
void WorldClear(void);
void WorldCullObjects(s32 unused, f32 *pos, s32 show);
void WorldFogDensity(s32 winidx, f32 *pos);
void ZOID_BuildCullingList(f32 *min, f32 *max, s16 node);
s32 ZOID_AddObject(s32 obj, void *mat, s16 parent, s32 flags);
s32 ZOID_ChangeTexture(u16 obj, s16 lod, u16 texidx);
void ZOID_ClearPolyFlags(tPoly *poly, u16 mask);
s32 zoid_draw_object(Gfx **gp, ObjectInstance *parent, s32 depth, s32 use_parent_distance,
                     ObjectInstance *node, s32 view, struct tPaletteDef *palette);
void zoid_draw_window(s32 view, struct tCamera *eye);
tUVMatrix *ZOID_FindMatrix(s16 node);
s16 ZOID_FindObjInstance(void *name);
void *ZOID_GetColor(s16 node);
void ZOID_GetFog(u16 *min, u16 *max, u8 *red, u8 *green, u8 *blue, u8 *alpha, s32 winidx);
tWindowInfo *ZOID_GetWindow(s32 winidx);
void ZOID_HidePoly(tPoly *poly);
void ZOID_Init(void);
tWindowInfo *ZOID_InitializeWindow(s32 idx, void *vp, void *s, f32 hang, f32 vang, f32 width, f32 height,
                                   f32 hcenter, f32 vcenter);
void ZOID_InsertObject(s16 node, s16 parent);
s16 ZOID_LastSibling(s16 node);
void zoid_load_palette(Gfx **gfxp, struct tPaletteDef *pal);
void zoid_load_texture(Gfx **gp, struct tTexture *tex, struct TexTile *rect, struct TexTile *clip,
                       struct tPaletteDef *palettes);
tUVMatrix *ZOID_NewMatrix(void);
tPoly *ZOID_NewPoly(s32 nverts, f32 *xyz, u16 texidx, u8 *color, u16 flags, s32 useTidx);
s32 zoid_node_visible(void *node, s32 view);
u32 ZOID_ObjectFlags(s16 node);
s16 ZOID_ObjParent(s16 node);
s16 ZOID_ObjPrevObj(s16 node);
void ZOID_ProjectWindow(s32 widx, f32 *point, tUVMatrix *mat, f32 *body, s16 *screen);
void ZOID_RemoveObject(s16 node);
void ZOID_RemoveObjectTree(s16 node, s32 removeChildren);
void ZOID_RemovePoly(tPoly *poly);
void ZOID_Reset(void);
void zoid_reset_lights(void);
void ZOID_ResetWindows(void);
void zoid_scale_windows(f32 x, f32 y);
void zoid_set_obj_palette(s16 node, uintptr_t palette);
void zoid_set_obj_tile_size(s16 node, uintptr_t tileSize, u32 unused);
s32 zoid_set_window_count(s32 count);
void ZOID_SetBGColor(u16 rgba5551);
void ZOID_SetColor(s16 node, tRGBA *col);
void ZOID_SetEnvColor(s16 node, tRGBA *col);
tWindowInfo *ZOID_SetFog(s16 min, s16 max, u8 red, u8 green, u8 blue, u8 alpha, u32 winidx);
void ZOID_SetObjectDef(s16 node, u16 def);
void ZOID_SetObjectFlags(s32 node, u32 flags);
void ZOID_SetObjectParent(s16 node, s16 newparent);
void ZOID_SetObjLights(s32 node, s32 numLights, void *lights);
void ZOID_SetPolyFlags(tPoly *poly, u16 mask);
void ZOID_SetPolyZ(tPoly *poly, s16 z);
void ZOID_SetScissor(u16 *s, u16 ulx, u16 uly, u16 lrx, u16 lry);
void ZOID_SetTexture(s16 node, void *tex, s32 lod);
void ZOID_SetViewport(s32 winidx, void *vp, void *s);
tWindowInfo *ZOID_SetWindow(s32 winidx, f32 hangle, f32 vangle, f32 width, f32 height, f32 hcenter,
                            f32 vcenter);
void ZOID_ShowPoly(tPoly *poly);
void ZOID_ShowPoly_bg(tPoly *poly);
s32 ZOID_SplitMenuScreen(s32 num);
s32 zoid_subtree_visible(ObjectInstance *node, s32 view);
s32 ZOID_SplitScreen(s32 n);
s16 ZOID_TrueObjParent(s16 child);
void ZOID_UpdatePoly(tPoly *poly, s32 nverts, tVec3 *xyz, u16 texidx, u8 *color, u16 flags, s32 useTidx);
void ZOID_UpdateObject(s16 node, void *pos, void *uvs);
void ZOID_WindowTo3D(s32 widx, s16 *screen, tUVMatrix *mat, f32 zdist, f32 *point);

#endif /* GAME_SCENE_H */
