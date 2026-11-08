/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;typedef unsigned char u8;typedef signed short s16;typedef unsigned short u16;typedef int s32;typedef unsigned int u32;typedef float f32;
extern s8 gBattlePoints[][4];
void RecordBattlePoints(s8 row,s8 column,s8 delta) {
 gBattlePoints[row][column]+=delta;
}
