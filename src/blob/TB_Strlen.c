#include "game/game.h"

int TB_Strlen(u8 *string)
{
    int count = 0;
    u8 *cursor = string;

    if (*cursor == 0xFF) {
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
