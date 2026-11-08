/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * UpdateListeners (historical label): for each of the gNumPlayers cars, if its 76-byte record
 * gPlayers[i] has a render node (+0x44, (void *) -1 = none), hand the node four Vec3 through
 * AudUpdateListener (which copies them under the audio_lock_queue queue lock): the car model's position
 * (D_8014A250[model].pos, 0x808-byte MODELDAT, +0x22C; model index u8 at record +0), unitmatrix_origin and the two
 * vectors of the car's 152-byte view record gCamera[i] at +0x18 and +0x0C. No arcade ancestor proven.
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

/* PC: an input record is 80 bytes (its pointer at +0x48 is 8 bytes). The node at +0x44 stays 32 bits: it is a
 * packet's window address (a heap block, AudAddListener) or -1 (docs/port/POINTERS.md 10). */
typedef struct { u8 model; u8 pad1[67]; u32 node; void *ref; } CarRec;
#define NO_NODE 0xFFFFFFFFu
#define NODE(n) (void *)PC_N64P(n)
typedef struct { u8 pad0[556]; f32 pos[3]; u8 pad238[2056 - 568]; } ModelDat; /* 0x808 */
typedef struct { u8 pad0[12]; f32 a[3]; f32 b[3]; u8 pad24[152 - 36]; } View;  /* 152 */

extern s16 gNumPlayers;
extern CarRec gPlayers[];
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern f32 unitmatrix_origin[];
extern View gCamera[];

void AudUpdateListener(void *node, f32 *a, f32 *b, f32 *c, f32 *d);

void UpdateListeners(void)
{
    s32 i;
    void *node;

    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].node != NO_NODE) {
            node = NODE(gPlayers[i].node);
            AudUpdateListener(node, PC_MODEL_RWR(gPlayers[i].model), unitmatrix_origin,
                            gCamera[i].b, gCamera[i].a);
        }
    }
}
