/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animRadar: per-player save_player_car_view/HUD node update.  Hides the node (byte +0x1A = 1,
 * word +0x28 = 0) when its player index is out of range, gGameState bit 3 is set or
 * gNumCars < 2; otherwise hidden = !gShowRadar || player[index].state(+0x17) == 1,
 * re-applying UpdateBlit when the flag changes.  A visible node takes its x/y
 * from the 4-entry row radarPos[count - 1][index] (count = gNumWindows), gets
 * RenameBlit(node, radar_md_blit_name, 0) and alpha 0x60 in multi-player, and publishes its
 * size (+0x14/+0x16) to radarWidth/radarHeight.  Returns the hidden flag / 1.
 * Also matches at -O2.  Whole-program unit: EQUAL.
 * Shaping quirk: gNumCars is `volatile` (retail reads it through `la; lh 0(reg)`,
 * like the other readers of this word, e.g. fake_init's store).  Body otherwise from
 * the heads_B13 structmode draft.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32;

/* PC: the node is a blit (NewMultiBlit.c's Blit, 0x68 bytes, docs/port/POINTERS.md 9.3): its name, image and info
 * are pointers, so the rest is 0x0C later, and the AnimFunc slot (word28) is pointer-sized at +0x38 with the AnimID
 * (the player index) after it at +0x40. */
typedef struct MenuNode {
    void *next;         /* 0x00 */
    void *image;        /* 0x08 */
    void *info;         /* 0x10 */
    s16 padC;           /* 0x18 */
    s16 x;              /* 0x1A */
    s16 y;              /* 0x1C */
    s16 pad12;          /* 0x1E */
    s16 w;              /* 0x20 */
    s16 h;              /* 0x22 */
    s8 alpha;           /* 0x24 */
    s8 pad19;
    s8 hidden;          /* 0x26 */
    s8 pad1B;
    s32 pad1C, pad20, pad24;
    intptr_t word28;    /* 0x38 */
    s32 index;          /* 0x40: player index */
} MenuNode;

typedef struct { s32 x, y; } Pos;
/* PC: a car record seen from +0xD8 (a name inside gGameCar): only its stride, PC_CAR_SIZE, matters here. */
typedef struct { u8 pad[0x17]; s8 state; u8 pad18[PC_CAR_SIZE - 0x18]; } Player;

extern s16 gNumWindows;                  /* player count */
extern volatile s16 gNumCars;
extern s8 gShowRadar;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s32 gGameState, radarWidth, radarHeight;
extern Pos radarPos[][4];     /* row = player count - 1 */
extern char radar_md_blit_name[];
extern void UpdateBlit(void *);
extern void RenameBlit(void *, char *, s32);

s32 animRadar(MenuNode *node) {
    s32 index;
    s32 hidden;

    index = node->index;
    if (index >= gNumWindows || (gGameState & 8) || gNumCars < 2) {
        node->word28 = 0;
        if (node->hidden != 1) {
            node->hidden = 1;
            UpdateBlit(node);
        }
        return node->hidden;
    }
    hidden = gShowRadar == 0;
    if (hidden == 0) {
        hidden = PC_CAR_FINISHED(index) == 1;
    }
    if (hidden != node->hidden) {
        node->hidden = hidden;
        UpdateBlit(node);
    }
    if (node->hidden != 0) {
        return 1;
    }
    node->x = radarPos[gNumWindows - 1][index].x;
    node->y = radarPos[gNumWindows - 1][index].y;
    if (gNumWindows >= 2) {
        RenameBlit(node, radar_md_blit_name, 0);
        node->alpha = 0x60;
    }
    UpdateBlit(node);
    radarWidth = node->w;
    radarHeight = node->h;
    return 1;
}
