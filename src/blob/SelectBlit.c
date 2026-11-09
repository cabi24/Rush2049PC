#include "game/game.h"

typedef struct TexDef {
    char name[16];
    u16 Width, Height;
    u8 other[16];
} TexDef;
typedef struct Blit {
    char *Name;
    void *Image;
    TexDef *Info;
    s16 TexIndex, X, Y;
    u16 State;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right, Color;
} Blit;

void SelectBlit(Blit *blt, int index, int hSize, int vSize)
{
    int row, col, secPerRow;

    if (!blt)
        return;
    if (!blt->Info || !blt->Info->Width) {
        RenameBlit(blt, blt->Name, 1);
        if (!blt->Info)
            return;
    }
    secPerRow = blt->Info->Width / hSize;
    if (!secPerRow)
        secPerRow = 1;
    row = index / secPerRow;
    col = index % secPerRow;
    blt->Top = row * vSize;
    blt->Bot = blt->Top + vSize - 1;
    if (blt->Flip) {
        col = secPerRow - col - 1;
        blt->Left = (blt->Info->Width % hSize) + col * hSize;
        blt->Right = blt->Left + hSize - 1;
    } else {
        blt->Left = col * hSize;
        blt->Right = blt->Left + hSize - 1;
    }
    UpdateBlit(blt);
}
