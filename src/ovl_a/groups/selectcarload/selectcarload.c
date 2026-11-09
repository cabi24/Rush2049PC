/* The car select screens' 3D cars (MAT3 selectcarload.c): each car shown on the car select screen (and the
 * circuit's review) is a full car model loaded in the background on demand, put in one player's window, coloured,
 * with its rims and tyre sizes, and unloaded again when it fades out. gSelectCar keeps one record per player and
 * car (52), gCarParts its scene objects. */
#include "game/game.h"

typedef union tRGBA {u8 c[4];u32 rgba;} tRGBA;      /* MAT3 rush_types.h */
/* A car's scene objects (gCarParts, MAT3 tCarParts; the N64 has its own order): the body, its parts, the tyres. */
typedef struct ModelState {
    s32 handle,parts[5];    /* the body (MAT3 frame) and its parts; parts[3] stays hidden here */
    u8 unaccessed24[4];
    s32 tires[4];           /* 0x1C the tyres: front left, front right, rear left, rear right */
    u8 unaccessed44[20];
} ModelState;
typedef struct Transform {f32 matrix[3][3],position[3];} Transform;
/* A car model's description (gCarModels, MAT3 _tCar): only the tyres' places. */
typedef struct ModelDescription {u8 unaccessed[112];f32 tireR[4][3];} ModelDescription;
VIEW(ModelState[], gCarParts);
#define gCarParts gCarParts_view
VIEW(ModelDescription *[13], gCarModels);
#define gCarModels gCarModels_view

VIEW(s8[5][13], gTireRims);
#define gTireRims gTireRims_view
VIEW(intptr_t[], gWheelTextures);
#define gWheelTextures gWheelTextures_view
VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view

extern intptr_t ZOID_FindMatrix(s16);

extern void ZOID_SetTexture(s16,intptr_t,s32);

#define CARS 13                     /* the roster; a record's car is its slot % 13 */
#define RIM_TEXTURES 3              /* gWheelTextures[]: the rims' textures from here on (gTireRims + this) */
#define RIM_TEXTURE_SMALL_MEMORY 16 /* the one rim texture used without an Expansion Pak on the first six tracks */
#define TIRE_LIFT 1.25f             /* the tyres sit this much higher than the model's places for them */
#define OBJ_FLAG_16 16              /* ObjInstanceList[].flags cleared each frame (MAT3 the same) */
#define OBJ_FLAG_32 32

/* Place and size car record id's four tyres for player (whose car settings are row player + 1 of the tyre-scale
 * tables; row 0 holds the defaults): each at its model's place, lifted a little, scaled by the front or rear scale
 * (unscaled at 1). */
