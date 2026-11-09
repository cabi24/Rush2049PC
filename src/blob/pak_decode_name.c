#include "game/game.h"

void pak_decode_name(u8 *out, u8 *in, u8 len) {
    s32 i;
    s32 wide;
    u8 buf[64];
    u8 *dst;
    u8 *src;

    dst = out;
    src = in;
    wide = 0;
    for (i = len - 1; i >= 0; i--) {
        if (src[i] != 0 && src[i] != 15) {
            break;
        }
    }
    len = i + 1;
    for (; i >= 0; i--) {
        if (src[i] == 0) {
            buf[i] = 15;
        } else {
            buf[i] = src[i];
            if (buf[i] > 65) {
                wide = 1;
            }
        }
    }
    if (wide) {
        *dst++ = 255;
        for (i = 0; i < len; i++) {
            *dst++ = pak_charset[buf[i]] >> 8;
            *dst++ = pak_charset[buf[i]] & 0xFF;
        }
        *dst++ = 0;
        *dst = 0;
    } else {
        for (i = 0; i < len; i++) {
            dst[i] = pak_charset[buf[i]];
        }
        dst[i] = 0;
    }
}
