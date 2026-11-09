/* TB_SetJust: how text is placed against its position (MAT3 TB_text.c). */
#include "game/game.h"

typedef struct { u32 h, v; } Justification;
VIEW(Justification, gJust);
#define gJust gJust_view

/* Sets the justification TB_DrawText and the text boxes use: hJust kTB_JustLeft, kTB_JustCenter or kTB_JustRight
 * against x, vJust kTB_JustTop, kTB_JustCenter or kTB_JustBottom against y. */
void TB_SetJust(u32 hJust,u32 vJust) {Justification *just=&gJust;just->h=hJust;just->v=vJust;}
