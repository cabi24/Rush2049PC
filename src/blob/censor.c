/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef signed int s32;
typedef struct { u8 *p[22]; } List;
extern List obscenities;
u8 fixword(u8 *a, intptr_t b);
u8 censor(intptr_t arg) {
    u8 r;
    u8 **p;
    List l;
    l = obscenities;
    r = 0;
    p = l.p;
    for (; **p != 0; p++) {
        r |= fixword(*p, arg);
    }
    return r;
}
