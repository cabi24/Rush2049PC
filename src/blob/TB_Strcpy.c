/* TB_Strcpy: copy a string, single- or double-byte (MAT3 TB_text.c). */
#include "game/game.h"

/* Copies source, a single- or a double-byte string (TEXT_DOUBLE_BYTE, ended by two zeros), to destination and returns
 * destination. */
u8 *TB_Strcpy(u8 *destination,u8 *source) {
    u8 *out,*in; u8 c;
    out=destination; in=source;
    if (*in==TEXT_DOUBLE_BYTE) {
        *out++=*in++;
        while (in[0]!=0 || in[1]!=0) {
            *out++=*in++;
            *out++=*in++;
        }
        *out++=0;
        *out=0;
    } else {
        while ((*out++=*in++)!=0) {}
    }
    return destination;
}
