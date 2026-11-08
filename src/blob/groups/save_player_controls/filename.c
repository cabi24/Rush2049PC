typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16; typedef signed int s32; typedef unsigned int u32;
typedef struct Y { u8 pad[0x38]; s32 f38; u8 name[11]; } Y;
typedef struct X { void *next, *prev; struct C96Handle *f8; u8 p1[0x15]; u8 name[11]; Y **f2C; } X;   /* the profile, pc_prelude.h */
s32 game_memcmp(u8 *a, u8 *b, s32 n);
extern void *memcpy(void *, void *, unsigned);
u32 CalcChecksumInternal(u8 *, u32);
struct C96Handle;
void PS_WriteFile(struct C96Handle *, u8 *, u32);
void SavePlayerControls(X **arg0, u8 *arg1)
{
Y *y;
 if ((*arg0)->f2C != 0) {
 y = *(*arg0)->f2C;
 if (game_memcmp(y->name, arg1, 11) != 0) {
 memcpy((*arg0)->name, arg1, 11);
 memcpy(y->name, arg1, 11);
 y->f38 = CalcChecksumInternal(y->name, 11);
 PS_WriteFile((*arg0)->f8, (u8 *) &y->f38, 15);
 }
 }
}