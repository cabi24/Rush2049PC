/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animCoins (0x800B7A40, 1,464 B; historical label) = MAT3 animCoins@hud.c (GC 0x8001FC2C) with the
 * per-coin "%d/8" text of animCoinsText@hud.c (GC 0x800204C8) folded into the coin loop, as on the N64 (the
 * N64 function sits in the NumCoinsFound/CountBits translation unit, after NumCoinsFound).
 * Base: particles_update/lead_area128.c (body). changes:
 * - `extern volatile u8 gNumModels` (gNumModels, volatile): retail builds its address
 *    (lui/addiu, lbu 0(reg)) -- 262 -> 10 words;
 *  - MAT3 operand order for x/y (`table + j * spacing`, `table + (n == 1 ? 12 : 8)`) -- 10 -> 0 code words;
 *  - the three strings as their retail data symbols (as animRadar.c's radar_md_blit_name, the next string):
 *    string literals would put own .rodata in two retail regions and make the names[] initializer point at
 *    own .rodata, which the unit/group own-data proofs cannot place;
 *  - HAL_SetFont/_GetSemaphore/_ReleaseSemaphore and TB_SetJust inlined as non-static helpers after the
 * caller (ShowHUD/animGear shape; _ReleaseSemaphore and HAL_SetFont on one source line).
 * Frame (disclosed shaping): retail frame 256 = cfe named area 128 + 6 spill homes + saves. Without the two
 * items below the named area is 104 (frame 232). +8: `f32 xoffset, yoffset;` (MAT3 animCoins DWARF locals,
 * unused in the N64 body) in the text block; +16: one extra inlined wrapper level at two call sites, written
 * here as HAL_SetTextDepth_Coins around HAL_SetTextDepth (= HAL_SetTextDepth). Which two-site helper carries
 * that level is not determinable from the bytes: an extra level on HAL_SetFont or TB_SetJust gives the same
 * 128 and the same code.
 * Callee identities: HAL_SetTextDepth, HAL_SetFontInternal = _HAL_SetFont (s2
 * parameter), MBOX_FindObject_WORK, ZOID_AddObject = MBOX_NewObject, MBOX_HideObject =
 * MBOX_HideObject, MBOX_ShowObject, ScaleUV, YawUV,
 * ZOID_WindowTo3D, NumCoinsFound, TB_Sprintf,
 * TB_SetTextColor, TB_DrawText; gPlayerCoin,
 * gCamera, ObjInstanceListAlias = ObjInstanceList, hudObjAngle, coinStuntPos
 * (image A), coinPos, gOptions_show_coins = gOptions[11], gGameCar[].mode = place_locked,
 * hud_whichCam = whichCam, gJust, text_lock_queue = font semaphore queue.
 * gJust is a tentative definition here (as hud_text_anims/audio_animPanes_demo_animText.c and countdown): with an extern the
 * group compile gives the TB_SetJust(0, 1) stores two `lui at`; the unit needs an extern_data override.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct RecordSlot { s16 id; u8 pad02[2]; f32 pos[9]; u8 obj[12]; } RecordSlot;   /* 52 bytes; +4 transform, +40 */
typedef struct RecordRow { RecordSlot slot[2]; } RecordRow;                                /* 104 bytes */
typedef struct Cam152 { u8 pad[152]; } Cam152;
typedef struct Car952 { u8 pad000[239]; s8 mode; u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad0F0_308[0x380 - 0x308]; void *input; u8 pad0F0_384[0x3B8 - 0x384]; } Car952;
typedef struct Pair { s32 first, second; } Pair;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * ObjInstanceListAlias is node 0 + 0x10: on the PC the field is the node's, at host +0x14. */
typedef struct Model68 { u32 flags, w4; void *obj; f32 w; f32 scale; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Model68;
#define ObjInstanceListAlias ObjInstanceList
typedef struct Pos { s16 x, y; } Pos;

extern s16 gNumWindows;
extern s16 gNumPlayers;
extern s32 gRace;
extern Pair gJust;   /* tentative definition (as hud_text_anims/audio_animPanes_demo_animText.c): see header */
extern RecordRow gPlayerCoin[];
extern Cam152 gCamera[];
extern volatile u8 gNumModels;
extern Model68 ObjInstanceListAlias[];
extern f32 hudObjAngle;
extern s32 coinStuntPos[][4][2];
extern s32 coinPos[][4][2];
extern s8 gOptions_show_coins;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s32 hud_whichCam[];
extern u8 text_lock_queue[];
extern char coin_gold_name[], coin_silver_name[]; /* "GOLDCOING_COIN", "SILVERCOINS_COI" */
extern char coin_count_format[];               /* "%d/8" */

void HAL_SetTextDepth(f32 arg0);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, char *);
s32 MBOX_FindObject_WORK(char *arg0, s8 arg1, s8 arg2, s32 arg3);
s32 ZOID_AddObject(s32 arg0, void *arg1, s16 arg2, s32 arg3);
void MBOX_HideObject(s16 arg0, s32 arg1, s32 arg2);
void ScaleUV(void *arg0, void *arg1, f32 arg2);
void YawUV(f32 arg0, void *arg1);
void ZOID_WindowTo3D(s32 arg0, void *arg1, void *arg2, f32 arg3, void *arg4);
void MBOX_ShowObject(s16 arg0, s32 arg1, s32 arg2);
s32 NumCoinsFound(s32 arg0, s32 arg1);
void TB_Sprintf(char *buf, char *fmt, ...);
extern s32 osRecvMesg(void *, void **, s32);
extern s32 osSendMesg(void *, void *, s32);
s32 HAL_SetFontInternal(s32 fontID);

typedef struct OSMesgQueue OSMesgQueue;
void _GetSemaphore_Coins(void);
void _ReleaseSemaphore_Coins(void);
s32 HAL_SetFont_Coins(s32 fontID);
void TB_SetJust_Coins(s32 h, s32 v);
void HAL_SetTextDepth_Coins(f32 d);

s32 animCoins(void *arg0) {
    Pos pos;
    char buf[8];
    s16 i;
    s16 j;

    HAL_SetTextDepth_Coins(0.0f);
    if (gNumWindows == 1) {
        HAL_SetFont_Coins(10);
    } else {
        HAL_SetFont_Coins(12);
    }
    TB_SetJust_Coins(0, 1);
    for (i = 0; i < gNumWindows; i++) {
        for (j = 0; j < 2; j++) {
            if (gPlayerCoin[i].slot[j].id == -1) {
                char *names[2] = { coin_gold_name, coin_silver_name };
                gPlayerCoin[i].slot[j].id = ZOID_AddObject(MBOX_FindObject_WORK(names[j], 0, gNumModels - 1, 1), gPlayerCoin[i].slot[j].pos, -1, 0);
                MBOX_HideObject(gPlayerCoin[i].slot[j].id, 1, 15);
                ObjInstanceListAlias[gPlayerCoin[i].slot[j].id].scale = 10.0f;
            }
            ScaleUV(&gCamera[i], gPlayerCoin[i].slot[j].pos,
                          (gNumPlayers == 1) ? 0.03f : (gNumPlayers == 2) ? 0.03f : 0.04f);
            YawUV(hudObjAngle, gPlayerCoin[i].slot[j].pos);
            if (gRace == 4) {
                pos.x = coinStuntPos[gNumWindows - 1][i][0];
                pos.y = coinStuntPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
            } else {
                pos.x = coinPos[gNumWindows - 1][i][0];
                pos.y = coinPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
            }
            ZOID_WindowTo3D(i, &pos, &gCamera[i], 2.0f, gPlayerCoin[i].slot[j].obj);
            if (gOptions_show_coins && PC_CAR_FINISHED(i) != 1) {
                s16 x;
                s16 y;
                f32 xoffset, yoffset;

                MBOX_ShowObject(gPlayerCoin[i].slot[j].id, 0, hud_whichCam[i]);
                if (gRace == 4) {
                    x = coinStuntPos[gNumWindows - 1][i][0] + ((gNumWindows == 1) ? 12 : 8);
                    y = coinStuntPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
                } else {
                    x = coinPos[gNumWindows - 1][i][0] + ((gNumWindows == 1) ? 12 : 8);
                    y = coinPos[gNumWindows - 1][i][1] + j * ((gNumWindows == 1) ? 15 : (gNumWindows == 2) ? 12 : 8);
                }
                TB_Sprintf(buf, coin_count_format, NumCoinsFound(i, j + 1));
                TB_SetTextColor(0);
                TB_DrawText(x + 1, y + 1, buf);
                TB_SetTextColor(1);
                TB_DrawText(x, y, buf);
            } else {
                MBOX_HideObject(gPlayerCoin[i].slot[j].id, 1, 15);
            }
        }
    }
    TB_SetJust_Coins(0, 3);
    HAL_SetTextDepth_Coins(-1.0f);
    return 1;
}

void _GetSemaphore_Coins(void) { osRecvMesg(text_lock_queue, (void **)0, 1); }
void _ReleaseSemaphore_Coins(void) { osSendMesg(text_lock_queue, (void *)0, 0); } s32 HAL_SetFont_Coins(s32 fontID) { s32 old; _GetSemaphore_Coins(); old = HAL_SetFontInternal(fontID); _ReleaseSemaphore_Coins(); return old; }

void TB_SetJust_Coins(s32 h, s32 v)
{
    gJust.first = h;
    gJust.second = v;
}

void HAL_SetTextDepth_Coins(f32 d) { HAL_SetTextDepth(d); }
