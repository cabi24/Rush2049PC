#include "game/game.h"

typedef struct { u8 pad0; u8 row; u8 pad2[0x48 - 0x2]; void *ref; } Rec76;
extern Rec76 gPlayers[];

s32 controls_validControl(s8 car)
{
    s8 *row = gNewControls[gPlayers[car].row];
    s16 idx = gControlSelect[car];

    return row[idx] != 23 && gControlValid[row[idx]] == 1 && (idx != 2 || row[2] == 25 || row[2] == 19) &&
           (idx < 3 || (row[idx] != 19 && row[idx] != 25));
}
/* note: controls_validControl = validControl@controlscr.c (GC 0x8006CDBC): gControlValid[control], the
 * steering slot (2) needs 19/20, other slots refuse them. body via, unchanged. */
