#include "game/game.h"

typedef struct Bank {
    /* 0x0 */ u8 pad0;
    /* 0x1 */ u8 mode;
    /* 0x2 */ u8 pad2[2];
    /* 0x4 */ u8 f4;
    /* 0x5 */ u8 pad5;
    /* 0x6 */ u8 f6;
    /* 0x7 */ u8 f7;
    /* 0x8 */ u8 f8;
    /* 0x9 */ s8 f9;
    /* 0xA */ u8 f10;
    /* 0xB */ u8 f11;
    /* 0xC */ u8 f12;
} Bank;

s16 TB_GetFontHeight(void) { u8 first; first=HAL_GetFontAscent(); return (s16)(first + HAL_GetFontDescent()); }
s16 TB_GetFontLineHeight(void) {
 u8 first;
 SetupFontData(0);
 first = *((u8 *)gFontHeader + 3);
 SetupFontData(0);
 return (s16)(gLeading + *((u8 *)gFontHeader + 2) + first);
}
