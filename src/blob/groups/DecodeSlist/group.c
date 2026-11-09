#include "game/game.h"

void DecodeSlist(u8 *input, s32 count, u16 *front, u32 marker)
{
    u16 *back = front + count - 1;
    u32 value;
    s32 run;

    while (count > 0) {
        value = ((u32)input[0] << 8) + input[1];
        input += 2;
        run = value >> 13;
        value &= 0x1FFF;
        count = count - run - 1;
        do {
            run--;
            if (value == marker) {
                *front++ = value++;
            } else {
                *back-- = value++;
            }
        } while (run >= 0);
    }
}
