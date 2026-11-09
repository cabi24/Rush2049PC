/* fixword (MAT3 censor.c's): the word filter for the names players type: censor.c calls it for each word of its
 * list. */
#include "game/game.h"

/* Looks in the upper-case name wp for the word op, letters in order with anything but letters and digits between
 * them and repeated letters counted once (so "B.A.D", "BAAD"), and overwrites each match with '!'. Recursion goes on
 * from after a match (to find them all) and from the next start after a failed try. Returns 1 when it censored
 * something at this level, else 0. The statics are shared by the recursion: op1 is the word's start for the next
 * try, sp the match's start. */
s32 fixword(u8 *op, u8 *wp) {
    static u8 *op1, *sp;
    static s16 cnt;

    op1 = op;
    while (1) {
        if (!*wp)
            return 0;
        if (*wp == *op)
            break;
        wp++;
    }

    sp = wp;

    while (*wp == *op)
        wp++;

    while (*op && (op[0] == op[1]))
        op++;

    while (1) {
        while (*++op == ' ');

        if (!*op)
            break;

        for (cnt = 0; 1; wp++, cnt--) {
            while (!((*wp >= 'A' && *wp <= 'Z') || (*wp >= '0' && *wp <= '9')))
                wp++;

            if (*wp == *op)
                break;

            if ((!*wp) || !cnt)
                return fixword(op1, sp + 1);
        }
        do {
            wp++;
        } while ((*wp == *op) && (op[0] != op[1]));
    }

    while (sp < wp)
        *sp++ = '!';

    fixword(op1, wp);
    return 1;
}
