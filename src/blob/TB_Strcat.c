/* TB_Strcat: append a string, single- or double-byte (MAT3 TB_text.c). */
#include "game/game.h"

/* Appends source to destination and returns destination. When either is double-byte (TEXT_DOUBLE_BYTE) so is the
 * result: a single-byte destination is converted in place first (each byte after a zero byte, through a 256-byte
 * copy), and a single-byte source is appended the same way. */
u8 *TB_Strcat(u8 *destination, u8 *source) {
    u8 *ret;
    u8 *out;
    u8 *in;
    u8 buf[256];
    u8 *p;

    ret = destination;
    out = destination;
    in = source;
    if (*destination == TEXT_DOUBLE_BYTE || *source == TEXT_DOUBLE_BYTE) {
        if (*destination == TEXT_DOUBLE_BYTE) {
            out++;
            while (out[0] != 0 || out[1] != 0) {
                out += 2;
            }
        } else {
            p = buf;
            while ((*p++ = *ret++) != 0) {}
            ret = destination;
            *destination++ = TEXT_DOUBLE_BYTE;
            out = destination;
            p = buf;
            while (*p != 0) {
                *out++ = 0;
                *out++ = *p++;
            }
        }
        if (*source == TEXT_DOUBLE_BYTE) {
            in++;
            while (in[0] != 0 || in[1] != 0) {
                *out++ = *in++;
                *out++ = *in++;
            }
        } else {
            while (*in) {
                *out++ = 0;
                *out++ = *in++;
            }
        }
        *out++ = 0;
        *out = 0;
    } else {
        while (*out) {
            out++;
        }
        while ((*out++ = *in++) != 0) {}
    }
    return ret;
}
