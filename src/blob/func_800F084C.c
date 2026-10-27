/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef signed int s32;
#ifndef TARGET_PC
typedef unsigned int uintptr_t;
typedef s32 intptr_t;
#endif
typedef struct { u8 *p[22]; } List;
extern List D_801149B4;
u8 func_800F0674(u8 *a, s32 b);
u8 func_800F084C(intptr_t arg) {
    u8 r;
    u8 **p;
    List l;
    l = D_801149B4;
    r = 0;
    p = l.p;
    for (; **p != 0; p++) {
        r |= func_800F0674(*p, arg);
    }
    return r;
}
