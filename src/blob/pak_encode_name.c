#include "game/game.h"

VIEW(u16[256], pak_charset);
#define pak_charset pak_charset_view

void pak_encode_name(u8 *output, u8 *input, u8 limit)
{
    int count = 0, index;
    u8 *cursor = input, *destination = output;

    if (*input == 255) {
        cursor++;
        while (cursor[0] != 0 || cursor[1] != 0) {
            unsigned int code = (cursor[0] << 8) | cursor[1];
            cursor += 2;
            for (index = 0; index < 256; index++) {
                if (code == pak_charset[index]) {
                    *destination++ = index;
                    count++;
                    break;
                }
            }
            if (count == limit) break;
        }
    } else {
        while (*cursor != 0) {
            unsigned int code;
            for (index = 0, code = *cursor; index < 256; index++) {
                if (pak_charset[index] - code == 0) {
                    *destination++ = index;
                    count++;
                    break;
                }
            }
            if (count == limit) break;
            cursor++;
        }
    }
    while (count < limit) {
        *destination++ = 0;
        count++;
    }
}
