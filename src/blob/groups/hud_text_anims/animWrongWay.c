/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_80108AB0 (0x80108AB0, 760 B) = GC animWrongWay@hud.c ("WRONG WAY" text per window): gRace = gRace,
 * gNumWindows = gNumWindows, gGameCar = gGameCar (heading +868, f239 = GC +0xEF), gGameState bit 0x400000,
 * gShowWrongWay = gShowWrongWay, wrongWayTime = wrongWayTime, gSeconds = gSeconds, wrongWayPos = wrongWayPos,
 * wasStrayed = function-local static (0x8013F2F8, GC wasStrayed$1766), TB_SetJust_8AB0 pair gJust,
 * HAL_SetTextDepth = HAL_SetTextDepth, HAL_SetTextTranslucency = HAL_SetTextTranslucency, TB_GetFlashTextCycle_8AB0 = gColorCycle,
 * TB_SetTextColor = TB_SetTextColor, TB_DrawText = TB_DrawText. GC locals x, y, slot (same here).
 * draft 184/190 -> unit EQUAL. Levers:
 *  - HAL_SetFont_8AB0 nests one level deeper than the lock/font helper (HAL_SetFont_8AB0 -> font_set_8AB0 -> gfx_lock_8AB0 /
 *    font_select_8AB0 / gfx_unlock_8AB0): with font_select_8AB0 nested the callee-saved cost rises and the FP constants
 *    (+-pi/2, 2.0, 255.0, -1.0) are rematerialised per use as in retail instead of hoisted into $f20-$f26
 *    (184 -> 41 rows); the HAL_SetFont_8AB0 level supplies the last 16 B of frame (two call sites, 8 B each).
 *  - `extern volatile f32 gSeconds` (gSeconds): retail reloads it in each arm through an address register (-> 11).
 *  - the reset arm stores wrongWayTime before wasStrayed (web order: wrongWayTime s6, wasStrayed s7 as retail).
 * Shaping devices (disclosed): inlined helpers (non-static *_8AB0, not on keep: stub rule) gfx_lock_8AB0/gfx_unlock_8AB0/font_select_8AB0/font_set_8AB0/HAL_SetFont_8AB0/TB_SetJust_8AB0/
 * TB_GetFlashTextCycle_8AB0 (the GC names; GC calls HAL_SetFont_8AB0/TB_SetJust_8AB0/TB_GetFlashTextCycle_8AB0 as functions, the
 * N64 nesting depth is inferred from the frame); volatile gSeconds.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct TblHdr {
    char *pad0[0x68 / 4];
    char *text;
} TblHdr;

typedef struct StrTable {
    u8 pad0[4];
    TblHdr *hdr;
} StrTable;

typedef struct Pos {
    s32 x, y;
} Pos;

typedef struct Pair {
    s32 h, v;
} Pair;

typedef struct Car {
    u8 pad0[239];
    s8 f239;
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x364 - 0x308];
    f32 heading;
    u8 pad368[0x380 - 0x368]; void *input; u8 pad368_384[0x3B8 - 0x384];
} Car;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s32 gRace;
extern s8 gShowWrongWay;
extern Pair gJust;
extern StrTable gText;
extern s16 gNumWindows;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern f32 wrongWayTime[];
extern volatile f32 gSeconds;
extern f32 gColorCycle;
extern Pos wrongWayPos[][4];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void HAL_SetTextDepth(f32);
void HAL_SetTextTranslucency(f32);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, void *);

void gfx_lock_8AB0(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_8AB0(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_select_8AB0(s32 font)
{
    return HAL_SetFontInternal(font);
}

s32 font_set_8AB0(s32 font)
{
    s32 old;

    gfx_lock_8AB0();
    old = font_select_8AB0(font);
    gfx_unlock_8AB0();
    return old;
}

void TB_SetJust_8AB0(s32 h, s32 v)
{
    gJust.h = h;
    gJust.v = v;
}

f32 TB_GetFlashTextCycle_8AB0(void)
{
    return gColorCycle;
}

void HAL_SetFont_8AB0(s32 font)
{
    font_set_8AB0(font);
}

s32 animWrongWay(void *item)
{
    s32 x;
    s32 y;
    s32 slot;
    static s8 wasStrayed[4];

    if (gRace == 5) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust_8AB0(1, 1);
    for (slot = 0; slot < gNumWindows; slot++) {
        if ((-1.5707964f <= PC_CAR_HEADING(slot) && PC_CAR_HEADING(slot) <= 1.5707964f) ||
            !(gGameState & 0x400000) || PC_CAR_FINISHED(slot) == 1 || !gShowWrongWay || gRace == 1) {
            wrongWayTime[slot] = gSeconds;
            wasStrayed[slot] = 0;
            continue;
        }
        wasStrayed[slot] = 1;
        if (gSeconds - wrongWayTime[slot] < 2.0f) {
            continue;
        }
        if (gNumWindows == 1) {
            HAL_SetFont_8AB0(10);
        } else {
            HAL_SetFont_8AB0(11);
        }
        HAL_SetTextTranslucency(255.0f * TB_GetFlashTextCycle_8AB0());
        x = wrongWayPos[gNumWindows - 1][slot].x;
        y = wrongWayPos[gNumWindows - 1][slot].y;
        TB_SetTextColor(0);
        TB_DrawText(x + 1, y + 1, gText.hdr->text);
        TB_SetTextColor(14);
        TB_DrawText(x, y, gText.hdr->text);
    }
    TB_SetJust_8AB0(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
