#include "game/game.h"

typedef struct Pool { u8 doubly; u8 n64; u8 pad2[2]; s32 count; s32 size; u8 *mem; intptr_t f16; u8 *free; } Pool;
void ClearStatLL(Pool *p) {
    u8 *m = p->mem;
    s32 i;
    p->f16 = 0;
    p->free = m;
    if (m != 0) {
        for (i = 0; i < p->count - 1; i++) {
            pc_statll_set_link(p->n64, m + i * p->size, 0, m + (i + 1) * p->size);
        }
        pc_statll_set_link(p->n64, m + i * p->size, 0, 0);
    }
}
