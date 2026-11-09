#include "game/game.h"

typedef struct List { u8 *p[22]; } List;
VIEW(List, obscenities);
#define obscenities obscenities_view
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
