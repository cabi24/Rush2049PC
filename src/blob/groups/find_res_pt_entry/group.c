typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)
/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct {u8 present;u8 pad[23];} Entry;
extern Entry *gResPath_alt;
s32 FindResPtRecurse(s32,s32,s32 *,s32 *,s32,s32);
s32 FindResPt(s32 index,s32 value,s32 *out_index,s32 *out_value,s32 flags) {
    if(flags && gResPath_alt[index].present) {
        *out_index=index;
        *out_value=value;
        return 1;
    }
    return FindResPtRecurse(index,value,out_index,out_value,flags,0);
}
