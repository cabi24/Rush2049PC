/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision of runtime_a_joint_transforms: UpdateSelectCar UpdateSelectCar
 * (MAT3 selectcarload.c, GC 0x8003bf00; N64 nine-argument form with r, g, b) becomes a claimed member, with the
 * deleted-inline stub of its helper CreateSelectCar = CreateSelectCar (MAT3 static, inlined at its one call site;
 * written non-static and not kept so its deleted procedure is emitted under that name). Member SetSelectTireSizes
 * (SetSelectTireSizes) is the installed text, unchanged.
 * Frame (retail 232 = save area 88 + merged locals area 116 + six temp homes 92..112, rounded to 8):
 *   - the inlined CreateSelectCar has one 4-byte local (`car`, used). umerge starts each inlined area on an
 *     8-byte boundary but does not round the last one, so its 8-byte call reserve plus that local make the merged
 *     area 4 mod 8 (116); with no callee local the area is 112 or 120 and every temp home is 4 bytes off (33 words).
 *   - DISCLOSED: eleven unused s32 locals in UpdateSelectCar, x0 (between cam and alphaCol: retail alphaCol is at
 * sp+184, cam at sp+192) and u0.u9 below wheelTex. They are the exact frame residual (measured,
 *     90DCC/fr): none -> frame 184, x0 only -> 192, x0 + u0..u8 -> 224 (59 words each), x0 + u0..u9 -> 232 = retail;
 *     without x0 alphaCol moves to sp+188 (3 words). Retail's locals area holds these 44 bytes untouched by any
 *     instruction; their identity is unknown.
 * - CreateSelectCar's cam parameter is s32 (stops the cam-load CSE web across the calls).
 * alphaCol keeps MAT3's aggregate initializer {255, 255, 255, 255}: its 4-byte image is this function's own data,
 * retail 0x803B65B4 = FF FF FF FF (the installed context read it as the extern selectcarload_alphaCol; same code).
 * Every old member is unchanged and still strict MATCH. */
/* (installed) A:80390A28 local group candidate, 100/100 words, 400 bytes. */
typedef signed char s8;typedef unsigned char u8;
typedef short s16;typedef unsigned short u16;
typedef int s32;typedef unsigned int u32;typedef float f32;
typedef union Color {u32 word;u8 channel[4];} Color;
typedef union tRGBA {u8 c[4];u32 rgba;} tRGBA;      /* MAT3 rush_types.h */
typedef struct ModelState {
    s32 handle,parts[5];
    u8 unaccessed24[4];
    s32 joints[4];
    u8 unaccessed44[20];
} ModelState;
typedef struct LoadState {
    s8 created,hidden,parts_ready,load_started;
    s32 resource;
    s8 appearance[3],unaccessed11;
    s32 texture;
    f32 front_scale,rear_scale;
} LoadState;
typedef struct Transform {f32 matrix[3][3],position[3];} Transform;
typedef struct ModelDescription {u8 unaccessed[112];f32 joint_position[4][3];} ModelDescription;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjectFlags { u32 flags, word4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } ObjectFlags;
extern ModelState gCarParts[];
extern LoadState gSelectCar[];
extern ModelDescription *gCarModels[13];
extern f32 gFrontTireScale[][13],gRearTireScale[][13];
extern s8 D_80111589[][13];
extern s8 gTireRims[5][13];      /* gTireRims */
extern intptr_t gWheelTextures[];
extern f32 unitmatrix[3][3],FSCLS_from_one[];
/* PC: D_801106C4 is FSCLS[3] and then, up to trackSpots, the tracks' respawn records init_camera_on_track reads
 * ({s8 flag; f32 pos[3]}, 16 bytes; trackSpots points at each track's). The loader converts ROM data by its
 * declaration: as f32[] each flag byte was swapped as part of an f32 and read as 0 (docs/port/DATA.md 8). */
