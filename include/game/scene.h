/* include/game/scene.h: the scene objects (mbox and zoid), models, textures, windows, the sky and the world */
#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "game/types.h"

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
#define TEX_TEXTURE           0x08000000  /* texBase is the image (else a display list that loads it) */
#define TEX_NOPALETTE         0x20000000  /* has no palette of its own */

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

/* Data */
extern ModelList gRomObjDefPtrs[EXTENT(64)];
extern Item12 texture_swap_shared_frames[EXTENT(400)];
extern struct Sequence20 *TextureSwapLists[EXTENT(19)];
extern struct Row20 *PaletteLists[EXTENT(19)];
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
extern s32 skyMatrices[EXTENT(4)];
extern ObjectInstance ObjInstanceList[EXTENT(600)];
extern ModelList gPalDefPtrs[EXTENT(64)];
extern s8 gFogIntensity;
extern u16 gNumLights;
extern s8 gInvisibleTrack;
extern struct Lights2 *gLights;
extern u8 gNumModels[EXTENT(4)];
extern s32 gMaxPolyInsts;
extern u8 gNumZoidWindows;
extern u32 *gWorldData;
extern struct NameEntry *WorldTable;
extern s16 scissor0[EXTENT(4)];
extern s16 scissor1[EXTENT(4)];
extern s16 scissor2[EXTENT(4)];
extern s16 scissorul[EXTENT(4)];
extern s16 scissorur[EXTENT(4)];
extern s16 scissorll[EXTENT(4)];
extern s16 scissorlr[EXTENT(4)];
extern struct Box104 *WorldBase;
extern s16 Worlddlidx;
extern struct Node104 *cur_world_obj;
extern f32 gViewableDistance;
extern ModelList gTexDefPtrs[EXTENT(64)];
extern OSMesgQueueWords audio_retrace_queue;
extern OSMesg audio_retrace_msgs[EXTENT(8)];
extern OSMesgQueue sample_dma_pi_queue;
extern s32 sample_dma_pi_msg;
extern f32 HFOV;
extern s32 gNumObjects;
extern s32 gMaxObjects;
extern s32 gNumMatrices;
extern s32 gMaxMatrices;
extern s32 cull_vertex_count;
extern s32 cull_vertex_max;
extern volatile s16 ObjHead;
extern PolyInstance PolyInstanceList[EXTENT(39)];
extern s32 gMatrices[EXTENT(560)];
extern s32 gNumPolyInsts;
extern f32 gGlobalCameraFacingScale;
extern OSIoMesg sample_dma_io;
extern ArbEntry ZoidWinInfo[EXTENT(4)];
extern s8 gSplitScreen;
/* pc_sample_dma_list: declared by its files (src/blob/groups/sample_dma_next/sample_dma_next.c: defines it) */

