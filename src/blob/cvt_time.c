#include "game/game.h"

s32 cvt_time(f32 value, u8 *digits, u8 format) {
    u32 integer, minutes, seconds, hundredths;
    if (value < 0.0f) value = 0.0f;
    switch (format) {
    case 'c':
        integer = (u32)value;
        if (integer >= 1000) integer = 999;
        digits[0] = (integer / 100) % 10;
        digits[1] = (integer / 10) % 10;
        digits[2] = integer % 10;
        digits[3] = 0;
        return 3;
    case 'f':
        integer = (u32)value;
        minutes = integer / 60;
        seconds = integer % 60;
        digits[0] = (minutes / 10) % 10;
        digits[1] = minutes % 10;
        digits[2] = seconds / 10;
        digits[3] = seconds % 10;
        hundredths = (u32)(value * 100.0f) % 100;
        digits[4] = hundredths / 10;
        digits[5] = hundredths % 10;
        digits[6] = 0;
        return 6;
    }
    return 0;
}
