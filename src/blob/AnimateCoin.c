#include "game/game.h"

typedef f32 Mat3[3][3];
typedef struct Coin {
    s32 model;          /* 0 */
    Mat3 matrix;        /* 4 */
    f32 position[3];    /* 40 */
    u8 pad52[8];
    f32 spin[3];        /* 60 */
    u16 missing;        /* 72 */
    s16 number;         /* 74 */
} Coin;
/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first
 * on a little-endian host (docs/port/ENDIAN.md). */
typedef union Id { s32 handle; struct { s16 index, high; } part; } Id;
/* PC: node is a visual (tVisual, 0x20 bytes: next and func are host pointers), and its data the target, a
 * level-object node, which keeps its N64 layout: both as 32-bit window addresses, as is the target's state, the
 * coin (a heap record of its category) (docs/port/CARS.md, POINTERS.md 10). The low half of the coin's s32
 * model handle comes first. */
enum n64p_Coin { n64p_Coin_ };
enum n64p_Entity { n64p_Entity_ };
typedef struct Entity {
    u8 pad0[4];
    u8 flags;           /* 4 */
    u8 pad5[7];
    Id id;              /* 12 */
    u8 pad16[40];
    f32 position[3];    /* 56 */
    u8 pad68[40];
    enum n64p_Coin coin; /* 108 */
} Entity;
typedef struct EntityHdr { s16 handle; u8 pad2[2]; } EntityHdr;
typedef struct Node { void *next; u8 pad8[8]; enum n64p_Entity entity; } Node;
#define N64P(x) _Generic((x), enum n64p_Coin: (Coin *)PC_N64P(x), enum n64p_Entity: (Entity *)PC_N64P(x), \
    default: (x))

typedef struct Player { u8 pad0[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad0_308[0x35C - 0x308]; s8 slot; u8 pad861[0x380 - 0x35D]; void *input; u8 pad861_384[0x3B8 - 0x384]; } Player;
/* PC: a player's view bit; a slot of -1 shifts by a negative count, undefined in C. The N64's sllv, x86-64's shl and
 * aarch64's lsl all take the count's low 5 bits, so this does (docs/port/SANITIZE.md). */
#define SLOT_BIT(s) 1 << ((s) & 31)

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(Mat3, unitmatrix);
#define unitmatrix unitmatrix_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(s32, gTargetList);
#define gTargetList gTargetList_view

extern s32 ZOID_AddObject(s32, void *, s16, s32);

void AnimateCoin(Node *node, s16 update)
{
    Entity *entity;
    Coin *coin;
    f32 spin[3];
    s32 number, i, alone;
    Player *player;
    u32 gold[1], silver[1];
    s32 pad[29];
    f32 *rate;
    if (!update) {
        ReleaseEnviron(node, 1);
        return;
    }
    if (gPauseState) return;
    entity = N64P(node->entity);
    coin = N64P(entity->coin);
    if (entity->flags & 0x80) {
        RemoveObjectTree(((EntityHdr *)coin)->handle, 0, 0);
        RemoveObjectTree(entity->id.part.index, 0, 0);
        FreeStatLLE(&gTargetList, entity);
        ReleaseEnviron(node, 1);
        return;
    }
    if (!(entity->flags & 1)) {
        gold[0] = PC_RGBA(coin_gold_glow_color);
        silver[0] = PC_RGBA(coin_silver_glow_color);
        if (entity->flags & 0x40) {
            number = gWhichGoldCoin + 8;
            coin->number = number;
            gWhichGoldCoin++;
        } else {
            number = gWhichSilverCoin;
            coin->number = number;
            gWhichSilverCoin++;
        }
        ZOID_SetObjectDef(entity->id.part.index, MBOX_FindObject_WORK(coin_glow_object_name, 0, gNumModels - 1, 1));
        coin->spin[0] = 0.0f;
        coin->spin[2] = 0.0f;
        coin->spin[1] = 3.0f;
        CopyUV(unitmatrix, coin->matrix);
        coin->position[0] = entity->position[0];
        coin->position[1] = entity->position[1];
        coin->position[2] = entity->position[2];
        if (entity->flags & 0x40) {
            coin->model = ZOID_AddObject(MBOX_FindObject_WORK(gold_coin_object_name, 0, gNumModels - 1, 1), coin->matrix, -1, 0x40000);
            ZOID_SetColor(entity->id.part.index, gold);
        } else {
            coin->model = ZOID_AddObject(MBOX_FindObject_WORK(silver_coin_object_name, 0, gNumModels - 1, 1), coin->matrix, -1, 0x40000);
            ZOID_SetColor(entity->id.part.index, silver);
        }
        coin->missing = 0;
        alone = 1;
        for (i = 0; i < gNumPlayers; i++) {
            if (!IsCoinFound(i, number)) {
                alone = 0;
                coin->missing |= 1 << i;
            } else {
                player = PC_CAR(i);
                MBOX_HideObject(entity->id.handle, 1, SLOT_BIT(player->slot));
                MBOX_HideObject(coin->model, 1, SLOT_BIT(player->slot));
            }
        }
        if (alone) {
            RemoveObjectTree(((EntityHdr *)coin)->handle, 0, 0);
            RemoveObjectTree(entity->id.part.index, 0, 0);
            FreeStatLLE(&gTargetList, entity);
            ReleaseEnviron(node, 1);
            return;
        }
        entity->flags |= 1;
    }
    rate = N64P(entity->coin)->spin;
    spin[0] = rate[0] * gFrameSeconds;
    spin[1] = rate[1] * gFrameSeconds;
    spin[2] = rate[2] * gFrameSeconds;
    RotateUV(spin, coin->matrix);
    if (entity) {}
}
