/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
extern void *gEnvList;
void ReleaseEnviron(void *, s32);
void InitSkids(void);
void RemoveEnvirons(void) {
    void *p;
    while ((p = gEnvList) != 0) {
        ReleaseEnviron(p, 1);
    }
    InitSkids();
}
