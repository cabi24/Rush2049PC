/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * game_results_input (0x800FF724, 1,732 B) = animControls(struct _tBlit *blt) of controlscr.c (GC rush_d.elf
 * animControls 0x8006BC2C: string "CONTROLS_SM", same branch structure). The N64 label is wrong: it animates
 * one controls-screen button blit (data word +0x2C packs id/conflict/uninited/player/value nibbles).
 * Called only through a function pointer (no direct callers).
 * Unit EQUAL. Starting draft (157/433, colour only).
 * GC-derived: validOption takes (player, item): GC animControls passes r3 = extsb(p), r4 = gControlFirst[p] + j
 * (GC's DWARF prototype "static int validOption(void)" lost the parameters). On N64 the player parameter is u8.
 * Shaping devices (disclosed):
 *  - validOption() and Hidden() are inlined static helpers (GC has both as functions); the named local `valid`
 *    inside validOption gives the retail frame (120; 112 without it).
 *  - three compiled-out reads of the u8 player value: `if (player) {}` in validOption, and `if ((u8)p) {}` once
 *    before gTextPosScale and once after the loop. They give the hoisted (u8)p web (no code: as1 drops the dead
 *    `andi a3`) a priority of 13/5 > first's 12/5, so it takes a3 and leaves retail's t0..t5/ra colouring
 *    (first t0, blt t1, id t2, action t3, p t4, conflict t5, uninited ra). Any two of the three: 41 or 133 words.
 *  - field extraction order conflict, uninited, p, id, val (ugen temp ring t6,t7,t8,t9,t6,t7 as retail).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    uintptr_t word0, word4, word8;
    u16 half12;
    s16 x, y;
    u16 half18;
    s16 w, h;
    u8 alpha, byte25;
    s8 hidden;
    u8 byte27;
    s16 left, right, top, bottom;
    u32 word36;
    intptr_t state;     /* AnimFunc */
    u32 data;
} Blit;

typedef struct ButtonName {
    u8 pad0[48];
    f32 pos[3];
    u8 alpha;
    u8 pad3D[3];
} ButtonName;

typedef struct Player {
    u8 b0;
    u8 control;
    u8 pad2[0x48 - 0x2]; void *ref;
} Player;

typedef struct TextPos {
    s32 pad0, y, pad8, padC, x, pad14;
} TextPos;

extern s16 gControlFirst[];          /* gControlFirst */
extern s16 gControlMenuSize;            /* gControlMenuSize */
extern s16 gNumWindows;            /* gNumWindows */
extern s8 gConflict[][10];       /* gConflict */
extern s8 controlScrActive[];           /* controlScrActive */
extern Player gPlayers[];       /* gPlayers */
extern s32 controls_buttonsInited[];          /* buttonsInited */
extern ButtonName controls_buttonInfo[][17]; /* buttonNames */
extern u8 gCamera[][152];      /* gCamera */
extern f32 controls_gButtonScale;            /* gButtonScale */
extern f32 controls_buttonZ;            /* buttonZ */
extern TextPos controls_gPlayerRects[][4];
extern s32 controls_idMap_conflict;
extern s32 controls_idMap[];          /* button textures */
extern s8 gNewControls[][10];       /* gNewControls */
extern void UpdateBlit(Blit *);
extern void RenameBlit(Blit *, char *, s32);
extern void ZOID_ProjectWindow(s32, f32 *, void *, f32 *, s16 *);
extern void SelectBlit(Blit *, s32, s32, s32);

static s32 validOption(u8 player, s32 item)
{
    s32 valid;

    valid = 1;
    if (player) {}
    if (item == 8) {
        valid = 0;
    }
    return valid;
}

s32 Hidden(Blit *blt, s32 hide);

s32 animControls(Blit *blt)
{
    s32 id;
    s32 conflict;
    s32 uninited;
    s32 p;
    s32 val;
    s32 action;
    s16 i;
    s16 j;
    s16 screenPos[2];
    f32 gTextPosScale;

    conflict = (blt->data >> 4) & 15;
    uninited = (blt->data >> 8) & 15;
    p = (blt->data >> 12) & 15;
    id = blt->data & 15;
    val = ((blt->data >> 16) & 255) - 1;
    action = gControlFirst[p] + id - 1;
    if ((u8)p) {}
    gTextPosScale = controls_gButtonScale;
    if (gNumWindows > 1) {
        gTextPosScale *= 0.5f;
    }
    for (i = 0, j = 0; i < id; j++) {
        if (!validOption(p, gControlFirst[p] + j)) {
            action++;
        } else {
            i++;
        }
    }
    if ((u8)p) {}
    if (Hidden(blt, gPlayers[p].control == 5 || (conflict && !gConflict[p][action]) || action >= 10 ||
                        !controlScrActive[p] || gControlMenuSize < id)) {
        return 1;
    }
    if (uninited) {
        if (gNumWindows > 1) {
            RenameBlit(blt, "CONTROLS_SM", 0);
        }
        if (controls_buttonsInited[p]) {
            ZOID_ProjectWindow(p, controls_buttonInfo[p][action].pos, gCamera[p], 0, screenPos);
            blt->x = screenPos[0] + 55.0f * gTextPosScale * 300.0f / controls_buttonZ;
            blt->y = screenPos[1] - blt->h / 2;
            blt->alpha = controls_buttonInfo[p][action].alpha;
        } else {
            blt->x = controls_gPlayerRects[gNumWindows - 1][p].x + 20;
            blt->y = controls_gPlayerRects[gNumWindows - 1][p].y + id * 16 + 8;
        }
        if (conflict) {
            blt->x += (gNumWindows == 1) ? 20 : 10;
        }
        UpdateBlit(blt);
        blt->data &= 0x00FFF0FF;
        if (conflict) {
            blt->data = (blt->data & 0xFFFF) | 0x1C0000;
            SelectBlit(blt, controls_idMap_conflict, blt->h, blt->h);
        }
    }
    if (controls_buttonsInited[p]) {
        ZOID_ProjectWindow(p, controls_buttonInfo[p][action].pos, gCamera[p], 0, screenPos);
        blt->x = screenPos[0] + 75.0f * gTextPosScale * 300.0f / controls_buttonZ;
        blt->y = screenPos[1] - blt->h / 2;
        blt->half18 = 0x7F00;
        if (conflict) {
            blt->x += (gNumWindows == 1) ? 20 : 10;
        }
        if (controls_buttonInfo[p][action].alpha >= 255) {
            blt->alpha = 254;
        } else {
            blt->alpha = controls_buttonInfo[p][action].alpha;
        }
        if (Hidden(blt, blt->alpha == 0)) {
            return 1;
        }
        UpdateBlit(blt);
    } else {
        blt->alpha = 254;
        UpdateBlit(blt);
    }
    if (conflict) {
        return 1;
    }
    if (val != gNewControls[gPlayers[p].control][action]) {
        val = gNewControls[gPlayers[p].control][action];
        blt->data = (blt->data & 0xFFFF) | (((val + 1) << 16) & 0xFF0000);
        if (val == 25 && (action == 0 || action == 1)) {
            val++;
        }
        SelectBlit(blt, controls_idMap[val], blt->h, blt->h);
    }
    return 1;
}
