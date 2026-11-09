/* TB_SetJust: how text is placed against its position (MAT3 TB_text.c). */
#include "game/game.h"

/* Sets the justification TB_DrawText and the text boxes use: hJust kTB_JustLeft, kTB_JustCenter or kTB_JustRight
 * against x, vJust kTB_JustTop, kTB_JustCenter or kTB_JustBottom against y. */
void TB_SetJust(u32 hJust,u32 vJust) {TextJust *just=&gJust;just->h=hJust;just->v=vJust;}