typedef struct PcRespawn { s8 flag; u8 pad1[3]; f32 pos[3]; } PcRespawn;
extern struct PcFscls { f32 fscls[3]; PcRespawn respawn[30]; } D_801106C4;
#define FSCLS D_801106C4.fscls
extern s16 gNumWindows,gCarModelNums[];
extern s8 gFrameScale,expansion_pak_present,gTrackNum;
extern Color selectcarload_alphaCol;
extern ObjectFlags ObjInstanceList[];
extern intptr_t ZOID_FindMatrix(s16);
extern void ScaleUV(f32 (*)[3],f32 (*)[3],f32);
extern void CopyUV(void *,void *);
extern void MBOX_HideObject(s32,s32,u32);
extern void MBOX_ShowObject(s32,s32,u32);
extern s32 WaveInstanceReady(s32);
extern void UnloadSelectCar(s16);
extern s32 LoadSelectCar(s16,s16,s32 *);
extern void ZOID_RemoveObjectTree(s16,s32);
extern void InitCarDynamicObjs(s16,s16,s16);
extern void CreateCar(s16,s16,s32);
extern void SetSheen(s16,s16,s16);
extern void SetupCarColor(s16,s16,u8,u8,u8);
extern void SetFrameColor(s16,s32 *);
extern void ZOID_UpdateObject(s16,void *,void *);
extern void ZOID_SetTexture(s16,intptr_t,s32);

void SetSelectTireSizes(s16 id,s16 player)
{
    s16 model = id % 13;
    s16 i;
    Transform *transform;
    ModelDescription *description;
    description = gCarModels[model]; for (i = 0; i < 4; i++) {
        transform = (Transform *)ZOID_FindMatrix((s16)gCarParts[id].joints[i]);
        transform->position[0] = description->joint_position[i][0];
        transform->position[1] = description->joint_position[i][1] + 1.25f;
        transform->position[2] = description->joint_position[i][2];
        if (i >= 2) {
            if (gRearTireScale[player + 1][model] != 1.0f)
                ScaleUV(unitmatrix,transform->matrix,gRearTireScale[player + 1][model]);
            else CopyUV(unitmatrix,transform->matrix);
        } else {
            if (gFrontTireScale[player + 1][model] != 1.0f)
                ScaleUV(unitmatrix,transform->matrix,gFrontTireScale[player + 1][model]);
            else CopyUV(unitmatrix,transform->matrix);
        }
    }
}

void CreateSelectCar(s16 slot, s16 player, s32 cam)
{
    ModelState *car = &gCarParts[slot];
    CreateCar(slot, cam, 0);
    MBOX_ShowObject(car->handle, 1, cam);
    MBOX_HideObject(car->parts[3], 1, cam);
    SetSelectTireSizes(slot, player);
}

/* UpdateSelectCar: direct typed port of MAT3 selectcarload.c UpdateSelectCar (GC 0x8003bf00), N64 9-argument
 * variant (r, g, b colour instead of PaintJob; SetupCarColor). */
