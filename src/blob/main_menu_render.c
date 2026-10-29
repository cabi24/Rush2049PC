/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * main_menu_render (historical label): for each of the D_8014A108 cars, if its 76-byte record
 * D_8014A118[i] has a render node (+0x44, (void *) -1 = none), hand the node four Vec3 through
 * main_menu_input (which copies them under the D_80142728 queue lock): the car model's position
 * (D_8014A250[model].pos, 0x808-byte MODELDAT, +0x22C; model index u8 at record +0), D_801141B0 and the two
 * vectors of the car's 152-byte view record D_80150B70[i] at +0x18 and +0x0C. No arcade ancestor proven.
 * Shaping: `node` is assigned inside the `if` after the test (lever): tested as the field, the value is
 * loaded into v1 and moved to a0; a node local assigned before the test gets a0 directly. Also -O2.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

#ifdef TARGET_PC
/* PC: an input record is 80 bytes (its pointer at +0x48 is 8 bytes). The node at +0x44 stays 32 bits: it is a
 * packet's window address (a heap block, func_800D63EC) or -1 (docs/port/POINTERS.md 10). */
typedef struct { u8 model; u8 pad1[67]; u32 node; void *ref; } CarRec;
#define NO_NODE 0xFFFFFFFFu
#define NODE(n) (void *)(uintptr_t)n
#else
typedef struct { u8 model; u8 pad1[67]; void *node; u8 pad48[4]; } CarRec;   /* 76 */
#define NO_NODE (void *) -1
#define NODE(n) n
#endif
typedef struct { u8 pad0[556]; f32 pos[3]; u8 pad238[2056 - 568]; } ModelDat; /* 0x808 */
typedef struct { u8 pad0[12]; f32 a[3]; f32 b[3]; u8 pad24[152 - 36]; } View;  /* 152 */

extern s16 D_8014A108;
extern CarRec D_8014A118[];
#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern ModelDat D_8014A250[];
#define PC_MODEL_RWR(i) D_8014A250[i].pos
#endif
extern f32 D_801141B0[];
extern View gCamera[];

void main_menu_input(void *node, f32 *a, f32 *b, f32 *c, f32 *d);

void main_menu_render(void)
{
    s32 i;
    void *node;

    for (i = 0; i < D_8014A108; i++) {
        if (D_8014A118[i].node != NO_NODE) {
            node = NODE(D_8014A118[i].node);
            main_menu_input(node, PC_MODEL_RWR(D_8014A118[i].model), D_801141B0,
                            gCamera[i].b, gCamera[i].a);
        }
    }
}