void SetSelectTireSizes(s16 id,s16 player)
{
    s16 model = id % CARS;
    s16 i;
    Transform *transform;
    ModelDescription *description;
    description = gCarModels[model]; for (i = 0; i < 4; i++) {
        transform = (Transform *)ZOID_FindMatrix((s16)gCarParts[id].tires[i]);
        transform->position[0] = description->tireR[i][0];
        transform->position[1] = description->tireR[i][1] + TIRE_LIFT;
        transform->position[2] = description->tireR[i][2];
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

/* Create car record slot's scene objects (CreateCar) shown only in window mask cam, hide its part 3 and size its
 * tyres for player. */
void CreateSelectCar(s16 slot, s16 player, s32 cam)
{
    ModelState *car = &gCarParts[slot];
    CreateCar(slot, cam, 0);
    MBOX_ShowObject(car->handle, 1, cam);
    MBOX_HideObject(car->parts[3], 1, cam);
    SetSelectTireSizes(slot, player);
}

/* One frame of car record slot in player's window, at pos and uvs with alpha (0: fading out). The N64's form takes
 * the paint as three colour choices r, g, b (SetupCarColor) where MAT3 takes a paint job.
 * - alpha 0: hide the car; on a later frame, once its model has loaded, unload it and remove its objects.
 * - Else: start loading its model (and return), wait for it, set up its parts, create its objects and its sheen,
 *   show it; recolour it when its colours change; set its alpha, its place (the body stretched by the frame scale
 *   cheat), its rims when they change (one small texture without an Expansion Pak on tracks 0-5) and its tyre
 *   sizes when they change.
 * With fewer than three windows players 2 and 3 use the windows of players 0 and 1. */
void UpdateSelectCar(s16 slot, s16 player, u8 r, u8 g, u8 b, s16 sheenIdx, f32 *pos, f32 (*uvs)[3], u8 alpha)
{
    f32 newuvs[3][3];
    s16 idx;
    s16 cam;
    tRGBA alphaCol = {255, 255, 255, 255};
    intptr_t wheelTex;
    idx = slot % CARS;
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
        } else if (gSelectCar[slot].waveInst != -1 && WaveInstanceReady(gSelectCar[slot].waveInst) && gSelectCar[slot].created) {
            UnloadSelectCar(slot);
            gSelectCar[slot].created = 0;
            gSelectCar[slot].ready = 0;
            gSelectCar[slot].waveInst = -1;
            gSelectCar[slot].loading = 0;
            gSelectCar[slot].color[0] = -1;
            gSelectCar[slot].color[1] = -1;
            gSelectCar[slot].color[2] = -1;
            gSelectCar[slot].rims = -1;
            gSelectCar[slot].frontTireScale = gSelectCar[slot].rearTireScale = -1.0f;
            ZOID_RemoveObjectTree(gCarParts[slot].handle, 1);
            gCarParts[slot].handle = 0;
            gCarParts[slot].parts[0] = 0;
            gCarParts[slot].parts[1] = 0;
            gCarParts[slot].parts[2] = 0;
            gCarParts[slot].parts[3] = 0;
            gCarParts[slot].parts[4] = 0;
            gCarParts[slot].tires[0] = 0;
            gCarParts[slot].tires[1] = 0;
            gCarParts[slot].tires[2] = 0;
            gCarParts[slot].tires[3] = 0;
        }
    } else {
        if (!gSelectCar[slot].loading) {
            gSelectCar[slot].loading = LoadSelectCar(slot, idx, &gSelectCar[slot].waveInst);
            return;
        }
        if (!WaveInstanceReady(gSelectCar[slot].waveInst))
            return;
        if (!gSelectCar[slot].ready) {
            gSelectCar[slot].ready = 1;
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
        if (gSelectCar[slot].color[0] != r || gSelectCar[slot].color[1] != g || gSelectCar[slot].color[2] != b) {
            SetupCarColor(slot, idx, r, g, b);
            gSelectCar[slot].color[0] = r;
            gSelectCar[slot].color[1] = g;
            gSelectCar[slot].color[2] = b;
        }
        alphaCol.c[3] = alpha;
        SetFrameColor(slot, (s32 *)&alphaCol);
        CopyUV(uvs, newuvs);
        if (gFrameScale) {
            newuvs[0][1] *= FSCLS.fscls[gFrameScale - 1];
            newuvs[1][1] *= FSCLS.fscls[gFrameScale - 1];
            newuvs[2][1] *= FSCLS.fscls[gFrameScale - 1];
        }
        ZOID_UpdateObject(gCarParts[slot].handle, pos, newuvs);
        if (gSelectCar[slot].rims != gTireRims[player + 1][idx]) {
            wheelTex = gWheelTextures[gTireRims[player + 1][idx] + RIM_TEXTURES];
            gSelectCar[slot].rims = gTireRims[player + 1][idx];
            if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6)
                ZOID_SetTexture(gCarParts[slot].tires[0], gWheelTextures[RIM_TEXTURE_SMALL_MEMORY], -1);
            else
                ZOID_SetTexture(gCarParts[slot].tires[0], wheelTex, -1);
        }
        if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6)
            ZOID_SetTexture(gCarParts[slot].tires[0], gWheelTextures[RIM_TEXTURE_SMALL_MEMORY], -1);
        if (gSelectCar[slot].frontTireScale != gFrontTireScale[player + 1][idx] || gSelectCar[slot].rearTireScale != gRearTireScale[player + 1][idx]) {
            gSelectCar[slot].frontTireScale = gFrontTireScale[player + 1][idx];
            gSelectCar[slot].rearTireScale = gRearTireScale[player + 1][idx];
            SetSelectTireSizes(slot, player);
        }
        ObjInstanceList[gCarParts[slot].handle].flags &= ~OBJ_FLAG_16;
        ObjInstanceList[gCarParts[slot].handle].flags &= ~OBJ_FLAG_32;
        ObjInstanceList[gCarParts[slot].handle].culling = 0;
    }
}
