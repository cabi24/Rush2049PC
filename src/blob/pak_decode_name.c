/* pak_decode_name: a pak file's name in the game's characters (the N64's own; MAT3's memory cards use ASCII). */
#include "game/game.h"

#define PAK_CHAR_SPACE 15     /* the pak character set's space */
#define PAK_CHAR_LAST_ROMAN 65 /* codes past it are kana: the name becomes a double-byte string */

/* Turns the len pak characters at in (a directory entry's name or extension) into a string at out: trailing spaces
 * and zeros dropped, other zeros made spaces, each code through pak_charset; a double-byte string (TEXT_DOUBLE_BYTE)
 * when a code is past the roman letters, else single bytes. */
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
        if (src[i] != 0 && src[i] != PAK_CHAR_SPACE) {
            break;
        }
    }
    len = i + 1;
    for (; i >= 0; i--) {
        if (src[i] == 0) {
            buf[i] = PAK_CHAR_SPACE;
        } else {
            buf[i] = src[i];
            if (buf[i] > PAK_CHAR_LAST_ROMAN) {
                wide = 1;
            }
        }
    }
    if (wide) {
        *dst++ = TEXT_DOUBLE_BYTE;
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