/* Functions */
struct tModelPart;   /* a world part (MAT3 tModelPart): groups/targets/NewWorldObject.c defines it */
s32 anim_show_on_high_score(void *blit);
void audio_retrace_thread_entry(void *arg);
u32 displaylist_get_texture_image(u32 *dl);
void displaylist_set_tile_size(struct Command *commands, s32 uls, s32 ult, s32 lrs, s32 lrt);
s32 FindWorldGroup(f32 *position);
s32 flip_triangle_winding(u32 *gfx);
s32 game_strcmp(u8 *s1, u8 *s2);
s32 game_strncmp(const u8 *s1, const u8 *s2, u32 n);
u32 get_world_part_flags(struct tModelPart *n);
void InitTextureAnimation(void);
s32 MBOX_FindObject_WORK(char *name, s8 firstModel, s8 lastModel, s8 complain);
s16 MBOX_GetChild(s32 node);
s16 MBOX_GetNext(s32 node);
f32 MBOX_GetObjectRadius(s32 handle);
void MBOX_GetPosition(void *pos, s32 node);
void MBOX_HideObject(s32 node, s32 mode, u32 views);
s32 MBOX_NewObject(u32 obj, uintptr_t mat, s32 parent, u32 flags);
s32 MBOX_NewObjectCam(s32 obj, intptr_t mat, s16 camlink, s32 parent, s32 flags);
void MBOX_ShowObject(s32 node, s32 mode, u32 views);
s16 NewWorld(u8 *world, s16 parent);
void NewWorldObject(struct tModelPart *wnode, s16 parent);
s32 objcmp(u8 *obj1, u8 *obj2);
void relocate_wave_model(s32 idx);
void RemoveObjectTree(s16 objnum, s32 removeChildren, s32 removeNext);
s32 sample_dma_next(void);
void sample_dma_thread_entry(intptr_t arg);
void SkyClear(void);
void SkyInit(void);
void SkyUpdate(s32 winidx);
f32 tan_cody_waite(f32 x);
s32 texcmp(u8 *ta, u8 *tb);
void TextureAnimation(void);
s32 world_name_compare(intptr_t key, intptr_t entry);
void WorldClear(void);
void WorldCullObjects(s32 unused, f32 *pos, s32 show);
void WorldFogDensity(s32 winidx, f32 *pos);
void ZOID_BuildCullingList(f32 *min, f32 *max, s16 node);
s32 ZOID_ChangeTexture(u16 obj, s16 lod, u16 texidx);
void ZOID_ClearPolyFlags(struct Header *poly, u16 mask);
void zoid_draw_window(s32 view, struct View *eye);
s16 ZOID_FindObjInstance(void *name);
void *ZOID_GetColor(s16 node);
void ZOID_GetFog(u16 *min, u16 *max, u8 *red, u8 *green, u8 *blue, u8 *alpha, s32 winidx);
void *ZOID_GetWindow(s32 winidx);
void ZOID_HidePoly(PolyInstance *poly);
void ZOID_Init(void);
void ZOID_InsertObject(s16 node, s16 parent);
s16 ZOID_LastSibling(s16 node);
void *ZOID_NewMatrix(void);
u32 ZOID_ObjectFlags(s16 node);
s16 ZOID_ObjParent(s16 node);
s16 ZOID_ObjPrevObj(s16 node);
void ZOID_ProjectWindow(s32 widx, f32 *point, struct tUVMatrix *mat, f32 *body, s16 *screen);
void ZOID_RemoveObject(s16 node);
void ZOID_RemoveObjectTree(s16 node, s32 removeChildren);
void ZOID_Reset(void);
void zoid_reset_lights(void);
void ZOID_ResetWindows(void);
void zoid_scale_windows(f32 x, f32 y);
void zoid_set_obj_palette(s16 node, uintptr_t palette);
void zoid_set_obj_tile_size(s16 node, uintptr_t tileSize, u32 unused);
s32 zoid_set_window_count(s32 count);
void ZOID_SetBGColor(u16 rgba5551);
void ZOID_SetColor(s16 node, union Color *col);
void ZOID_SetEnvColor(s16 node, union Color *col);
void ZOID_SetObjectDef(s16 node, u16 def);
void ZOID_SetObjectFlags(s32 node, u32 flags);
void ZOID_SetObjectParent(s16 node, s16 newparent);
void ZOID_SetObjLights(s32 node, s32 numLights, uintptr_t lights);
void ZOID_SetPolyFlags(struct Header *poly, u16 mask);
void ZOID_SetPolyZ(PolyInstance *poly, s16 z);
void ZOID_SetScissor(u16 *s, u16 ulx, u16 uly, u16 lrx, u16 lry);
void ZOID_SetViewport(s32 winidx, void *vp, void *s);
void ZOID_ShowPoly(PolyInstance *poly);
s32 ZOID_SplitMenuScreen(s32 num);
s32 ZOID_SplitScreen(s32 n);
s16 ZOID_TrueObjParent(s16 child);
void ZOID_UpdateObject(s16 node, void *pos, void *uvs);
void ZOID_WindowTo3D(s32 widx, s16 *screen, struct tUVMatrix *mat, f32 zdist, f32 *point);

#endif /* GAME_SCENE_H */
