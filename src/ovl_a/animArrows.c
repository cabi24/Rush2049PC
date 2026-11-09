#include "game/game.h"

typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State;
    s16 Width, Height; u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID;
    uintptr_t reserved30; u16 Texture;
} Blit;
typedef struct Point { s16 x, y; } Point;
typedef struct ResourceIndices { u16 unused, first; } ResourceIndices;
typedef struct ResourceTables {
    u16 count0; u32 *ptrs0; u16 count1, count2;
    ResourceIndices *indices; char **strings;
} ResourceTables;
/* PC: the sprite's host layout (particle_knot's Sprite), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Texture { void *pal, *tex; u16 model; s16 x, y; u16 mode; s16 w, h; u8 color; u8 flags; s8 hidden; u8 pad17; s16 t0, s0, t1, s1; } Texture;

VIEW(Point[][4], trackscr_arrowPos);
#define trackscr_arrowPos trackscr_arrowPos_view
VIEW(ResourceTables, gText);
#define gText gText_view
VIEW(Texture[], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

s32 animArrows(Blit *blt)
{
    s16 pulse = blt->AnimID & 15;
    s16 item = (blt->AnimID >> 4) & 15;
    s16 group = (blt->AnimID >> 8) & 15;
    s16 texture = (blt->AnimID >> 12) & 15;
    s16 active;
    if (Hidden(blt, trackscr_buttonsInited == 1)) return 1;
    if (group == 1) {
        if (Hidden(blt, item == 2)) return 1;
    }
    if (group == 0) {
        if (item == 2) {
            if (Hidden(blt, trackscr_gFirstOption == 0)) return 1;
        } else if (item == 3) {
            if (Hidden(blt, trackscr_gNumOptions - trackscr_gFirstOption < 5)) return 1;
        }
    }
    active = trackscr_gActiveOptionEnabled || item >= 2;
    RenameBlit(blt, trackscr_arrowNames[texture][active], 1);
    if (pulse) blt->Alpha = (u32)(TB_GetFlashTextCycle() * 255.0f);
    blt->X = trackscr_arrowPos[group][item].x - blt->Width / 2;
    blt->Y = trackscr_arrowPos[group][item].y - blt->Height / 2;
    if (group == 0) {
        HAL_SetFont(11);
        if (item == 0) {
            blt->X -= HAL_GetTextWidth(gText.strings[trackscr_gOpts[trackscr_gActiveOption] + gText.indices->first], -1);
        }
        if (item == 0 || item == 1) blt->Y += (trackscr_gActiveOption - trackscr_gFirstOption) * 13;
    }
    if (item == 1) blt->Flip = 1;
    else if (item == 3) gBlitInstanceList[blt->Texture].flags |= 8;
    UpdateBlit(blt);
    return 1;
}
