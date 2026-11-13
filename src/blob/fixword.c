/* fixword (MAT3 censor.c's): the word filter for the names players type: censor.c calls it for each word of its
 * list. */
#include "game/game.h"

/* Looks in the upper-case name wp for the word op, letters in order with anything but letters and digits between
 * them and repeated letters counted once (so "B.A.D", "BAAD"), and overwrites each match with '!'. Recursion goes on
 * from after a match (to find them all) and from the next start after a failed try. Returns 1 when it censored
 * something at this level, else 0. The statics are shared by the recursion: op1 is the word's start for the next
 * try, sp the match's start.
 *
 * end is one past the table the name is a row of (censor's caller). Between two letters of a word the search skips
 * anything that isn't a letter or a digit, the name's terminating NUL included, so a name that ends partway through a
 * word reads on into the rows after it: the next players' names, as the original game does. In the last row it ran
 * on past the table, into whatever data followed (on the N64 the next variables' bytes, here another object). The
 * search now stops at end as if the byte there ended the name and matched no letter: the same result as wherever
 * the original stayed inside the table, and a read inside it where the original ran past. */
s32 fixword(u8 *op, u8 *wp, const u8 *end) {
    static u8 *op1, *sp;
    static s16 cnt;

    op1 = op;
    while (1) {
        if (wp == end || !*wp)
            return 0;
        if (*wp == *op)
            break;
        wp++;
    }

    sp = wp;

    while (wp < end && *wp == *op)
        wp++;

    while (*op && (op[0] == op[1]))
        op++;

    while (1) {
        while (*++op == ' ');

        if (!*op)
            break;

        for (cnt = 0; 1; wp++, cnt--) {
            while (wp < end && !((*wp >= 'A' && *wp <= 'Z') || (*wp >= '0' && *wp <= '9')))
                wp++;

            if (wp < end && *wp == *op)
                break;

            if (wp == end || (!*wp) || !cnt)
                return fixword(op1, sp + 1, end);
        }
        do {
            wp++;
        } while (wp < end && (*wp == *op) && (op[0] != op[1]));
    }

    while (sp < wp)
        *sp++ = '!';

    fixword(op1, wp, end);
    return 1;
}