void UpdateSelectCar(s16 slot, s16 player, u8 r, u8 g, u8 b, s16 sheenIdx, f32 *pos, f32 (*uvs)[3], u8 alpha)
{
    f32 newuvs[3][3];
    s16 idx;
    s16 cam;
    s32 x0;                 /* disclosed unused: frame residual (header) */
    tRGBA alphaCol = {255, 255, 255, 255};
    intptr_t wheelTex;
    s32 u0, u1, u2, u3, u4, u5, u6, u7, u8, u9;   /* disclosed unused: frame residual (header) */
    idx = slot % 13;
    if (gNumWindows < 3) {
        if (player == 2) player = 0;
        if (player == 3) player = 1;
    }
    switch (player) {
    case 0: cam = 1; break;
    case 1: cam = 2; break;
    case 2: cam = 4; break;
    case 3: cam = 8; break;
    }
    if (alpha == 0) {
        if (!gSelectCar[slot].hidden) {
            gSelectCar[slot].hidden = 1;
            MBOX_HideObject(gCarParts[slot].handle, 0, cam);
        } else if (gSelectCar[slot].resource != -1 && WaveInstanceReady(gSelectCar[slot].resource) && gSelectCar[slot].created) {
            UnloadSelectCar(slot);
            gSelectCar[slot].created = 0;
            gSelectCar[slot].parts_ready = 0;
            gSelectCar[slot].resource = -1;
            gSelectCar[slot].load_started = 0;
            gSelectCar[slot].appearance[0] = -1;
            gSelectCar[slot].appearance[1] = -1;
            gSelectCar[slot].appearance[2] = -1;
            gSelectCar[slot].texture = -1;
            gSelectCar[slot].front_scale = gSelectCar[slot].rear_scale = -1.0f;
            ZOID_RemoveObjectTree(gCarParts[slot].handle, 1);
            gCarParts[slot].handle = 0;
            gCarParts[slot].parts[0] = 0;
            gCarParts[slot].parts[1] = 0;
            gCarParts[slot].parts[2] = 0;
            gCarParts[slot].parts[3] = 0;
            gCarParts[slot].parts[4] = 0;
            gCarParts[slot].joints[0] = 0;
            gCarParts[slot].joints[1] = 0;
            gCarParts[slot].joints[2] = 0;
            gCarParts[slot].joints[3] = 0;
        }
    } else {
        if (!gSelectCar[slot].load_started) {
            gSelectCar[slot].load_started = LoadSelectCar(slot, idx, &gSelectCar[slot].resource);
            return;
        }
        if (!WaveInstanceReady(gSelectCar[slot].resource))
            return;
        if (!gSelectCar[slot].parts_ready) {
            gSelectCar[slot].parts_ready = 1;
            InitCarDynamicObjs(slot, idx, gCarModelNums[slot]);
        }
        if (!gSelectCar[slot].created) {
            gSelectCar[slot].created = 1;
            gSelectCar[slot].hidden = 0;
            CreateSelectCar(slot, player, cam);
            SetSheen(idx, player, sheenIdx);
        }
        if (gSelectCar[slot].hidden) {
            MBOX_ShowObject(gCarParts[slot].handle, 0, cam);
            gSelectCar[slot].hidden = 0;
        }
        if (gSelectCar[slot].appearance[0] != r || gSelectCar[slot].appearance[1] != g || gSelectCar[slot].appearance[2] != b) {
            SetupCarColor(slot, idx, r, g, b);
            gSelectCar[slot].appearance[0] = r;
            gSelectCar[slot].appearance[1] = g;
            gSelectCar[slot].appearance[2] = b;
        }
        alphaCol.c[3] = alpha;
        SetFrameColor(slot, (s32 *)&alphaCol);
        CopyUV(uvs, newuvs);
        if (gFrameScale) {
            newuvs[0][1] *= FSCLS[gFrameScale - 1];
            newuvs[1][1] *= FSCLS[gFrameScale - 1];
            newuvs[2][1] *= FSCLS[gFrameScale - 1];
        }
        ZOID_UpdateObject(gCarParts[slot].handle, pos, newuvs);
        if (gSelectCar[slot].texture != gTireRims[player + 1][idx]) {
            wheelTex = gWheelTextures[gTireRims[player + 1][idx] + 3];
            gSelectCar[slot].texture = gTireRims[player + 1][idx];
            if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6)
                ZOID_SetTexture(gCarParts[slot].joints[0], gWheelTextures[16], -1);
            else
                ZOID_SetTexture(gCarParts[slot].joints[0], wheelTex, -1);
        }
        if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6)
            ZOID_SetTexture(gCarParts[slot].joints[0], gWheelTextures[16], -1);
        if (gSelectCar[slot].front_scale != gFrontTireScale[player + 1][idx] || gSelectCar[slot].rear_scale != gRearTireScale[player + 1][idx]) {
            gSelectCar[slot].front_scale = gFrontTireScale[player + 1][idx];
            gSelectCar[slot].rear_scale = gRearTireScale[player + 1][idx];
            SetSelectTireSizes(slot, player);
        }
        ObjInstanceList[gCarParts[slot].handle].flags &= ~16;
        ObjInstanceList[gCarParts[slot].handle].flags &= ~32;
        ObjInstanceList[gCarParts[slot].handle].word4 = 0;
    }
}
