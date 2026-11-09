/* TB_Strlen: the length of a string in characters, single- or double-byte (MAT3 TB_text.c). */
#include "game/game.h"

/* The number of characters in string: bytes for a single-byte string, byte pairs after the TEXT_DOUBLE_BYTE mark for a
 * double-byte one. */
int TB_Strlen(u8 *string)
{
    int count = 0;
    u8 *cursor = string;

    if (*cursor == TEXT_DOUBLE_BYTE) {
        cursor++;
        while (cursor[0] || cursor[1]) {
            cursor += 2;
            count++;
        }
    } else {
        while (*cursor++) {
            count++;
        }
    }
    return count;
}
