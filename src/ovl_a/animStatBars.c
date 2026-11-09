#include "game/game.h"

typedef struct Blit {
    char *Name;
    void *Image;
    void *Info;
    s16 TexIndex, X, Y;
    u16 State;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right, Color;
    u16 reserved26;
    s32 (*AnimFunc)(struct Blit *);
    u32 AnimID;
} Blit;

s32 animStatBars(Blit *blt)
{
    s32 segment = blt->AnimID & 15;
    s32 bar = (blt->AnimID >> 4) & 15;
    s32 player = (blt->AnimID >> 8) & 15;
    s32 height;
    s32 initialized = blt->AnimID >> 31;
    if (!initialized) {
        if (gNumPlayers >= 3 && player < 2)
            RenameBlit(blt, carscr_stat_bars_texture_name, 0);
        blt->AnimID |= 0x80000000;
        if (player < gNumPlayers && gNumPlayers == 1 && gRace != 2) {
            blt->X = 10;
            height = blt->Height / 8;
            blt->Y = 48 + bar * 2 * height;
        } else {
            Hidden(blt, 1);
            blt->AnimFunc = 0;
            return 1;
        }
        blt->Left = 0;
        blt->Top = (bar * 2 + (segment == 0)) * height;
        blt->Bot = blt->Top + height - 1;
    }
    if (Hidden(blt, gCarLocked[player] == 1))
        return 1;
    if (segment == 1) {
        blt->Right = gStats[player][bar] * blt->Width - 1;
        blt->Right = blt->Right < 2 ? 2 : blt->Right;
    } else
        blt->Right = blt->Width - 1;
    UpdateBlit(blt);
    return 1;
}
