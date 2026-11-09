/* animPowerupIcons, animWeaponIcons and UpdateHUDExtraPostCamera: the battle HUD's 3D icons (MAT3 hudextra.c;
 * runtime image B). */
#include "game/game.h"

typedef struct Status { s16 kind, handle; f32 uv[3][3], position[3]; } Status;
typedef struct Handle { s16 handle, unknown2; f32 uv[3][3], position[3]; } Handle;
typedef struct Camera { f32 uv[3][3], position[3]; u8 unknown30[104]; } Camera;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Scene { u32 flags, w4; void *obj; f32 scaleA; f32 scale; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Scene;
typedef struct Point { s16 x, y; } Point;
typedef union Color { u32 word; struct { u8 r,g,b,a; } rgba; } Color;
/* PC: the fade's alpha passes 255 while a score's start is ahead of the clock; a float out of a u8's range is
 * undefined in C, so it goes through an int and keeps its low byte, as the N64's trunc.w.s and sb do
 * (docs/port/SANITIZE.md). */
#define ALPHA_U8 (u8)(s32)

typedef struct ScoreState {
    s16 active, lock;
    f32 started;
    s32 unknown08, score, shown_score, total_score, working;
    u8 unknown1C[4];
    s16 source_counts[10], counts[10];
    u8 unknown48[29];
    s8 message_kind;
    u8 unknown66;
    s8 message_index;
    u8 unknown68[4];
    s32 multiplier, pending, state;
} ScoreState;
typedef struct Effect { s16 state, handle; f32 uv[3][3], position[3], alpha; s16 x,y,from_x,from_y,to_x,to_y; f32 timer; } Effect;

VIEW(Status[], weaponIcon);
#define weaponIcon weaponIcon_view
VIEW(Status[], powerupIcon);
#define powerupIcon powerupIcon_view
VIEW(Handle[], battleCoin);
#define battleCoin battleCoin_view
VIEW(Camera[], gCamera);
#define gCamera gCamera_view
VIEW(Scene[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(Point[4][4], powerupPos);
#define powerupPos powerupPos_view
VIEW(Point[4][4], ammoPos);
#define ammoPos ammoPos_view
VIEW(s32[4][4][2], fragPos);
#define fragPos fragPos_view
VIEW(s32[4][4][2], stuntCoinPos);
#define stuntCoinPos stuntCoinPos_view
VIEW(s32[4][4][2], wrongWayPos);
#define wrongWayPos wrongWayPos_view
VIEW(Color, weapon_icon_color);
#define weapon_icon_color weapon_icon_color_view
VIEW(Color[], hudextra_playerColor);
#define hudextra_playerColor hudextra_playerColor_view
VIEW(Color, stunt_coin_color);
#define stunt_coin_color stunt_coin_color_view

VIEW(ScoreState[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view
VIEW(Effect[4][10], stuntCoin);
#define stuntCoin stuntCoin_view

/* Shows each window's powerup icon, a 3D object turning by hudObjAngle at powerupPos, for the powerup its car has
 * (made or swapped when that changes, removed when none). Returns 1. */
s32 animPowerupIcons(void *unused)
{
    Point point;
    s32 kind, i;
    Status *status;
    for (i = 0; i < gNumWindows; i++) {
        if (PC_CAR_BATTLE_FLAGS(i)) {
            if (PC_CAR_BATTLE_FLAGS(i) & 1) kind = 1;
            else if (PC_CAR_BATTLE_FLAGS(i) & 2) kind = 2;
        } else {
            kind = 0;
        }
        status = &powerupIcon[i];
        if (kind != status->kind) {
            if (status->handle != -1) {
                ZOID_RemoveObject(status->handle);
                status->handle = -1;
            }
            status->kind = kind;
        }
        if (status->kind && status->handle == -1) {
            status->handle = MBOX_NewObject(
                MBOX_FindObject_WORK(powerupIconNames[status->kind], 0, *(u8 *)(uintptr_t)&gNumModels - 1, 1),
                powerupIcon[i].uv, -1, 0);
            MBOX_HideObject(status->handle, 1, 15);
            MBOX_ShowObject(status->handle, 0, hudextra_whichCam[i]);
            ObjInstanceList[status->handle].scale = 1.5f;
        }
        ScaleUV(gCamera[i].uv, powerupIcon[i].uv, 0.025f);
        YawUV(hudObjAngle, powerupIcon[i].uv);
        point.x = powerupPos[gNumWindows - 1][i].x;
        point.y = powerupPos[gNumWindows - 1][i].y;
        ZOID_WindowTo3D(i, &point.x, &gCamera[i], 2.0f, status->position);
    }
    return 1;
}
/* The same for each window's weapon icon (weaponPos), the weapon its car carries. Returns 1. */
s32 animWeaponIcons(void *unused)
{
    Point point;
    s32 i, kind, resource;
    Color color;
    s32 player_kind;
    for (i = 0; i < gNumWindows; i++) {
        player_kind = PC_CAR_KIND(i);
        kind = weaponIcon[i].kind;
        if (player_kind != kind) {
            if (weaponIcon[i].handle != -1) {
                ZOID_RemoveObject(weaponIcon[i].handle);
                weaponIcon[i].handle = -1;
                player_kind = PC_CAR_KIND(i);
            }
            weaponIcon[i].kind = player_kind;
            kind = weaponIcon[i].kind;
        }
        if (kind != 8 && weaponIcon[i].handle == -1) {
            color.word = weapon_icon_color.word;
            switch (kind) {
            case 0: resource = 216; break;
            case 1: resource = 217; break;
            case 2: resource = 218; break;
            case 3: resource = 219; break;
            case 4: resource = 220; break;
            case 5: resource = 221; break;
            case 6: resource = 222; break;
            case 7: resource = 223; break;
            }
            weaponIcon[i].handle = MBOX_NewObject(gObjList[resource], weaponIcon[i].uv, -1, 0x42000);
            ZOID_SetColor(weaponIcon[i].handle, &color.word);
            ObjInstanceList[weaponIcon[i].handle].scale = 1.5f;
            MBOX_HideObject(weaponIcon[i].handle, 1, 15);
            MBOX_ShowObject(weaponIcon[i].handle, 0, hudextra_whichCam[i]);
            kind = weaponIcon[i].kind;
        }
        if (kind == 7) ScaleUV(gCamera[i].uv, weaponIcon[i].uv, 0.175f);
        else ScaleUV(gCamera[i].uv, weaponIcon[i].uv, 0.069f);
        YawUV(hudObjAngle, weaponIcon[i].uv);
        point.x = ammoPos[gNumWindows - 1][i].x;
        point.y = ammoPos[gNumWindows - 1][i].y;
        if (weaponIcon[i].kind == 7) ZOID_WindowTo3D(i, &point.x, &gCamera[i], 5.75f, weaponIcon[i].position);
        else ZOID_WindowTo3D(i, &point.x, &gCamera[i], 2.0f, weaponIcon[i].position);
    }
    return 1;
}

/* After the camera has moved: turns the extras' 3D icons and coins (UpdateHUDPostCamera calls it in battle and stunt
 * races). */
void UpdateHUDExtraPostCamera(void)
{
    s32 i, j, kind, y_offset;
    Point point;
    NameQuad names;
    Color color;
    Color player_color;
    char name[32];
    Handle *handle;
    ScoreState *score;
    Effect *effect, *selected, *previous;
    f32 alpha, fraction;

    if (gRace == 6) {
        animWeaponIcons(0);
        animPowerupIcons(0);
        for (i = 0; i < gNumWindows; i++) {
            handle = &battleCoin[i];
            if (handle->handle == -1) {
                names = battle_coin_object_names;
                player_color.rgba.r = hudextra_playerColor[gCarTeam[i]].rgba.r;
                player_color.rgba.g = hudextra_playerColor[gCarTeam[i]].rgba.g;
                player_color.rgba.b = hudextra_playerColor[gCarTeam[i]].rgba.b;
                player_color.rgba.a = 255;
                handle->handle = MBOX_NewObject(
                    MBOX_FindObject_WORK(names.name[gCarTeam[i]], 0, *(u8 *)(uintptr_t)&gNumModels - 1, 1),
                    handle->uv, -1, 0);
                ObjInstanceList[handle->handle].scale = 1.5f;
                MBOX_HideObject(handle->handle, 1, 15);
                MBOX_ShowObject(handle->handle, 0, hudextra_whichCam[i]);
            }
            ScaleUV(gCamera[i].uv, handle->uv, gNumPlayers >= 3 ? 0.012f : 0.009f);
            PitchUV(-1.5707963267948966f, handle->uv);
            point.x = fragPos[gNumWindows - 1][i][0];
            point.y = fragPos[gNumWindows - 1][i][1];
            ZOID_WindowTo3D(i, &point.x, &gCamera[i], 2.0f, handle->position);
        }
    }
    if (gRace == 4) {
        color.word = stunt_coin_color.word;
        if ((gGameState & 0x7C03FFFE) || gPauseState) return;
        for (i = 0; i < gNumPlayers; i++) {
            score = &gStuntPlayer[i];
            if (score->lock) alpha = (1.0f - (gModelTime - score->started) / 2.69f) * 255.0f;
            else alpha = 255.0f;
            if (alpha < 0.0f) alpha = 0.0f;
            color.rgba.a = ALPHA_U8 alpha;
            if (score->active && (gModelTime - score->started < 2.69f || score->working)) {
                for (kind = 0; kind < 10; kind++) {
                    if (!score->state) score->counts[kind] = score->source_counts[kind];
                    if (score->counts[kind]) {
                        for (j = 0; j < 10; j++) {
                            effect = &stuntCoin[i][j];
                            selected = effect;
                            if (effect->state == kind) break;
                            if (effect->state == 11) {
                                TB_Sprintf(name, stunt_coin_object_format, stuntCoinTexNames[kind]);
                                if (effect->handle != -1) {
                                    ZOID_SetObjectDef(effect->handle, (u16)MBOX_FindObject_WORK(name, 0, *(u8 *)(uintptr_t)&gNumModels - 1, 1));
                                } else {
                                    effect->handle = MBOX_NewObject(
                                        MBOX_FindObject_WORK(name, 0, *(u8 *)(uintptr_t)&gNumModels - 1, 1), effect->uv, -1, 0x42000);
                                }
                                ObjInstanceList[effect->handle].scale = 1.5f;
                                effect->state = kind;
                                effect->timer = 0.5f;
                                y_offset = gNumWindows == 1 ? -28 : gNumWindows == 2 ? -16 : -12;
                                selected->from_x = wrongWayPos[gNumWindows - 1][i][0];
                                selected->from_y = wrongWayPos[gNumWindows - 1][i][1] + y_offset;
                                selected->to_x = stuntCoinPos[gNumWindows - 1][i][0];
                                selected->to_y = stuntCoinPos[gNumWindows - 1][i][1] + y_offset;
                                for (j--; j >= 0; j--) {
                                    previous = &stuntCoin[i][j];
                                    previous->from_x = previous->x;
                                    previous->from_y = previous->y;
                                    previous->timer = 0.5f;
                                    y_offset -= gNumWindows == 1 ? 18 : 8;
                                    previous->to_x = stuntCoinPos[gNumWindows - 1][i][0];
                                    previous->to_y = stuntCoinPos[gNumWindows - 1][i][1] + y_offset;
                                }
                                break;
                            }
                        }
                        if (selected->handle != -1) {
                            selected->timer -= gFrameSeconds;
                            if (selected->timer < 0.0f) selected->timer = 0.0f;
                            fraction = selected->timer / 0.5f;
                            selected->x = (selected->from_x - selected->to_x) * fraction + selected->to_x;
                            selected->y = (selected->from_y - selected->to_y) * fraction + selected->to_y;
                            ZOID_WindowTo3D(i, &selected->x, &gCamera[i], 2.0f, selected->position);
                            ScaleUV(gCamera[i].uv, selected->uv,
                                gNumWindows == 1 ? 0.005f : gNumWindows == 2 ? 0.0035f : 0.005f);
                            selected->alpha = alpha;
                            color.rgba.a = ALPHA_U8 alpha;
                            ZOID_SetColor(selected->handle, &color.word);
                            MBOX_ShowObject(selected->handle, 0, hudextra_whichCam[i]);
                        }
                    }
                }
            } else if (!score->working) {
                score->active = 0;
                score->lock = 0;
                score->multiplier = 1;
                score->message_index = 0;
                score->message_kind = 0;
                score->shown_score = 0;
                for (j = 0; j < 10; j++) {
                    score->counts[j] = 0;
                    selected = &stuntCoin[i][j];
                    if (selected->handle != -1) {
                        ZOID_RemoveObject(selected->handle);
                        selected->handle = -1;
                    }
                    selected->state = 11;
                }
            }
        }
    }
}
