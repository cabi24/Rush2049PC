/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Exact genuine predicate; all D_zz/zz_caller stand-ins omitted. */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

typedef struct { u8 pad0; u8 row; u8 pad2[0x48 - 0x2]; void *ref; } Rec76;
extern Rec76 gPlayers[];
extern s16 gControlSelect[];
extern s8 gNewControls[][10];
extern s8 gControlValid[];

s32 controls_validControl(s8 car)
{
    s8 *row = gNewControls[gPlayers[car].row];
    s16 idx = gControlSelect[car];

    return row[idx] != 23 && gControlValid[row[idx]] == 1 && (idx != 2 || row[2] == 25 || row[2] == 19) &&
           (idx < 3 || (row[idx] != 19 && row[idx] != 25));
}
/* note: controls_validControl = validControl@controlscr.c (GC 0x8006CDBC): gControlValid[control], the
 * steering slot (2) needs 19/20, other slots refuse them. body via, unchanged. */
