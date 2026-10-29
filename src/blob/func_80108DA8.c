/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_80108DA8: per-player menu/HUD node update.  Hides the node (byte +0x1A = 1,
 * word +0x28 = 0) when its player index is out of range, state_word_a bit 3 is set or
 * D_801543CA < 2; otherwise hidden = !D_80156CE8 || player[index].state(+0x17) == 1,
 * re-applying Input_ApplyPadConfig when the flag changes.  A visible node takes its x/y
 * from the 4-entry row D_80116028[count - 1][index] (count = D_80151AD0), gets
 * func_800EF5B0(node, D_80120E34, 0) and alpha 0x60 in multi-player, and publishes its
 * size (+0x14/+0x16) to D_8011617C/D_80116180.  Returns the hidden flag / 1.
 * Also matches at -O2.  Whole-program unit: EQUAL.
 * Shaping quirk: D_801543CA is `volatile` (retail reads it through `la; lh 0(reg)`,
 * like the other readers of this word, e.g. func_800EC190's store).  Body otherwise from
 * the heads_B13 structmode draft.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32;

#ifdef TARGET_PC
/* PC: the node is a blit (sound_control.c's Blit, 0x68 bytes, docs/port/POINTERS.md 9.3): its name, image and info
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
#else
typedef struct MenuNode {
    s32 next;           /* 0x00 */
    s16 flags, pad6;    /* 0x04 */
    s32 pad8;           /* 0x08 */
    s16 padC;           /* 0x0C */
    s16 x;              /* 0x0E */
    s16 y;              /* 0x10 */
    s16 pad12;          /* 0x12 */
    s16 w;              /* 0x14 */
    s16 h;              /* 0x16 */
    s8 alpha;           /* 0x18 */
    s8 pad19;
    s8 hidden;          /* 0x1A */
    s8 pad1B;
    s32 pad1C, pad20, pad24;
    s32 word28;         /* 0x28 */
    s32 index;          /* 0x2C: player index */
} MenuNode;
#endif

typedef struct { s32 x, y; } Pos;
#ifdef TARGET_PC
/* PC: a car record seen from +0xD8 (a name inside player_array): only its stride, PC_CAR_SIZE, matters here. */
typedef struct { u8 pad[0x17]; s8 state; u8 pad18[PC_CAR_SIZE - 0x18]; } Player;
#else
typedef struct { u8 pad[0x17]; s8 state; u8 pad18[0x3B8 - 0x18]; } Player;  /* 0x3B8 */
#endif

extern s16 gNumWindows;                  /* player count */
extern volatile s16 D_801543CA;
extern s8 D_80156CE8;
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Player D_801528F0[];
#define PC_CAR_FINISHED(i) D_801528F0[i].state
#endif
extern s32 state_word_a, D_8011617C, D_80116180;
extern Pos D_80116028[][4];     /* row = player count - 1 */
extern char D_80120E34[];
extern void Input_ApplyPadConfig(void *);
extern void func_800EF5B0(void *, char *, s32);

s32 func_80108DA8(MenuNode *node) {
    s32 index;
    s32 hidden;

    index = node->index;
    if (index >= gNumWindows || (state_word_a & 8) || D_801543CA < 2) {
        node->word28 = 0;
        if (node->hidden != 1) {
            node->hidden = 1;
            Input_ApplyPadConfig(node);
        }
        return node->hidden;
    }
    hidden = D_80156CE8 == 0;
    if (hidden == 0) {
        hidden = PC_CAR_FINISHED(index) == 1;
    }
    if (hidden != node->hidden) {
        node->hidden = hidden;
        Input_ApplyPadConfig(node);
    }
    if (node->hidden != 0) {
        return 1;
    }
    node->x = D_80116028[gNumWindows - 1][index].x;
    node->y = D_80116028[gNumWindows - 1][index].y;
    if (gNumWindows >= 2) {
        func_800EF5B0(node, D_80120E34, 0);
        node->alpha = 0x60;
    }
    Input_ApplyPadConfig(node);
    D_8011617C = node->w;
    D_80116180 = node->h;
    return 1;
}
